#ifndef VIVALDI_PLUS_WEB_HTTP_H_
#define VIVALDI_PLUS_WEB_HTTP_H_

#include <cstddef>
#include <map>
#include <string>

namespace web_http {

constexpr size_t kMaxBody = 65536;

struct Request {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct Response {
    int status = 200;
    std::string type = "application/json; charset=utf-8";
    std::string body;
};

using Handler = Response (*)(const Request&);

bool ParseHead(const std::string& head, Request& request, size_t& bodyLength);
bool Start(Handler handler, unsigned short& port);

} // namespace web_http

#endif
