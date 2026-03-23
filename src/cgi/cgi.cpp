#include "cgi.hpp"

Cgi::Cgi() {
    // Initialize CGI environment variables already known
    setEnv("GATEWAY_INTERFACE", "CGI/1.1");
    setEnv("SERVER_SOFTWARE", "webserv/1.0");
}

Cgi::~Cgi() {}

void Cgi::setEnv(const std::string& key, const std::string& value) {
    _env[key] = value;
}

std::string Cgi::get(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _env.find(key);
    if (it != _env.end())
        return it->second;
    return "";
}

char** Cgi::getEnv() const {
    char **env = new char*[_env.size() + 1];
    size_t i = 0;
    for (std::map<std::string, std::string>::const_iterator it = _env.begin(); it != _env.end(); ++it) {
        std::string envVar = it->first + "=" + it->second;
        env[i] = new char[envVar.size() + 1];
        std::strcpy(env[i], envVar.c_str());
        i++;
    }
    env[i] = NULL; // Null-terminate the array
    return env;
}

void Cgi::clearEnv(char **env) const {
    if (!env)
        return;
    for (size_t i = 0; env[i] != NULL; i++) {
        delete env[i];
    }
    delete env;
}