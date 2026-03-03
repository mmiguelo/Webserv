#include "HttpResponse.hpp"
#include "HttpRequest.hpp"

//define the static member
std::map<int, std::string> HttpResponse::_codeMsg;

void HttpResponse::initCodeMsg() {
    if (!_codeMsg.empty())
        return;

    _codeMsg[100] = "Continue";
    _codeMsg[101] = "Switching Protocols";
    _codeMsg[200] = "OK";
    _codeMsg[201] = "Created";
    _codeMsg[204] = "No Content";
    _codeMsg[301] = "Moved Permanently";
    _codeMsg[302] = "Found";
    _codeMsg[304] = "Not Modified";
    _codeMsg[400] = "Bad Request";
    _codeMsg[403] = "Forbidden";
    _codeMsg[404] = "Not Found";
    _codeMsg[405] = "Method Not Allowed";
    _codeMsg[408] = "Request Timeout";
    _codeMsg[413] = "Payload Too Large";
    _codeMsg[414] = "Uri Too Long";
    _codeMsg[431] = "Request Header Too Large";
    _codeMsg[500] = "Internal Server Error";
    _codeMsg[501] = "Not Implemented";
    _codeMsg[503] = "Service Unavailable";
    _codeMsg[504] = "Gateway Timeout";
    _codeMsg[505] = "HTTP Version Not Supported";
}

HttpResponse::HttpResponse()
    :   _statusCode(200),
        _body(""),
        _contentType(""),
        _version("") {
    initCodeMsg();
}

HttpResponse::~HttpResponse() {}

HttpResponse::HttpResponse(const HttpResponse& other)
    :   _statusCode(other._statusCode),
        _body(other._body),
        _contentType(other._contentType) {}

HttpResponse& HttpResponse::operator=(const HttpResponse& other) {
    if (this != &other) {
        _codeMsg = other._codeMsg;
        _statusCode = other._statusCode;
        _body = other._body;
        _contentType = other._contentType;
    }
    return *this;
}

void HttpResponse::build(int statusCode, const std::string& body, const std::string& contentType, const std::string& version) {
    _statusCode = statusCode;
    _body = body;
    _contentType = contentType;
    _version = version;
}

void HttpResponse::buildError(int statusCode, const HttpRequest& request) {
    _version = request.getVersion().empty() ? "HTTP/1.1" : request.getVersion();
    _statusCode = statusCode;
    std::string ct = request.getHeader("content-type");
    _contentType = ct.empty() ? "text/html" : ct;
    std::ostringstream oss;
    oss << "<!DOCTYPE html>\n<html>\n<head><title>"
        << statusCode << " " << getStatusMessage(statusCode)
        << "</title></head>\n<body>\n<h1>"
        << statusCode << " " << getStatusMessage(statusCode)
        << "</h1>\n</body>\n</html>";
    _body = oss.str();
}

std::string HttpResponse::serialize(HttpMethod method) const {
    std::ostringstream oss;

    oss << _version << " " << _statusCode << " " << getStatusMessage(_statusCode) << "\r\n";
    oss << "Server: WebServ\r\n";
    oss << "Date: " << httpDate() << "\r\n";
    if (!_contentType.empty())
        oss << "Content-Type: " << _contentType << "\r\n";
    if (hasBody()) // No Content should not have a body
        oss << "Content-Length: " << _body.size() << "\r\n";
    oss << "Connection: keep-alive\r\n"; //TODO on sprint 4
    oss << "\r\n";

    if (hasBody() && method != METHOD_HEAD)
        oss << _body;
    return oss.str();
}

const std::string& HttpResponse::getStatusMessage(int code) const {
    
    std::map<int, std::string>::const_iterator it;
    it = _codeMsg.find(code);
    if (it != _codeMsg.end())
        return it->second;
    static std::string unknown = "Unknown Status";
    return unknown;
}

std::string HttpResponse::httpDate() const {
    char buffer[100];
    time_t now = time(NULL);
    struct tm* gmt = gmtime(&now);

    /*format is: "Mon, 02 Mar 2026 00:00:00 GMT"
    %a - abbreviated weekday name
    %d - day of the month
    %b - abbreviated month name
    %Y - year
    %H - hour (24h)
    %M - minute
    %S - second
    */
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return std::string(buffer);
}

bool HttpResponse::hasBody() const {
    return !(_statusCode == 204 || _statusCode == 304);
}

std::string HttpResponse::getVersion() const {
    return _version;
}