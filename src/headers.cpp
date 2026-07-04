#include "headers.h"

#include <charconv>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

using namespace std::string_view_literals;

using Callback = std::function<void(std::string_view, std::string_view)>;

void iterHeaders(std::string_view req, Callback &&callback) {
    for (const auto &header : std::views::split(req, "\r\n"sv)) {
        auto parts = header | std::ranges::views::split(": "sv) | std::ranges::views::transform([](auto &&rng) {
                         return std::string_view(&*rng.begin(), std::ranges::distance(rng));
                     }) |
                     std::ranges::to<std::vector<std::string_view>>();
        if (parts.size() != 2)
            continue;

        callback(parts[0], parts[1]);
    }
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    std::string host;
    std::string port;

    iterHeaders(req, [&](std::string_view name, std::string_view value) {
        if (name == "Host") {
            auto parts = value | std::ranges::views::split(':') | std::ranges::views::transform([](auto &&rng) {
                             return std::string_view(&*rng.begin(), std::ranges::distance(rng));
                         }) |
                         std::ranges::to<std::vector<std::string_view>>();
            if (parts.empty())
                return;

            host = parts[0];
            if (parts.size() > 1)
                port = parts[1];
            else
                port = "80";
        }
    });
    return std::make_pair(host, port);
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    std::optional<size_t> result = std::nullopt;
    iterHeaders(rsp, [&](std::string_view name, std::string_view value) {
        if (name == "Content-Length") {
            size_t len = 0;
            if (std::from_chars(value.begin(), value.end(), len).ec == std::errc())
                result = len;
        }
    });
    return result;
}
