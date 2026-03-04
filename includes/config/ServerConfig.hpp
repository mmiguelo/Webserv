#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"

// Exemplo de configuração do `server`
// A struct tera que conter os seguintes campos(nao necessariamente todos)
//	server {
//	    listen 8080;
//	    server_name coiso.com;
//	    root /var/www/html;
//	    methods GET POST;
//	    client_max_body_size 5M;
//	    error_page 404 /errors/404.html;
//	}

struct ServerConfig {
	std::string host;							//default "0.0.0.0"
	int port; 									//obrigatorio
	size_t client_max_body_size;				//opcional example: client_max_body_size 5M; -> 5 * 1024 * 1024
	std::string root;							//obrigatorio

	std::vector<std::string> server_name;		//opcional
	std::vector<std::string> methods;			//opcional
	std::map<int, std::string> error_page;		//opcional

	std::vector<LocationConfig> locations;		//opcional
	ServerConfig() : host("0.0.0.0"), port(-1), client_max_body_size(1024 * 1024) {}
};
#endif