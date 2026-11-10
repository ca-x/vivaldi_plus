#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "../src/web_http.h"

#include <cstdlib>
#include <iostream>

namespace {

void Require(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

bool Parse(const std::string& head)
{
    web_http::Request request;
    size_t length = 0;
    return web_http::ParseHead(head, request, length);
}

web_http::Response Echo(const web_http::Request& request)
{
    return {200, "text/plain", request.method + " " + request.path + ":" + request.body};
}

std::string Exchange(unsigned short port, const std::string& request)
{
    SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Require(socket != INVALID_SOCKET, "socket creation failed");
    DWORD timeout = 4000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    Require(connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "loopback connect failed");
    size_t offset = 0;
    while (offset < request.size()) {
        int count = send(socket, request.data() + offset, static_cast<int>(request.size() - offset), 0);
        Require(count > 0, "socket send failed");
        offset += static_cast<size_t>(count);
    }
    std::string response;
    char buffer[4096];
    int count;
    while ((count = recv(socket, buffer, sizeof(buffer), 0)) > 0) response.append(buffer, count);
    closesocket(socket);
    Require(count == 0, "socket response timeout/failure");
    return response;
}

} // namespace

int main()
{
    web_http::Request request;
    size_t length = 0;
    Require(web_http::ParseHead("PUT /api/config HTTP/1.1\r\nHost: 127.0.0.1:16877\r\nContent-Length: 6\r\nX-Vivaldi-Plus-Token: abc", request, length), "valid PUT rejected");
    Require(length == 6 && request.method == "PUT" && request.headers.at("x-vivaldi-plus-token") == "abc", "PUT headers incorrectly parsed");
    Require(Parse("GET /settings.html?version=1 HTTP/1.1\r\nHost: 127.0.0.1:16877"), "valid GET rejected");
    Require(!Parse("GET / HTTP/1.1"), "missing Host accepted");
    Require(!Parse("GET / HTTP/1.1\r\nHost: a\r\nhost: b"), "duplicate Host accepted");
    Require(!Parse("PUT / HTTP/1.1\r\nHost: a\r\nContent-Length: 1\r\ncontent-length: 1"), "duplicate Content-Length accepted");
    Require(!Parse("PUT / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked"), "chunked request accepted");
    Require(!Parse("PUT / HTTP/1.1\r\nHost: a\r\nContent-Length: 65537"), "oversized body accepted");
    Require(!Parse("PUT / HTTP/1.1\r\nHost: a\r\nContent-Length: -1"), "negative Content-Length accepted");
    Require(!Parse("PUT / HTTP/1.1\r\nHost: a\r\nContent-Length: 12x"), "malformed Content-Length accepted");
    Require(!Parse("PUT / HTTP/1.1\r\nHost: a"), "PUT without length accepted");
    Require(!Parse("GET / HTTP/1.1\r\nHost : a"), "invalid header name accepted");
    Require(!Parse("GET / HTTP/1.0\r\nHost: a"), "unsupported protocol accepted");
    unsigned short port = 0;
    Require(web_http::Start(Echo, port), "could not start loopback test server");
    auto response = Exchange(port, "GET /settings.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    Require(response.find("HTTP/1.1 200") == 0 && response.ends_with("GET /settings.html:"), "GET response incorrect");
    Require(response.find("X-Frame-Options: DENY") != std::string::npos && response.find("frame-ancestors 'none'") != std::string::npos, "security response headers missing");
    response = Exchange(port, "PUT /api/config HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 6\r\n\r\nconfig");
    Require(response.find("HTTP/1.1 200") == 0 && response.ends_with("PUT /api/config:config"), "PUT body did not reach handler");
    response = Exchange(port, "GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n");
    Require(response.find("HTTP/1.1 400") == 0, "duplicate Host not rejected over socket");
    std::cout << "HTTP parser and real loopback request/response tests passed\n";
    return 0;
}
