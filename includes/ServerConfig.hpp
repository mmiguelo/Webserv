/* ************************************************************************** */
/*                                                                            */
/*   ServerConfig.hpp - PLACEHOLDER for Dev C                                 */
/*                                                                            */
/*   This header will hold the ServerConfig class that parses                 */
/*   config/default.conf and exposes listen address, server_name,             */
/*   root, error_pages, etc.                                                  */
/*                                                                            */
/*   Dev C: replace this file with your real implementation.                  */
/*   Other devs can already #include "ServerConfig.hpp".                      */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>

struct LocationBlock
{
    std::string path;
    std::string root;
    std::string index;
    bool autoindex;
    std::string redirect;
    std::vector<std::string> allowed_methods;
    std::string cgi_extension;
    std::string cgi_path;
    size_t client_max_body_size;
};

struct ServerBlock
{
    std::string host;
    int port;
    std::string server_name;
    std::string root;
    std::string index;
    std::map<int, std::string> error_pages;
    size_t client_max_body_size;
    std::vector<LocationBlock> locations;
};

class ServerConfig
{
public:
    ServerConfig();
    ~ServerConfig();

    /* Dev C: implement these */
    // void parse(const std::string &filepath);
    // const std::vector<ServerBlock> &getServers() const;

private:
    std::vector<ServerBlock> _servers;
};

#endif
