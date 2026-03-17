#ifndef VALIDATOR_HPP
#define VALIDATOR_HPP

#include <vector>
#include "config/ServerConfig.hpp"
#include "config/LocationConfig.hpp"

class Validator {
private:
	static void validateServer(const ServerConfig &server);
	static void validateLocation(const LocationConfig &location);

	static bool isValidMethod(const std::string &method);
	static bool isValidHost(const std::string& host);

	static void validateHost(const ServerConfig &server);
	static void validatePort(const ServerConfig &server);
	static void validateRoot(const ServerConfig &server);
	static void validateLocations(const ServerConfig &server);
	static void validateErrorPages(const ServerConfig &server);
	static void validateServerNames(const ServerConfig &server);
	static void validateDuplicateLocations(const std::vector<LocationConfig>& locations);
	static void checkDuplicatePorts(const std::map<int, std::vector<ServerConfig> > &servers);

	static void validateLocationPath(const LocationConfig &location);
	static void validateLocationMethods(const LocationConfig &location);
	static void validateLocationIndex(const LocationConfig &location);
	static void validateLocationRoot(const LocationConfig &location);
	static void validateLocationUpload(const LocationConfig &location);
	static void validateLocationCgi(const LocationConfig &location);
	static void validateLocationRedirect(const LocationConfig &location);
	static void validateLocationError(const LocationConfig &location);

public:
	static void validate(const std::map<int, std::vector<ServerConfig> > &servers); // vector pois pode haver mais que 1 server block no config file
};

#endif