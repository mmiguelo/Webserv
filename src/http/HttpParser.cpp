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

// Feed raw data into the parser. Returns true when request is COMPLETE.
bool HttpParser::feed(const std::string &data)
{
    _buffer.append(data);
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
    _buffer.erase(0, pos + 2);

    std::vector<std::string> parts;
    std::stringstream ss(requestLine);
    std::string token;

    // Split by spaces: METHOD URI VERSION
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
            if (!cl.empty())
            {
                char *endptr;
                long len = std::strtol(cl.c_str(), &endptr, 10);
                if (*endptr != '\0')
                {
                    _request.setErrorCode(STATUS_BAD_REQUEST);
                    _state = PARSE_ERROR;
                    return false;
                }
                if (len < 0)
                {
                    _request.setErrorCode(STATUS_BAD_REQUEST);
                    _state = PARSE_ERROR;
                    return false;
                }
                _contentLength = static_cast<size_t>(len);
                if (_contentLength == 0)
                {
                    _state = PARSE_COMPLETE;
                    return true; // no need to reed more, since there will be no body
                }
                _state = PARSE_BODY_CONTENT_LENGTH;
                return true;
            }
            _state = PARSE_COMPLETE;
            return true;
        }

        std::string line = _buffer.substr(0, pos);
        _buffer.erase(0, pos + 2); // remove /r/n from the buffer

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
}

bool HttpParser::_parseBodyContentLength()
{
    std::cout << "Parse body content length" << std::endl;
    std::cout << _buffer << std::endl;
    _state = PARSE_COMPLETE;
    return true;
} // TASK 2

bool HttpParser::_parseBodyChunked()
{
    std::cout << "Parse body chunked" << std::endl;
    _state = PARSE_COMPLETE;
    return true;
} // TASK 2

void HttpParser::reset()
{
    _state = PARSE_REQUEST_LINE;
    _buffer.clear();
    _request.reset();
    _contentLength = 0;
    _headerSize = 0;
}

ParserState HttpParser::getState() const
{
    return _state;
}

HttpRequest &HttpParser::getRequest()
{
    return _request;
}

const HttpRequest &HttpParser::getRequest() const
{
    return _request;
}
