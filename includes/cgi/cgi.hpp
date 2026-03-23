#pragma once

#include <string>
#include <map>

class Cgi {
    private:
        std::map<std::string, std::string> _env;
        REQUEST_METHOD; // rq.method
        QUERY_STRING; // request.query_string
        CONTENT_TYPE; // request.content_type
        CONTENT_LENGTH; // request.content_length
        SCRIPT_FILENAME; // configLocation.cgi_ext
        PATH_INFO; // request.path_info
        SERVER_NAME; // configServer.server_name
        SERVER_PORT; // configServer.listen
        SERVER_PROTOCOL; // request.getVersion()
        SERVER_SOFTWARE; // "webserv/1.0"
        GATEWAY_INTERFACE; // "CGI/1.1"
    public:
        Cgi();
        ~Cgi();
        void set(const std::string& key, const std::string& value);
        std::string get(const std::string& key) const;
        char** getEnv() const;
        void clearEnv(char **env) const;

}