#pragma once

#include "HttpRequest.hpp"
#include "ServerConfig.hpp"
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <ctime>
#include <fstream>

class HttpResponse {
    private:
        static std::map<int, std::string>   _codeMsg;
        int                                 _statusCode;
        std::string                         _body;
        std::string                         _contentType;
        std::string                         _version;

        std::string                         _location;  //TESTE
        std::string                         _allow;     //TESTE

        static void initCodeMsg();
        std::string _readFile(const std::string& path) const;
        static std::string replaceAll(std::string str, const std::string& from, const std::string& to);
        std::string httpDate() const;
        static std::string _getMimeType(const std::string& path);
        static bool _fileExists(const std::string& path);
        std::string _getErrorPage(int code, const ServerConfig* config);

    public:
        HttpResponse();
        ~HttpResponse();
        HttpResponse(const HttpResponse& other);
        HttpResponse& operator=(const HttpResponse& other);

        int checkFile(const std::string& path) const;

        void build(int statusCode, const std::string& body, const std::string& contentType, const std::string& version);
        std::string buildError(int statusCode, const HttpRequest& request);
        std::string buildFromFile(const HttpRequest& request, const std::string& filePath);
        std::string buildFromDirectory(const HttpRequest& request, const std::string& dirPath, bool autoindex);
        std::string buildAutoIndex(const HttpRequest& request, const std::string& dirPath);
        std::string serialize(HttpMethod method) const;

        const std::string& getStatusMessage(int code) const;
        std::string getVersion() const;
        
        void setLocation(const std::string& url);
        void setAllow(const std::vector<std::string>& methods);

        bool hasBody() const;
};