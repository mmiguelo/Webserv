#pragma once

#include <string>
#include <map>
#include <iostream>

enum HttpMethod {
    METHOD_GET,
    METHOD_POST,
    METHOD_DELETE,
    METHOD_HEAD,
    METHOD_UNKNOWN
};

enum HttpStatusCode {
    STATUS_CONTINUE                     = 100,
    STATUS_SWITCHING_PROTOCOLS          = 101,
    STATUS_OK                           = 200,
    STATUS_CREATED                      = 201,
    STATUS_NO_CONTENT                   = 204,
    STATUS_MOVED_PERMANENTLY            = 301,
    STATUS_FOUND                        = 302,
    STATUS_NOT_MODIFIED                 = 304,
    STATUS_BAD_REQUEST                  = 400,
    STATUS_FORBIDDEN                    = 403,
    STATUS_NOT_FOUND                    = 404,
    STATUS_METHOD_NOT_ALLOWED           = 405,
    STATUS_REQUEST_TIMEOUT              = 408,
    STATUS_PAYLOAD_TOO_LARGE            = 413,
    STATUS_URI_TOO_LONG                 = 414,
    STATUS_REQUEST_HEADER_TOO_LARGE     = 431,
    STATUS_INTERNAL_SERVER_ERROR        = 500,
    STATUS_NOT_IMPLEMENTED              = 501,
    STATUS_SERVICE_UNAVAILABLE          = 503,
    STATUS_GATEWAY_TIMEOUT              = 504,
    STATUS_HTTP_VERSION_NOT_SUPPORTED   = 505
};

class HttpRequest {
    private:
        HttpMethod                          _method;
        std::string                         _path;
        std::string                         _query;
        std::string                         _version;
        std::map<std::string, std::string>  _headers;
        std::string                         _body;
        HttpStatusCode                      _statusCode;

    public:
        HttpRequest();
        ~HttpRequest();
        HttpRequest(const HttpRequest& other);
        HttpRequest& operator=(const HttpRequest& other);

        void                reset();

        HttpMethod          getMethod() const;
        const std::string&  getPath() const;
        const std::string&  getQuery() const;
        const std::string&  getVersion() const;
        const std::string&  getBody() const;
        HttpStatusCode      getErrorCode() const;
        bool                hasHeader(const std::string& key) const;
        std::string         getHeader(const std::string& key) const;
        const std::map<std::string, std::string>& getAllHeaders() const;

        void    setMethod(HttpMethod method);
        void    setPath(const std::string& path);
        void    setQuery(const std::string& query);
        void    setVersion(const std::string& version);
        void    setBody(const std::string& body);
        void    setErrorCode(HttpStatusCode code);
        void    setHeader(const std::string& key, const std::string& value);
};