#pragma once

#include "HttpRequest.hpp"
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <ctime>

class HttpResponse {
    private:
        static std::map<int, std::string>   _codeMsg;
        int                                 _statusCode;
        std::string                         _body;
        std::string                         _contentType;
        std::string                         _version;

        static void initCodeMsg();
    public:
        HttpResponse();
        ~HttpResponse();
        HttpResponse(const HttpResponse& other);
        HttpResponse& operator=(const HttpResponse& other);

        void build(int statusCode, const std::string& body, const std::string& contentType, const std::string& version);
        void buildError(int statusCode);
        std::string serialize(HttpMethod method) const;

        const std::string& getStatusMessage(int code) const;
        std::string httpDate() const;

        bool hasBody() const;
};