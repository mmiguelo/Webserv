#include "HttpParser.hpp"

HttpParser::HttpParser()
    : _state(PARSE_REQUEST_LINE),
      _contentLength(0),
      _headerSize(0) {}

HttpParser::~HttpParser() {}

HttpParser::HttpParser(const HttpParser &other)
    : _state(other._state),
      _buffer(other._buffer),
      _request(other._request),
      _contentLength(other._contentLength),
      _headerSize(other._headerSize) {}

HttpParser &HttpParser::operator=(const HttpParser &other)
{
    if (this != &other)
    {
        _state = other._state;
        _buffer = other._buffer;
        _request = other._request;
        _contentLength = other._contentLength;
        _headerSize = other._headerSize;
    }
    return *this;
}

void HttpParser::reset()
{
    _state = PARSE_REQUEST_LINE;
    _buffer.clear();
    _request.reset();
    _contentLength = 0;
    _headerSize = 0;
}

void HttpParser::setServerConfig(const ServerConfig& serverConfig) {
    _serverConfig = const_cast<ServerConfig*>(&serverConfig);
}

ServerConfig& HttpParser::getServerConfig() const {
    return *_serverConfig;
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

// Feed raw data into the parser. Returns true ONLY when request is COMPLETE.
bool HttpParser::feed(const std::string& data, const ServerConfig& serverConfig) {
    _buffer.append(data);
    setServerConfig(serverConfig);
    while (true)
    {
        if (_state == PARSE_REQUEST_LINE)
        {
            if (!_parseRequestLine())
                return false;
        }
        else if (_state == PARSE_HEADERS)
        {
            if (!_parseHeaders())
                return false;
        }
        else if (_state == PARSE_BODY_CONTENT_LENGTH)
        {
            if (!_parseBodyContentLength())
                return false;
        }
        else if (_state == PARSE_BODY_CHUNKED)
        {
            if (!_parseBodyChunked())
                return false;
        }
        else
            break;
    }
    return _state == PARSE_COMPLETE;
}

bool HttpParser::_parseRequestLine()
{
    std::string::size_type pos = _buffer.find("\r\n");
    if (pos == std::string::npos)
        return false; // incomplete: wait for more data

    std::string requestLine = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 2); //clean \r\n

    std::vector<std::string> parts;
    std::stringstream ss(requestLine);
    std::string token;

    // Split by spaces into METHOD URI VERSION
    while (std::getline(ss, token, ' '))
        parts.push_back(token);

    if (parts.size() != 3)
    {
        _request.setErrorCode(STATUS_BAD_REQUEST);
        _state = PARSE_ERROR;
        return false;
    }

    // this section is case sensitive
    HttpMethod method = stringToMethod(parts[0]);
    std::string uri = parts[1];
    std::string version = parts[2];
    if (method == METHOD_UNKNOWN)
    {
        _request.setErrorCode(STATUS_METHOD_NOT_ALLOWED);
        _state = PARSE_ERROR;
        return false;
    }
    _request.setMethod(method);

    // Split URI on '?' into path and query
    std::string::size_type qpos = uri.find('?');
    if (qpos == std::string::npos)
        _request.setPath(uri); // means it does not have a query
    else
    {
        _request.setPath(uri.substr(0, qpos));
        _request.setQuery(uri.substr(qpos + 1));
    }

    if (version != "HTTP/1.1" && version != "HTTP/1.0")
    {
        _request.setErrorCode(STATUS_HTTP_VERSION_NOT_SUPPORTED);
        _state = PARSE_ERROR;
        return false;
    }
    _request.setVersion(version);

    _state = PARSE_HEADERS;
    return true;
}

/* validations:
Header must contain ':'
Header size limit
Encoding vs Content-Length
Content-Lenght must be numeric
Host required for HTTP
*/
bool HttpParser::_parseHeaders()
{
    // parse until it hits the empty line (\r\n\r\n)
    // only one header per line
    while (true)
    {
        std::string::size_type pos = _buffer.find("\r\n");
        if (pos == std::string::npos)
            return false;

        // verify if my header is larger then the limit. see HttpParser.hpp define
        _headerSize += pos + 2;
        if (_headerSize > MAX_HEADER_SIZE)
        {
            _request.setErrorCode(STATUS_REQUEST_HEADER_TOO_LARGE);
            _state = PARSE_ERROR;
            return false;
        }

        if (pos == 0)
        {
            // TODO: parse ao conteudo porque ja chegamos ao fim dos headers
            _buffer.erase(0, 2);

            // version HTTP/1.1 requires to have a header "host"
            if (_request.getVersion() == "HTTP/1.1" && !_request.hasHeader("host"))
            {
                _request.setErrorCode(STATUS_BAD_REQUEST);
                _state = PARSE_ERROR;
                return false;
            }

            std::string te = _request.getHeader("transfer-encoding");
            if (te.find("chunked") != std::string::npos)
            {
                _state = PARSE_BODY_CHUNKED;
                return true;
            }

            std::string cl = _request.getHeader("content-length");
            if (!cl.empty()) {
                if (!isValidDecimal(cl)) {
                    _request.setErrorCode(STATUS_BAD_REQUEST);
                    _state = PARSE_ERROR;
                    return false;
                }

                _contentLength = static_cast<size_t>(std::strtol(cl.c_str(), NULL, 10));
                size_t client_max_body_size = _serverConfig->getClientMaxBodySize();

                if (_contentLength > client_max_body_size) {
                    _request.setErrorCode(STATUS_PAYLOAD_TOO_LARGE);
                    _state = PARSE_ERROR;
                    return false;
                }

                if (_contentLength == 0) {
                    _state = PARSE_COMPLETE;
                    return true; //no need to reed more, since there will be no body
                }
                _state = PARSE_BODY_CONTENT_LENGTH;
                return true;
            }
            _state = PARSE_COMPLETE;
            return true;
        }

        std::string line = _buffer.substr(0, pos);
        _buffer.erase(0, pos + 2);

        std::string::size_type colon = line.find(':');
        if (colon == std::string::npos)
        {
            _request.setErrorCode(STATUS_BAD_REQUEST);
            _state = PARSE_ERROR;
            return false;
        }

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        _request.setHeader(key, value); // it trims WS in this function
    }

    std::map<int, ServerConfig> servers;
}

size_t HttpParser::getHeaderSize() const {
    return _headerSize;
}

// body when header has content-length
bool HttpParser::_parseBodyContentLength() {
    if (_buffer.size() < _contentLength)
        return false; // wait for more data

    //this covers both buffer > contenlength and ==
    _request.setBody(_buffer.substr(0, _contentLength));
    _buffer.erase(0, _contentLength);
    _state = PARSE_COMPLETE;
    return true;
}

/* example of body with transfer-encoding for <Hello World!!!> :
4\r\n <- size of the next chunked message you receive
Hell\r\n
A\r\n  <- size is hexadecimal
o World!!!\r\n
0\r\n <- it means message ended
\r\n
*/
bool HttpParser::_parseBodyChunked() {
    while (true) {
        //find first chunk line
        std::string::size_type pos = _buffer.find("\r\n");
        if (pos == std::string::npos)
            return false;

        std::string sizeStr = _buffer.substr(0, pos);
        if (!isValidHexadecimal(sizeStr)) {
            _request.setErrorCode(STATUS_BAD_REQUEST);
            _state = PARSE_ERROR;
            return false;
        }
        unsigned long chunkSize = std::strtoul(sizeStr.c_str(), NULL, 16);
        size_t totalBodySize = _request.getBody().size() + chunkSize;
        size_t client_max_body_size = _serverConfig->getClientMaxBodySize();

        if (totalBodySize > client_max_body_size) {
            _request.setErrorCode(STATUS_PAYLOAD_TOO_LARGE);
            _state = PARSE_ERROR;
            return false;
        }
        //Final chunk: "0\r\n\r\n"
        if (chunkSize == 0) {
            if (_buffer.size() < pos + 4)
                return false;
            _buffer.erase(0, pos + 4);
            _state = PARSE_COMPLETE;
            return true;
        }

        // needed: size-line + \r\n (2) + chunk-data + \r\n (2)
        size_t needed = pos + 2 + chunkSize + 2;
        if (_buffer.size() < needed) //information missing
            return false;

        std::string chunk = _buffer.substr(pos + 2, chunkSize);
        _request.setBody(_request.getBody() + chunk);
        _buffer.erase(0, needed);
    }
}