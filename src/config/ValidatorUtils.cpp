#include "config/Validator.hpp"
#include "config/UtilsConfig.hpp"
#include <stdexcept>
#include <cstdlib>

void Validator::validateHost(const ServerConfig &server) {
	if (!isValidHost(server.getHost()))
		throw std::runtime_error("Invalid host in listen directive.");
}

void Validator::validatePort(const ServerConfig &server) {
	int port = server.getPort();
	if (port == -1)
		throw std::runtime_error("Server block missing listen directive.");
	if (port <= 0 || port > 65535)
		throw std::runtime_error("Invalid port number.");
}

void Validator::validateRoot(const ServerConfig &server) {
	if (server.getRoot().empty())
		throw std::runtime_error("Root directive is required in server block.");
}

void Validator::validateDuplicateLocations(const std::vector<LocationConfig>& locations) {
	for (size_t i = 0; i < locations.size(); i++) {
		for (size_t j = i + 1; j < locations.size(); j++) {
			if (normalizePath(locations[i].path) == normalizePath(locations[j].path))
			throw std::runtime_error("Duplicate location paths found.");
		}
	}
}

void Validator::validateErrorPages(const ServerConfig &server) {
	const std::map<int, std::string>& errorPages = server.getErrorPage();
	for (std::map<int, std::string>::const_iterator it = errorPages.begin();
	it != errorPages.end(); ++it) {
		int code = it->first;
		if (code < 100 || code > 599)
		throw std::runtime_error("Invalid HTTP status code in error_page directive.");
	}
}

void Validator::validateServerNames(const ServerConfig &server) {
	const std::vector<std::string>& names = server.getServerName();
	for (size_t i = 0; i < names.size(); i++) {
		for (size_t j = i + 1; j < names.size(); j++) {
			if (names[i] == names[j])
			throw std::runtime_error("Duplicate server names found.");
		}
	}
}

void Validator::validateLocations(const ServerConfig &server) {
	const std::vector<LocationConfig>& locations = server.getLocations();
	validateDuplicateLocations(locations);
	for (size_t i = 0; i < locations.size(); i++)
		validateLocation(locations[i]);
}

//AUXILIARY FUNCTIONS

bool Validator::isValidMethod(const std::string &method) {
	return (method == "GET" ||
			method == "POST" ||
			method == "DELETE");
}

bool Validator::isValidHost(const std::string& host) {
    if (host == "localhost")
        return true;

    int parts = 0;
	std::string segment;
    for (size_t i = 0; i <= host.length(); i++)
    {
		if ( i == host.length() || host[i] == '.' ) {
			if (segment.empty())
				return false;
			if (!isNumber(segment))
				return false;
			int value = std::atoi(segment.c_str());
			if (value < 0 || value > 255)
				return false;
			segment.clear();
			parts++;
		} else
			segment += host[i];
    }
    return parts == 4;
}