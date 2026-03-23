#include "ServerConfig.hpp"
#include "utils.hpp"
#include <algorithm>
#include <iostream>

ServerConfig::ServerConfig() : _client_max_body_size(1024 * 1024) {} // 1MB

static bool compareLocationLength(const LocationConfig &a, const LocationConfig &b)
{
	return a.path.length() > b.path.length();
}
void ServerConfig::sortLocations()
{
	std::sort(_locations.begin(), _locations.end(), compareLocationLength);
}

// Getters
const std::vector<ServerConfig::ListenDirective>& ServerConfig::getListenDirectives() const {
	return _listen;
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
const std::map<int, std::string>& ServerConfig::getAllErrorPages() const {
	return _error_page;
}
const std::vector<LocationConfig>& ServerConfig::getLocations() const {
	return _locations;
}
void ServerConfig::setLargeHeaderBufferSize(size_t size) {
	_largeHeaderBuffer_size = size;
}

// Setters (set by ConfigParser)
void ServerConfig::addListenDirective(const std::string& host, int port) {
	_listen.push_back(ListenDirective(host, port));
}
void ServerConfig::setClientMaxBodySize(size_t size) {
	_client_max_body_size = size;
}
void ServerConfig::setRoot(const std::string& root) {
	_root = toAbsolutePath(root);
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
std::string ServerConfig::getErrorPage(const int key) {
	return _error_page[key];
}
size_t ServerConfig::getLargeHeaderBufferSize() const {
	return _largeHeaderBuffer_size;
}