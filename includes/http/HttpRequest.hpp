#pragma once

#include "utils.hpp"

class HttpRequest {
    private:
        HttpMethod                          _method;
        std::string                         _path;
        std::string                         _query;
        std::string                         _version;
        std::map<std::string, std::string>  _headers;
        std::string                         _body;
        HttpStatusCode                      _errorCode;
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
        const std::map<std::string, std::string>& getHeaders() const;

        void    setMethod(HttpMethod method);
        void    setPath(const std::string& path);
        void    setQuery(const std::string& query);
        void    setVersion(const std::string& version);
        void    setBody(const std::string& body);
        void    setErrorCode(HttpStatusCode code);
        void    setHeader(const std::string& key, const std::string& value);
};