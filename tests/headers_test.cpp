#include "headers.h"
#include <gtest/gtest.h>

TEST(iterHeaders, Empty) {
    std::string_view req("");
    int call_count = 0;
    iterHeaders(req, [&](std::string_view, std::string_view) { call_count++; });
    EXPECT_EQ(call_count, 0);
}

TEST(iterHeaders, SkipRequestLine) {
    std::string_view req("GET /index.html HTTP/1.1\r\nHost: example.com\r\n");
    int call_count = 0;
    iterHeaders(req, [&](std::string_view, std::string_view) { call_count++; });
    EXPECT_EQ(call_count, 1);
}

TEST(iterHeaders, SingleHeader) {
    std::string_view req("Host: example.com\r\n");
    std::vector<std::pair<std::string, std::string>> headers;
    iterHeaders(req, [&](std::string_view name, std::string_view value) { headers.emplace_back(name, value); });
    EXPECT_EQ(headers.size(), 1);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "example.com");
}

TEST(iterHeaders, MultipleHeaders) {
    std::string_view req("GET /index.html HTTP/1.1\r\nHost: example.com\r\nAccept-Encoding: gzip\r\n");
    std::vector<std::pair<std::string, std::string>> headers;
    iterHeaders(req, [&](std::string_view name, std::string_view value) { headers.emplace_back(name, value); });
    EXPECT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "example.com");
    EXPECT_EQ(headers[1].first, "Accept-Encoding");
    EXPECT_EQ(headers[1].second, "gzip");
}

TEST(iterHeaders, MultipleSameHeaders) {
    std::string_view req("GET /index.html HTTP/1.1\r\nHost: example.com\r\nHost: example.com\r\n");
    int call_count = 0;
    iterHeaders(req, [&](std::string_view, std::string_view) { call_count++; });
    EXPECT_EQ(call_count, 2);
}

TEST(findHostPort, Simple) {
    std::string_view req("GET /index.html HTTP/1.1\r\nHost: example.com:12345\r\n");
    auto res = findHostPort(req);
    EXPECT_EQ(res.first, "example.com");
    EXPECT_EQ(res.second, "12345");
}

TEST(findHostPort, NoHost) {
    std::string_view req("GET /index.html HTTP/1.1\r\nAccept-Encoding: gzip\r\n");
    auto res = findHostPort(req);
    EXPECT_TRUE(res.first.empty());
    EXPECT_TRUE(res.second.empty());
}

TEST(findHostPort, NoPort) {
    std::string_view req("GET /index.html HTTP/1.1\r\nHost: example.com\r\nAccept-Encoding: gzip\r\n");
    auto res = findHostPort(req);
    EXPECT_EQ(res.first, "example.com");
    EXPECT_EQ(res.second, "80");
}

TEST(findContentLength, Simple) {
    std::string_view req(
        "GET /index.html HTTP/1.1\r\nHost: example.com:12345\r\nAccept-Encoding: gzip\r\nContent-Length: 5678\r\n");
    auto res = findContentLength(req);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value(), 5678);
}

TEST(findContentLength, NoContentLength) {
    std::string_view req("GET /index.html HTTP/1.1\r\nHost: example.com:12345\r\nAccept-Encoding: gzip\r\n");
    auto res = findContentLength(req);
    EXPECT_FALSE(res.has_value());
}

TEST(findContentLength, InvalidLength) {
    std::string_view req(
        "GET /index.html HTTP/1.1\r\nHost: example.com:12345\r\nContent-Length: abc\r\nAccept-Encoding: gzip\r\n");
    auto res = findContentLength(req);
    EXPECT_FALSE(res.has_value());
}
