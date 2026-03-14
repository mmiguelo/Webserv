#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include "HttpRouter.hpp"

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

std::string HttpResponse::_getMimeType(const std::string& path)
{
    size_t dot = path.rfind('.');
    if (dot == std::string::npos)
        return "application/octet-stream";
    
    std::string ext = path.substr(dot);
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".txt") return "text/plain";
    if (ext == ".xml") return "application/xml";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".mp4") return "video/mp4";
    return "application/octet-stream";
}

bool HttpResponse::_fileExists(const std::string& path)
{
    std::cout << "Checking file existence: " << path << std::endl;
    std::ifstream file(path.c_str());
    return file.good();
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

std::string HttpResponse::_readFile(const std::string& path) const {
    std::ifstream file(path.c_str());
    if (!file.is_open())
        return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string HttpResponse::buildFromFile(const HttpRequest& request, const std::string& filePath) {

    _version = request.getVersion();
    if (_version.empty())
        _version = "HTTP/1.1";

    std::cout << "Serving resolved path: " << filePath << std::endl;
    std::string path = filePath;
    // If path ends with /, try to serve index.html
    if (!path.empty() && path[path.size() - 1] == '/')
        path += "index.html";
    // If path is a directory without trailing slash, try index.html
    else if (_fileExists(path + "/index.html"))
        path += "/index.html";

    // Check if file exists
    if (_fileExists(path)) {
        std::string body = _readFile(path);
        _statusCode = 200;
        _body = body;
        _contentType = _getMimeType(path);
        return serialize(request.getMethod());
    }
    else
        return buildError(404, request);
}

std::string HttpResponse::buildError(int statusCode, const HttpRequest& request) {
    _version = request.getVersion().empty() ? "HTTP/1.1" : request.getVersion();
    _statusCode = statusCode;
    _contentType = "text/html";

    // Convert status code to string
    std::ostringstream codeStr;
    codeStr << statusCode;

    // Try to read template file
    std::string templateHtml = _readFile("www/html/errors/default.html");

    if (templateHtml.empty()) {
        // Protecao caso alguem apague a pasta? nao sei se isto é necessario, mas é melhor do que retornar uma resposta vazia
        std::ostringstream oss;
        oss << "<!DOCTYPE html>\n<html>\n<head><title>"
            << statusCode << " " << getStatusMessage(statusCode)
            << "</title></head>\n<body>\n<h1>"
            << statusCode << " " << getStatusMessage(statusCode)
            << "</h1>\n</body>\n</html>";
        _body = oss.str();
    }
    else {
        // Replace variables with actual values
        templateHtml = replaceAll(templateHtml, "{{ERROR_CODE}}", codeStr.str());
        templateHtml = replaceAll(templateHtml, "{{ERROR_MESSAGE}}", getStatusMessage(statusCode));
        _body = templateHtml;
    }
    return serialize(request.getMethod());
}

std::string HttpResponse::serialize(HttpMethod method) const {
    std::ostringstream oss;

    oss << _version << " " << _statusCode << " " << getStatusMessage(_statusCode) << "\r\n";
    oss << "Server: WebServ\r\n";
    oss << "Date: " << httpDate() << "\r\n";
    if (!_contentType.empty())
        oss << "Content-Type: " << _contentType << "\r\n";

    if (!_location.empty())
        oss << "Location: " << _location << "\r\n";

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

void HttpResponse::setLocation(const std::string& url) {
    _location = url;
}