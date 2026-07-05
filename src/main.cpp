#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_at.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/asio/write.hpp>
#include <exception>
#include <iostream>
#include <print>
#include <stdexcept>
#include <string_view>

using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::buffer;
using boost::asio::co_spawn;
using boost::asio::dynamic_buffer;
using boost::asio::io_service;
using boost::asio::transfer_at_least;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
using boost::system::error_code;

constexpr std::string_view delimiter = "\r\n\r\n";

struct HttpPack {
    std::string raw;
    std::string_view headers;
    std::string body;
    size_t need_read;
};

void split_headers(HttpPack &req) {
    size_t n = req.raw.find(delimiter);
    req.headers = std::string_view(req.raw.data(), n);
    std::string_view already_read = std::string_view(req.raw).substr(n + 4);
    req.body.assign(already_read.begin(), already_read.end());

    auto expected = findContentLength(req.headers);
    size_t content_len = expected.value_or(0);
    if (req.body.size() < content_len) {
        req.body.resize(content_len);
    }
    req.need_read = req.body.size() - already_read.size();
}

awaitable<void> session(tcp::socket client_socket, io_service &io_service) {
    try {
        HttpPack req;
        co_await async_read_until(client_socket, dynamic_buffer(req.raw), delimiter, use_awaitable);

        split_headers(req);

        auto address = findHostPort(req.headers);
        if (address.first.empty())
            throw std::runtime_error("HOST header is empty");

        tcp::resolver resolver(io_service);
        auto endpoints = co_await resolver.async_resolve(address.first, address.second, use_awaitable);

        tcp::socket server_socket(io_service);
        co_await boost::asio::async_connect(server_socket, endpoints, use_awaitable);

        if (req.need_read > 0)
            co_await boost::asio::async_read(client_socket,
                                             buffer(req.body.data() + (req.body.size() - req.need_read), req.need_read),
                                             use_awaitable);

        co_await boost::asio::async_write(server_socket, buffer(req.headers), use_awaitable);
        co_await boost::asio::async_write(server_socket, buffer(delimiter), use_awaitable);
        co_await boost::asio::async_write(server_socket, buffer(req.body), use_awaitable);

        //----------
        HttpPack resp;
        co_await async_read_until(server_socket, dynamic_buffer(resp.raw), delimiter, use_awaitable);

        split_headers(resp);

        if (resp.need_read > 0)
            co_await boost::asio::async_read(
                server_socket, buffer(resp.body.data() + (resp.body.size() - resp.need_read), resp.need_read),
                use_awaitable);

        co_await boost::asio::async_write(client_socket, buffer(resp.headers), use_awaitable);
        co_await boost::asio::async_write(client_socket, buffer(delimiter), use_awaitable);
        co_await boost::asio::async_write(client_socket, buffer(resp.body), use_awaitable);

    } catch (const std::exception &e) {
        std::println("error: {}", e.what());
    } catch (...) {
        std::println("unknown error");
    }

    boost::system::error_code ec;
    client_socket.shutdown(tcp::socket::shutdown_both, ec);
    client_socket.close(ec);
    // server_socket освободился сам
}

class Server {
public:
    Server(io_service &io_service, short port)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), socket_(io_service) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(socket_, [this](error_code ec) {
            if (!ec) {
                co_spawn(io_service_, session(std::move(socket_), io_service_), boost::asio::detached);
            }
            socket_ = tcp::socket(io_service_);
            do_accept();
        });
    }

    io_service &io_service_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: proxy_server";
            std::cerr << " <listen_port>\n";
            return 1;
        }
        io_service io_service(1);
        Server server(io_service, std::atoi(argv[1]));
        io_service.run();

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
