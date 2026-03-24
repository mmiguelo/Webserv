#pragma once

#include <string>
#include <map>

class Cgi {
    private:
        std::map<std::string, std::string> _env;
        std::string _scriptpath;
    public:
        Cgi();
        ~Cgi();
        void set(const std::string& key, const std::string& value);
        std::string get(const std::string& key) const;
        char** getEnv() const;
        void freeEnv(char **env) const;
        void setScriptPath(const std::string& path);
        std::string getScriptPath() const;
};