#ifndef CONFIGVALIDATOR_HPP
#define CONFIGVALIDATOR_HPP

#include <vector>
#include "config/ServerConfig.hpp"
#include "config/LocationConfig.hpp"


class ConfigValidator {
	private:
		static void validateServer(const ServerConfig& server);
		static void validateLocation(const LocationConfig& location);
		
		static bool isValidMethod(const std::string& method);

	public:
	    static void validate(const std::vector<ServerConfig>& servers); //vector pois pode haver mais que 1 server block no config file
	
};

#endif