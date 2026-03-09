#include "config/ServerConfig.hpp"
#include <algorithm>

ServerConfig::ServerConfig() : _host("0.0.0.0"), _port(-1), _client_max_body_size(1024 * 1024) {}

static bool compareLocationLength(const LocationConfig& a, const LocationConfig& b) {
	return a.path.length() > b.path.length();
}
void ServerConfig::sortLocations() {
	std::sort(_locations.begin(), _locations.end(),compareLocationLength);
}

// Getters

const std::string& ServerConfig::getHost() const {
	return _host;
}

int ServerConfig::getPort() const {
	return _port;
}

size_t ServerConfig::getClientMaxBodySize() const {
	return _client_max_body_size;
}

const std::string& ServerConfig::getRoot() const {
	return _root;
}

const std::vector<std::string>& ServerConfig::getServerName() const {
	return _server_name;
}

const std::vector<std::string>& ServerConfig::getMethods() const {
	return _methods;
}

const std::map<int, std::string>& ServerConfig::getErrorPage() const {
	return _error_page;
}

const std::vector<LocationConfig>& ServerConfig::getLocations() const {
	return _locations;
}

// Setters (set by ConfigParser)

void ServerConfig::setHost(const std::string& host) {
	_host = host;
}

void ServerConfig::setPort(int port) {
	_port = port;
}

void ServerConfig::setClientMaxBodySize(size_t size) {
	_client_max_body_size = size;
}

void ServerConfig::setRoot(const std::string& root) {
	_root = root;
}

void ServerConfig::addServerName(const std::string& name) {
	_server_name.push_back(name);
}

void ServerConfig::addMethod(const std::string& method) {
	_methods.push_back(method);
}

void ServerConfig::addErrorPage(int code, const std::string& path) {
	_error_page.insert(std::make_pair(code, path));
}

void ServerConfig::addLocation(const LocationConfig& location) {
	_locations.push_back(location);
}