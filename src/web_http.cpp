#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "web_http.h"
#include "web_peer.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <memory>

namespace web_http {
namespace {

constexpr size_t kMaxHead = 16384;
struct Server { SOCKET socket; Handler handler; };

std::string Trim(std::string value)
{
    const auto begin = value.find_first_not_of(" \t");
    if (begin == std::string::npos) return {};
    const auto end = value.find_last_not_of(" \t");
    return value.substr(begin, end - begin + 1);
}

void Send(SOCKET socket, const Response& response)
{
    const std::string status = std::to_string(response.status);
    const std::string headers = "HTTP/1.1 " + status + " VivaldiPlus\r\nContent-Type: " + response.type
        + "\r\nContent-Length: " + std::to_string(response.body.size())
        + "\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff"
          "\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer"
          "\r\nContent-Security-Policy: default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'; form-action 'none'\r\n\r\n";
    for (const auto* part : {&headers, &response.body}) {
        size_t offset = 0;
        while (offset < part->size()) {
            int sent = send(socket, part->data() + offset, static_cast<int>(part->size() - offset), 0);
            if (sent <= 0) return;
            offset += static_cast<size_t>(sent);
        }
    }
}

void Serve(SOCKET socket, Handler handler)
{
    const ULONGLONG deadline = GetTickCount64() + 5000;
    const DWORD timeout = 2000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    std::string bytes;
    char buffer[4096];
    size_t separator = std::string::npos;
    while ((separator = bytes.find("\r\n\r\n")) == std::string::npos) {
        if (GetTickCount64() >= deadline) return;
        int length = recv(socket, buffer, sizeof(buffer), 0);
        if (length <= 0) return;
        bytes.append(buffer, length);
        if (bytes.size() > kMaxHead) { Send(socket, {413, "text/plain", "Request headers too large"}); return; }
    }
    Request request;
    size_t bodyLength = 0;
    if (!ParseHead(bytes.substr(0, separator), request, bodyLength)) {
        Send(socket, {400, "text/plain", "Invalid request"}); return;
    }
    const size_t bodyStart = separator + 4;
    while (bytes.size() - bodyStart < bodyLength) {
        if (GetTickCount64() >= deadline) return;
        int length = recv(socket, buffer, sizeof(buffer), 0);
        if (length <= 0) return;
        bytes.append(buffer, length);
        if (bytes.size() - bodyStart > kMaxBody) { Send(socket, {413, "text/plain", "Request body too large"}); return; }
    }
    if (bytes.size() - bodyStart != bodyLength) { Send(socket, {400, "text/plain", "Unexpected request bytes"}); return; }
    request.body = bytes.substr(bodyStart);
    Send(socket, handler(request));
}

DWORD WINAPI Run(void* context)
{
    std::unique_ptr<Server> server(static_cast<Server*>(context));
    for (;;) {
        sockaddr_in remote = {};
        int length = sizeof(remote);
        SOCKET client = accept(server->socket, reinterpret_cast<sockaddr*>(&remote), &length);
        if (client == INVALID_SOCKET) break;
        if (remote.sin_addr.s_addr == htonl(INADDR_LOOPBACK) && IsSameUserPeer(client)) Serve(client, server->handler);
        closesocket(client);
    }
    closesocket(server->socket);
    WSACleanup();
    return 0;
}

} // namespace

bool ParseHead(const std::string& head, Request& request, size_t& bodyLength)
{
    request = {};
    const auto firstEnd = head.find("\r\n");
    const auto first = head.substr(0, firstEnd);
    const auto firstSpace = first.find(' ');
    const auto secondSpace = first.find(' ', firstSpace == std::string::npos ? 0 : firstSpace + 1);
    if (firstSpace == std::string::npos || secondSpace == std::string::npos || first.substr(secondSpace + 1) != "HTTP/1.1") return false;
    request.method = first.substr(0, firstSpace);
    request.path = first.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    if (request.path.empty() || request.path[0] != '/' || request.path.find_first_of("\r\n\t") != std::string::npos) return false;
    size_t begin = firstEnd == std::string::npos ? head.size() : firstEnd + 2;
    while (begin < head.size()) {
        auto end = head.find("\r\n", begin);
        if (end == std::string::npos) end = head.size();
        auto line = head.substr(begin, end - begin);
        auto colon = line.find(':');
        if (colon == std::string::npos || colon == 0) return false;
        auto name = line.substr(0, colon);
        if (!std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) || c == '-'; })) return false;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        auto value = Trim(line.substr(colon + 1));
        if (value.find_first_of("\r\n\0", 0, 3) != std::string::npos || !request.headers.emplace(name, value).second) return false;
        begin = end + 2;
    }
    if (!request.headers.count("host") || request.headers.count("transfer-encoding")) return false;
    bodyLength = 0;
    const auto found = request.headers.find("content-length");
    if (found != request.headers.end()) {
        const auto& number = found->second;
        const auto result = std::from_chars(number.data(), number.data() + number.size(), bodyLength);
        if (result.ec != std::errc() || result.ptr != number.data() + number.size() || bodyLength > kMaxBody) return false;
    }
    return request.method != "PUT" || found != request.headers.end();
}

bool Start(Handler handler, unsigned short& port)
{
    if (port != 0 && (port < 16877 || port > 16887)) return false;
    WSADATA data = {};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
    const unsigned short first = port == 0 ? 16877 : port;
    const unsigned short last = port == 0 ? 16887 : port;
    for (unsigned short candidate = first; candidate <= last; candidate++) {
        SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket == INVALID_SOCKET) break;
        const BOOL exclusive = TRUE;
        setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(candidate);
        if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(socket, SOMAXCONN) != 0) { closesocket(socket); continue; }
        auto server = std::make_unique<Server>(Server{socket, handler});
        HANDLE thread = CreateThread(nullptr, 0, Run, server.get(), 0, nullptr);
        if (!thread) { closesocket(socket); break; }
        server.release();
        CloseHandle(thread);
        port = candidate;
        return true;
    }
    WSACleanup();
    return false;
}

} // namespace web_http
