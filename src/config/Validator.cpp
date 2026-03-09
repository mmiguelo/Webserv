#include "config/Validator.hpp"
#include "config/UtilsConfig.hpp"
#include <stdexcept>


void Validator::validate(const std::map<int, ServerConfig> &servers)
{
	if (servers.empty())
		throw std::runtime_error("No server blocks found in configuration.");

	for (std::map<int, ServerConfig>::const_iterator it = servers.begin(); it != servers.end(); ++it) {
		validateServer(it->second);
	}
}

void Validator::validateServer(const ServerConfig &server)
{
	validateHost(server);
	validatePort(server);
	validateRoot(server);
	validateLocations(server);
	validateErrorPages(server);
	validateServerNames(server);
}

void Validator::validateLocation(const LocationConfig &location)
{
	validateLocationPath(location);
	validateLocationMethods(location);
	validateLocationIndex(location);
	validateLocationRoot(location);
	validateLocationUpload(location);
	validateLocationCgi(location);
	validateLocationRedirect(location);
}
