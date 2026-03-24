#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"

class ServerConfig {
	public:
		struct ListenDirective {
			std::string host;
			int port;
		
			ListenDirective(const std::string& h, int p) : host(h), port(p) {}
		};
	private:
		std::vector<ListenDirective>	_listen;
		std::string						_root;							//obrigatorio
		size_t							_client_max_body_size;			//opcional
		std::vector<std::string>		_server_name;					//opcional
		std::vector<std::string>		_methods;						//opcional
		std::map<int, std::string>		_error_page;					//opcional
		std::vector<LocationConfig>		_locations;						//opcional
		size_t							_largeHeaderBuffer_size;		//opcional
		//bool 							_hasLargeHeaderBuffers;			//opcional
	
	public:
		ServerConfig();
		void sortLocations();
	
		// Getters
		const std::vector<ListenDirective>& getListenDirectives() const;
		size_t getClientMaxBodySize() const;
		const std::string& getRoot() const;
		const std::vector<std::string>& getServerName() const;
		const std::vector<std::string>& getMethods() const;
		const std::map<int, std::string>& getAllErrorPages() const;
		const std::vector<LocationConfig>& getLocations() const;
		std::string getErrorPage(const int key);
		size_t getLargeHeaderBufferSize() const;
	
		// Setters (set by ConfigParser)
		void setClientMaxBodySize(size_t size);
		void setRoot(const std::string& root);
		void addServerName(const std::string& name);
		void addMethod(const std::string& method);
		void addErrorPage(int code, const std::string& path);
		void addLocation(const LocationConfig& location);
		void addListenDirective(const std::string& host, int port);
		void setLargeHeaderBufferSize(size_t size);
};

#endif