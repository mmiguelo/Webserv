#include "http/HttpParser.hpp"

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
        _state         = other._state;
        _buffer        = other._buffer;
        _request       = other._request;
        _contentLength = other._contentLength;
        _headerSize    = other._headerSize;
    }
    return *this;
}

void HttpParser::reset() {
    _state         = PARSE_REQUEST_LINE;
    _buffer.clear();
    _request.reset();
    _contentLength = 0;
    _headerSize    = 0;
}

ParserState HttpParser::getState() const            { return _state; }
HttpRequest& HttpParser::getRequest()               { return _request; }
const HttpRequest& HttpParser::getRequest() const   { return _request; }

// ── Main feed loop ──

bool HttpParser::feed(const std::string& data) {
    _buffer.append(data);

    while (true) {
        if (_state == PARSE_REQUEST_LINE) {
            if (!_parseRequestLine())
                return false;
        } else if (_state == PARSE_HEADERS) {
            if (!_parseHeaders())
                return false;
        } else if (_state == PARSE_BODY_CONTENT_LENGTH) {
            if (!_parseBodyContentLength())
                return false;
        } else if (_state == PARSE_BODY_CHUNKED) {
            if (!_parseBodyChunked())
                return false;
        } else {
            break; // COMPLETE or ERROR
        }
    }
    return _state == PARSE_COMPLETE;
}

// ── REQUEST LINE: "METHOD /path?query HTTP/1.1\r\n" ──

bool HttpParser::_parseRequestLine() {
    std::string::size_type pos = _buffer.find("\r\n");
    if (pos == std::string::npos)
        return false; // incomplete — wait for more data

    std::string line = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 2);

    // Split by spaces: METHOD URI VERSION
    std::string::size_type sp1 = line.find(' ');
    if (sp1 == std::string::npos) {
        _request.setErrorCode(STATUS_BAD_REQUEST);
        _state = PARSE_ERROR;
        return false;
    }
    std::string::size_type sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) {
        _request.setErrorCode(STATUS_BAD_REQUEST);
        _state = PARSE_ERROR;
        return false;
    }

    std::string method  = line.substr(0, sp1);
    std::string uri     = line.substr(sp1 + 1, sp2 - sp1 - 1);
    std::string version = line.substr(sp2 + 1);

    // Validate method (case-sensitive)
    HttpMethod m = stringToMethod(method);
    if (m == METHOD_UNKNOWN) {
        _request.setErrorCode(STATUS_METHOD_NOT_ALLOWED);
        _state = PARSE_ERROR;
        return false;
    }
    _request.setMethod(m);

    // Split URI on '?' into path and query
    std::string::size_type qpos = uri.find('?');
    if (qpos != std::string::npos) {
        _request.setPath(uri.substr(0, qpos));
        _request.setQuery(uri.substr(qpos + 1));
    } else {
        _request.setPath(uri);
    }

    // Validate version
    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        _request.setErrorCode(STATUS_HTTP_VERSION_NOT_SUPPORTED);
        _state = PARSE_ERROR;
        return false;
    }
    _request.setVersion(version);

    _state = PARSE_HEADERS;
    return true;
}

// ── HEADERS: "Key: Value\r\n" ... "\r\n" ──

bool HttpParser::_parseHeaders() {
    while (true) {
        std::string::size_type pos = _buffer.find("\r\n");
        if (pos == std::string::npos)
            return false; // incomplete line — wait

        // Track total header size
        _headerSize += pos + 2;
        if (_headerSize > MAX_HEADER_SIZE) {
            _request.setErrorCode(STATUS_REQUEST_HEADER_TOO_LARGE);
            _state = PARSE_ERROR;
            return false;
        }

        // Empty line = end of headers
        if (pos == 0) {
            _buffer.erase(0, 2);

            // HTTP/1.1 requires Host header
            if (_request.getVersion() == "HTTP/1.1" && !_request.hasHeader("host")) {
                _request.setErrorCode(STATUS_BAD_REQUEST);
                _state = PARSE_ERROR;
                return false;
            }

            // Determine body handling: chunked wins over content-length
            std::string te = _request.getHeader("transfer-encoding");
            if (te.find("chunked") != std::string::npos) {
                _state = PARSE_BODY_CHUNKED;
                return true;
            }

            std::string cl = _request.getHeader("content-length");
            if (!cl.empty()) {
                long len = std::atol(cl.c_str());
                if (len < 0) {
                    _request.setErrorCode(STATUS_BAD_REQUEST);
                    _state = PARSE_ERROR;
                    return false;
                }
                _contentLength = static_cast<size_t>(len);
                if (_contentLength == 0) {
                    _state = PARSE_COMPLETE;
                    return false; // done, break loop
                }
                _state = PARSE_BODY_CONTENT_LENGTH;
                return true;
            }

            // No body expected
            _state = PARSE_COMPLETE;
            return false; // done, break loop
        }

        // Parse "Key: Value"
        std::string line = _buffer.substr(0, pos);
        _buffer.erase(0, pos + 2);

        std::string::size_type colon = line.find(':');
        if (colon == std::string::npos) {
            _request.setErrorCode(STATUS_BAD_REQUEST);
            _state = PARSE_ERROR;
            return false;
        }

        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        _request.setHeader(key, value); // setHeader normalizes key + trims value
    }
}

// ── BODY with Content-Length ──

bool HttpParser::_parseBodyContentLength() {
    if (_buffer.size() < _contentLength)
        return false; // wait for more data

    _request.setBody(_buffer.substr(0, _contentLength));
    _buffer.erase(0, _contentLength);
    _state = PARSE_COMPLETE;
    return false; // done, break loop
}

// ── BODY with chunked Transfer-Encoding ──

bool HttpParser::_parseBodyChunked() {
    while (true) {
        // Find chunk size line
        std::string::size_type pos = _buffer.find("\r\n");
        if (pos == std::string::npos)
            return false;

        std::string sizeStr = _buffer.substr(0, pos);
        unsigned long chunkSize = std::strtoul(sizeStr.c_str(), NULL, 16);

        // Final chunk: "0\r\n\r\n"
        if (chunkSize == 0) {
            // Need the trailing \r\n after the 0-size chunk
            if (_buffer.size() < pos + 4) // "0\r\n\r\n"
                return false;
            _buffer.erase(0, pos + 4);
            _state = PARSE_COMPLETE;
            return false;
        }

        // Need: size-line + \r\n + chunk-data + \r\n
        size_t needed = pos + 2 + chunkSize + 2;
        if (_buffer.size() < needed)
            return false;

        _request.setBody(_request.getBody() + _buffer.substr(pos + 2, chunkSize));
        _buffer.erase(0, needed);
    }
}
