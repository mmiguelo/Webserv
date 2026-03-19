#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>
#include <map>

struct LocationConfig {
	std::string path;									//obrigatorio
	std::string root;									//opcional
	std::vector<std::string> index; 					//opcional
	std::string upload_dir;								//opcional
	size_t client_max_body_size;
	bool has_client_max_body_size;

	bool autoindex;										//opcional
	std::vector<std::string> methods;					//opcional
	std::map<std::string, std::string> cgi_ext;			//opcional
	std::map<int, std::string> error_page;

	std::string redirect_url;							//opcional
	int redirect_code;									//default 0
	bool has_redirect;

	LocationConfig() : client_max_body_size(0), has_client_max_body_size(false), autoindex(false), redirect_code(0), has_redirect(false) {} //os outros campos iniciam-se vazios por defualt
};

#endif