#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include "HttpRouter.hpp"
#include "cgi.hpp"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>
#include "utils.hpp"
#include <sys/wait.h>

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
           _version(""),
           _connection("keep-alive") {
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

std::string HttpResponse::_getMimeType(const std::string& path)
{
    size_t dot = path.rfind('.');
    if (dot == std::string::npos)
        return "application/octet-stream";
    
    std::string ext = path.substr(dot);
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".xml") return "application/xml";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".mp4") return "video/mp4";
    return "application/octet-stream";
}

bool HttpResponse::_fileExists(const std::string& path)
{
    std::ifstream file(path.c_str());
    return file.good();
}

std::string HttpResponse::_sanitizeFilename(const std::string& filename)
{
    size_t slashPos = filename.rfind('/');
    if (slashPos == std::string::npos)
        return filename;
    return filename.substr(slashPos + 1);
}

bool HttpResponse::_writeBinaryFile(const std::string& path, const std::string& data)
{
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        return false;
    out.write(data.data(), data.size());
    return out.good();
}

void HttpResponse::build(int statusCode, const std::string& body, const std::string& contentType, const std::string& version) {
    _statusCode = statusCode;
    _body = body;
    _contentType = contentType;
    _version = version;
}

std::string HttpResponse::replaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
    return str;
}

bool HttpResponse::_readFile(const std::string& path, std::string& out) const {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

int HttpResponse::checkFile(const struct stat& st) const
{    
    if(S_ISDIR(st.st_mode))
        return 300; // Special code to indicate it's a directory

    if (!(S_ISREG(st.st_mode))) //is not a reg file
        return 403;

    return 200; // OK
}

std::string HttpResponse::serialize(HttpMethod method) const {
    std::ostringstream oss;

    const std::string httpVersion = _version.empty() ? "HTTP/1.1" : _version;
    oss << httpVersion << " " << _statusCode << " " << getStatusMessage(_statusCode) << "\r\n";
    oss << "Server: WebServ\r\n";
    oss << "Date: " << httpDate() << "\r\n";
    if (!_contentType.empty())
        oss << "Content-Type: " << _contentType << "\r\n";

    if (!_location.empty())
        oss << "Location: " << _location << "\r\n";
    if (!_allow.empty())
        oss << "Allow: " << _allow << "\r\n";

    if (hasBody()) // No Content should not have a body
    {
        oss << "Content-Length: " << _body.size() << "\r\n";
        if (!_connection.empty())
            oss << "Connection: " << _connection << "\r\n";
    }
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

void HttpResponse::setLocation(const std::string& url) {
    _location = url;
}

void HttpResponse::setAllow(const std::vector<std::string>& methods) {
    _allow.clear();
    for (size_t i = 0; i < methods.size(); i++) {
        if (i > 0)
            _allow += ", ";
        _allow += methods[i];
    }
}

int HttpResponse::getStatusCode() const {
    return _statusCode;
}

void HttpResponse::setConnection(const std::string& conn) {
    _connection = conn;
}
