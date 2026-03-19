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
        std::string                         _connection; // Connection header value (keep-alive/close)

        static void initCodeMsg();
        bool _readFile(const std::string& path, std::string& out) const;
        static std::string replaceAll(std::string str, const std::string& from, const std::string& to);
        std::string httpDate() const;
        static std::string _getMimeType(const std::string& path);
        static bool _fileExists(const std::string& path);
        static std::string _sanitizeFilename(const std::string& filename);
        static bool _writeBinaryFile(const std::string& path, const std::string& data);

    public:
        HttpResponse();
        ~HttpResponse();
        HttpResponse(const HttpResponse& other);
        HttpResponse& operator=(const HttpResponse& other);

        int checkFile(const struct stat& st) const;

        void build(int statusCode, const std::string& body, const std::string& contentType, const std::string& version);
        std::string buildError(int statusCode, const HttpRequest& request);
        std::string buildFromFile(const HttpRequest& request, const std::string& filePath, int checkResult);
        std::string buildFromDirectory(const HttpRequest& request, const std::string& dirPath, bool autoindex);
        std::string handleDelete(const HttpRequest& request, const std::string& path, int checkResult);
        std::string handleUpload(const HttpRequest& request, const std::string& uploadDir);
        std::string buildAutoIndex(const HttpRequest& request, const std::string& dirPath);
        std::string serialize(HttpMethod method) const;

        const std::string& getStatusMessage(int code) const;
        std::string getVersion() const;
        int getStatusCode() const;
        
        void setLocation(const std::string& url);
        void setAllow(const std::vector<std::string>& methods);
        void setConnection(const std::string& conn);

        bool hasBody() const;
};