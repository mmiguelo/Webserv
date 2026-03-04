#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>
#include <map>

// Exemplo de configuração da `location`
// A struct tera que conter os seguintes campos(nao necessariamente todos)
//
//	location /images {
//	    root /data/images;
//		upload_dir /data/uploads;
//	    autoindex on;
//	    methods GET POST;
//	    cgi_ext .php /usr/bin/php-cgi;
//	    return 301 http://www.coise.com/imagens/;
//	}

struct LocationConfig {
	std::string path;									//obrigatorio
	std::string root;									//obrigatorio
	std::string upload_dir;								//opcional
	bool autoindex;										//opcional

	std::vector<std::string> methods;					//opcional
	std::map<std::string, std::string> cgi_ext;			//opcional
	
	std::string return_url;								//opcional
	int return_code;									//default 0

	LocationConfig() : autoindex(false), return_code(0) {} //os outros campos iniciam-se vazios por defualt
};

#endif