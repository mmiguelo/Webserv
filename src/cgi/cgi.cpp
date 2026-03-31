#include "cgi.hpp"
#include <cstring>

Cgi::Cgi() {}

Cgi::~Cgi() {}

void Cgi::set(const std::string& key, const std::string& value) {
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
    env[i] = NULL;
    return env;
}

void Cgi::freeEnv(char **env) const {
    if (!env)
        return;
    for (size_t i = 0; env[i] != NULL; i++) {
        delete[] env[i];
    }
    delete[] env;
}

void Cgi::setScriptPath(const std::string& path) {
    _scriptpath = path;
}

std::string Cgi::getScriptPath() const {
    return _scriptpath;
}