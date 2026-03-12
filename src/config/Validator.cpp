#include "config/Validator.hpp"
#include "config/UtilsConfig.hpp"
#include <stdexcept>


void Validator::validate(const std::map<int, std::vector<ServerConfig> > &servers)
{
	if (servers.empty())
		throw std::runtime_error("No server blocks found in configuration.");
		
	checkDuplicatePorts(servers);
	for (std::map<int, std::vector<ServerConfig> >::const_iterator it = servers.begin(); it != servers.end(); ++it) {
		const std::vector<ServerConfig> &serverConfigs = it->second;
		for (size_t i = 0; i < serverConfigs.size(); i++) {
			validateServer(serverConfigs[i]);
		}
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

void Validator::checkDuplicatePorts(const std::map<int, std::vector<ServerConfig> > &servers) {
	for (std::map<int, std::vector<ServerConfig> >::const_iterator it = servers.begin();
		it != servers.end(); ++it) {
			if (it->second.size() > 1) {
				throw std::runtime_error("Duplicate port found in config.");
			}
	}
}