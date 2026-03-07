#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"

class ServerConfig {
private:
	std::string _host;							//default "0.0.0.0"
	int _port; 									//obrigatorio
	size_t _client_max_body_size;				//opcional example: client_max_body_size 5M; -> 5 * 1024 * 1024
	std::string _root;							//obrigatorio
	std::vector<std::string> _server_name;		//opcional
	std::vector<std::string> _methods;			//opcional
	std::map<int, std::string> _error_page;		//opcional
	std::vector<LocationConfig> _locations;		//opcional

public:
	ServerConfig();
	void sortLocations();
	// Getters
	const std::string& getHost() const;
	int getPort() const;
	size_t getClientMaxBodySize() const;
	const std::string& getRoot() const;
	const std::vector<std::string>& getServerName() const;
	const std::vector<std::string>& getMethods() const;
	const std::map<int, std::string>& getErrorPage() const;
	const std::vector<LocationConfig>& getLocations() const;

	// Setters (set by ConfigParser)
	void setHost(const std::string& host);
	void setPort(int port);
	void setClientMaxBodySize(size_t size);
	void setRoot(const std::string& root);
	void addServerName(const std::string& name);
	void addMethod(const std::string& method);
	void addErrorPage(int code, const std::string& path);
	void addLocation(const LocationConfig& location);
};
#endif