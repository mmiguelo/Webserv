#include "HttpParser.hpp"

HttpParser::HttpParser() 
    : _state(PARSE_REQUEST_LINE),
      _contentLength(0),
      _headerSize(0) {}

HttpParser::~HttpParser() {}

HttpParser::HttpParser(const HttpParser& other)
    : _state(other._state),
      _buffer(other._buffer),
      _request(other._request),
      _contentLength(other._contentLength),
      _headerSize(other._headerSize) {}

HttpParser& HttpParser::operator=(const HttpParser& other) {
    if (this != &other) {
        _state = other._state;
        _buffer = other._buffer;
        _request = other._request;
        _contentLength = other._contentLength;
        _headerSize = other._headerSize;
    }
    return *this;
}

// Feed raw data into the parser. Returns true when request is COMPLETE.
bool HttpParser::feed(const std::string& data) {
    _buffer.append(data);
    while(true) {
        if (_state == PARSE_REQUEST_LINE) {
            if (!_parseRequestLine())
                return false;
        }
        else if (_state == PARSE_HEADERS) {
            if (!_parseHeaders())
                return false;
        }
        else if (_state == PARSE_BODY_CONTENT_LENGTH) {
            if (!_parseBodyContentLength())
                return false;
        }
        else if (_state == PARSE_BODY_CHUNKED) {
            if (!_parseBodyChunked())
                return false;
        }
        else
            break;
    }
    return _state == PARSE_COMPLETE;
}

bool HttpParser::_parseRequestLine() {
    std::string::size_type pos = _buffer.find("\r\n");
    if (pos == std::string::npos)
        return false;

    std::string requestLine = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 2);

    std::vector<std::string> parts;
    std::stringstream ss(requestLine);
    std::string token;
    while (std::getline(ss, token, ' '))
        parts.push_back(token);

    if (parts.size() != 3) {
        _request.setErrorCode(STATUS_BAD_REQUEST);
        _state = PARSE_ERROR;
        return false;
    }

    HttpMethod method = stringToMethod(parts[0]);
    std::string uri     = parts[1];
    std::string version = parts[2];
    if (method == METHOD_UNKNOWN) {
        _request.setErrorCode(STATUS_METHOD_NOT_ALLOWED);
        _state = PARSE_ERROR;
        return false;
    }
    _request.setMethod(method);

    std::string::size_type qpos = uri.find('?');
    if (qpos == std::string::npos)
        _request.setPath(uri);
    else {
        _request.setPath(uri.substr(0, qpos));
        _request.setQuery(uri.substr(qpos + 1));
    }

    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        _request.setErrorCode(STATUS_HTTP_VERSION_NOT_SUPPORTED);
        _state = PARSE_ERROR;
        return false;
    }
    _request.setVersion(version);
    
    _state = PARSE_HEADERS;
    return true;
}

bool HttpParser::_parseHeaders() {
}

bool HttpParser::_parseBodyContentLength() {

}

bool HttpParser::_parseBodyChunked() {

}

void HttpParser::reset() {
    _state = PARSE_REQUEST_LINE;
    _buffer.clear();
    _request.reset();
    _contentLength = 0;
    _headerSize = 0;
}

ParserState HttpParser::getState() const {
    return _state;
}

HttpRequest& HttpParser::getRequest() {
    return _request;
}

const HttpRequest& HttpParser::getRequest() const {
    return _request;
}
