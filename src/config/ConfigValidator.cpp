#include "config/configValidator.hpp"
#include <stdexcept>

void ConfigValidator::validate(const std::map<int, ServerConfig> &servers)
{
	if (servers.empty())
		throw std::runtime_error("No server blocks found in configuration.");

	for (size_t i = 0; i < servers.size(); i++)
	{
		for (size_t j = i + 1; j < servers.size(); j++)
		{
			if (servers[i].port == servers[j].port && servers[i].host == servers[j].host)
				throw std::runtime_error("Duplicate server blocks with same host and port found.");
		}
	}
	for (size_t i = 0; i < servers.size(); i++)
		validateServer(servers[i]);
}

void ConfigValidator::validateServer(const ServerConfig &server)
{
	if (server.port <= 0 || server.port > 65535)
		throw std::runtime_error("Invalid port number.");
	if (server.root.empty())
		throw std::runtime_error("Root directive is required.");

	for (size_t i = 0; i < server.locations.size(); i++)
	{
		for (size_t j = i + 1; j < server.locations.size(); j++)
		{
			if (server.locations[i].path == server.locations[j].path)
				throw std::runtime_error("Duplicate location paths found.");
		}
		validateLocation(server.locations[i]);
	}
	for (std::map<int, std::string>::const_iterator it = server.error_page.begin(); it != server.error_page.end(); ++it)
	{
		int code = it->first;
		if (code < 100 || code > 599)
			throw std::runtime_error("Invalid HTTP status code in error_page directive.");
	}

	for (size_t i = 0; i < server.server_name.size(); i++)
	{
		for (size_t j = i + 1; j < server.server_name.size(); j++)
		{
			if (server.server_name[i] == server.server_name[j])
				throw std::runtime_error("Duplicate server names found.");
		}
	}
}

void ConfigValidator::validateLocation(const LocationConfig &location)
{
	if (location.path.empty())
		throw std::runtime_error("Location path is required.");
	if (location.path[0] != '/')
		throw std::runtime_error("Location path must start with '/'.");
	if (!location.methods.empty())
	{
		for (size_t i = 0; i < location.methods.size(); i++)
		{
			if (!isValidMethod(location.methods[i]))
				throw std::runtime_error("Invalid method found in location block.");
		}
	}
}

bool ConfigValidator::isValidMethod(const std::string &method)
{
	return (method == "GET" ||
			method == "POST" ||
			method == "DELETE");
}