#include "HttpRequest.hpp"

HttpRequest::HttpRequest() : _method(METHOD_UNKNOWN), _errorCode(STATUS_OK) {}

HttpRequest::~HttpRequest() {}

HttpRequest::HttpRequest(const HttpRequest& other) 
    : _method(other._method),
      _path(other._path),
      _query(other._query),
      _version(other._version),
      _headers(other._headers),
      _body(other._body),
      _errorCode(other._errorCode) {}

HttpRequest& HttpRequest::operator=(const HttpRequest& other) {
    if (this != &other) {
        _method = other._method;
        _path = other._path;
        _query = other._query;
        _version = other._version;
        _headers = other._headers;
        _body = other._body;
        _errorCode = other._errorCode;
    }
    return *this;
}

void HttpRequest::reset() {
    _method = METHOD_UNKNOWN;
    _path.clear();
    _query.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
    _errorCode = STATUS_OK;
}

HttpMethod HttpRequest::getMethod() const {
    return _method;
}

const std::string& HttpRequest::getPath() const {
    return _path;
}

const std::string& HttpRequest::getQuery() const {
    return _query;
}

const std::string& HttpRequest::getVersion() const {
    return _version;
}

const std::string& HttpRequest::getBody() const {
    return _body;
}

HttpStatusCode HttpRequest::getErrorCode() const {
    return _errorCode;
}

bool HttpRequest::hasHeader(const std::string& key) const {
    if (_headers.find(toLowerStr(key)) != _headers.end())
        return true;
    else
        return false;
}

/*Iters through map and returns the value of the header
ex: Content-Type: text/plain
key -> Content-Type
return -> "text/plain"
*/
std::string HttpRequest::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it;
    it = _headers.find(toLowerStr(key));
    if (it != _headers.end())
        return it->second;
    return "";
}

const std::map<std::string, std::string>& HttpRequest::getAllHeaders() const {
    return _headers;
}

void HttpRequest::setMethod(HttpMethod method) {
    _method = method;
}

void HttpRequest::setPath(const std::string& path) {
    _path = path;
}

void HttpRequest::setQuery(const std::string& query) {
    _query = query;
}

void HttpRequest::setVersion(const std::string& version) {
    _version = version;
}

void HttpRequest::setBody(const std::string& body) {
    _body = body;
}

void HttpRequest::setErrorCode(HttpStatusCode code) {
    _errorCode = code;
}

void HttpRequest::setHeader(const std::string& key, const std::string& value) {
    _headers[toLowerStr(key)] = trimWhitespace(value);
}

void HttpRequest::print(std::ostream& os) const
{
    os << "===== HttpRequest =====" << std::endl;

    os << "Method: " << _method << std::endl;
    os << "Path: " << _path << std::endl;
    os << "Query: " << _query << std::endl;
    os << "Version: " << _version << std::endl;
    os << "Error Code: " << _errorCode << std::endl;

    os << "\nHeaders:" << std::endl;
    if (_headers.empty())
        os << "  (none)" << std::endl;
    else
    {
        for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
             it != _headers.end(); ++it)
        {
            os << "  " << it->first << ": " << it->second << std::endl;
        }
    }

    os << "\nBody:" << std::endl;
    if (_body.empty())
        os << "  (empty)" << std::endl;
    else
        os << _body << std::endl;

    os << "=======================" << std::endl;
}