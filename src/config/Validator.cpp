#include "Validator.hpp"
#include "utils.hpp"
#include <stdexcept>
#include <cstdlib>
#include <iostream>

void Validator::validate(const std::map<int, std::vector<ServerConfig>> &servers)
{
	if (servers.empty())
		throw std::runtime_error("No server blocks found in configuration.");

	checkDuplicatePorts(servers);
	for (std::map<int, std::vector<ServerConfig>>::const_iterator it = servers.begin(); it != servers.end(); ++it)
	{
		const std::vector<ServerConfig> &serverConfigs = it->second;
		for (size_t i = 0; i < serverConfigs.size(); i++)
		{
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

void Validator::checkDuplicatePorts(const std::map<int, std::vector<ServerConfig>> &servers)
{
	for (std::map<int, std::vector<ServerConfig>>::const_iterator it = servers.begin();
		 it != servers.end(); ++it)
	{
		if (it->second.size() > 1)
		{
			throw std::runtime_error("Duplicate port found in config.");
		}
	}
}

void Validator::validateHost(const ServerConfig &server)
{
	const std::vector<ServerConfig::ListenDirective> &listens = server.getListenDirectives();
	for (size_t i = 0; i < listens.size(); i++)
	{
		if (!isValidHost(listens[i].host))
			throw std::runtime_error("Invalid host in listen directive.");
	}
}

void Validator::validatePort(const ServerConfig &server)
{

	const std::vector<ServerConfig::ListenDirective> &listens = server.getListenDirectives();
	if (listens.empty())
		throw std::runtime_error("Server block missing listen directive.");

	for (size_t i = 0; i < listens.size(); i++)
	{
		int port = listens[i].port;
		if (port < 1 || port > 65535)
			throw std::runtime_error("Invalid port number in listen directive.");
	}
}

void Validator::validateRoot(const ServerConfig &server)
{
	if (server.getRoot().empty())
	{
		bool locationHasRoot = false;

		const std::vector<LocationConfig> &locations = server.getLocations();
		for (size_t i = 0; i < locations.size(); i++)
		{
			if (!locations[i].root.empty())
			{
				locationHasRoot = true;
				break;
			}
		}
		if (!locationHasRoot)
			throw std::runtime_error("Root directive is required in server block.");
	}
}

void Validator::validateErrorPages(const ServerConfig &server)
{
	const std::map<int, std::string> &errorPages = server.getErrorPage();
	for (std::map<int, std::string>::const_iterator it = errorPages.begin();
		 it != errorPages.end(); ++it)
	{
		int code = it->first;
		if (code < 100 || code > 599)
			throw std::runtime_error("Invalid HTTP status code in error_page directive.");
	}
}

void Validator::validateServerNames(const ServerConfig &server)
{
	const std::vector<std::string> &names = server.getServerName();
	for (size_t i = 0; i < names.size(); i++)
	{
		for (size_t j = i + 1; j < names.size(); j++)
		{
			if (names[i] == names[j])
				throw std::runtime_error("Duplicate server names found.");
		}
	}
}

void Validator::validateLocations(const ServerConfig &server)
{
	const std::vector<LocationConfig> &locations = server.getLocations();
	validateDuplicateLocations(locations);
	for (size_t i = 0; i < locations.size(); i++)
		validateLocation(locations[i]);
}

void Validator::validateDuplicateLocations(const std::vector<LocationConfig> &locations)
{
	for (size_t i = 0; i < locations.size(); i++)
	{
		for (size_t j = i + 1; j < locations.size(); j++)
		{
			if (normalizePath(locations[i].path) == normalizePath(locations[j].path))
				throw std::runtime_error("Duplicate location paths found.");
		}
	}
}

// AUXILIARY FUNCTIONS

bool Validator::isValidMethod(const std::string &method)
{
	return (method == "GET" ||
			method == "POST" ||
			method == "DELETE" ||
			method == "HEAD");
}

bool Validator::isValidHost(const std::string &host)
{
	if (host == "localhost")
		return true;

	int parts = 0;
	std::string segment;
	for (size_t i = 0; i <= host.length(); i++)
	{
		if (i == host.length() || host[i] == '.')
		{
			if (segment.empty())
				return false;
			if (!isNumber(segment))
				return false;
			int value = std::atoi(segment.c_str());
			if (value < 0 || value > 255)
				return false;
			segment.clear();
			parts++;
		}
		else
			segment += host[i];
	}
	return parts == 4;
}

void Validator::validateLocationPath(const LocationConfig &location)
{
	if (location.path.empty())
		throw std::runtime_error("Location path is required.");
	if (location.path[0] != '/')
		throw std::runtime_error("Location path must start with '/'.");
	if (location.path.find(' ') != std::string::npos)
		throw std::runtime_error("Location path must not contain spaces.");
}

void Validator::validateLocationMethods(const LocationConfig &location)
{
	for (size_t i = 0; i < location.methods.size(); i++)
	{
		if (!isValidMethod(location.methods[i]))
			throw std::runtime_error("Invalid method found in location block.");

		for (size_t j = i + 1; j < location.methods.size(); j++)
		{
			if (location.methods[i] == location.methods[j])
				throw std::runtime_error("Duplicate methods in location block.");
		}
	}
}

void Validator::validateLocationIndex(const LocationConfig &location)
{
	for (size_t i = 0; i < location.index.size(); i++)
	{
		if (location.index[i].empty())
			throw std::runtime_error("Invalid index file name.");
	}
}

void Validator::validateLocationRoot(const LocationConfig &location)
{
	if (!location.root.empty())
	{
		if (location.root[0] != '/')
			throw std::runtime_error("Root must be an absolute path.");
	}
}

void Validator::validateLocationUpload(const LocationConfig &location)
{
	if (!location.upload_dir.empty())
	{
		if (location.upload_dir[0] != '/')
			throw std::runtime_error("Upload directory must be absolute.");
	}
}

void Validator::validateLocationCgi(const LocationConfig &location)
{
	for (std::map<std::string, std::string>::const_iterator it = location.cgi_ext.begin();
		 it != location.cgi_ext.end(); ++it)
	{
		if (it->first.empty() || it->first[0] != '.')
			throw std::runtime_error("Invalid CGI extension.");

		if (it->second.empty())
			throw std::runtime_error("Invalid CGI executable path.");
	}
}

void Validator::validateLocationRedirect(const LocationConfig &location)
{
	if (!location.has_redirect)
		return;
	if (location.redirect_code < 100 || location.redirect_code > 599)
		throw std::runtime_error("Invalid redirect code in location block.");
	if ((location.redirect_code == 301 || location.redirect_code == 302 ||
		 location.redirect_code == 303 || location.redirect_code == 307 ||
		 location.redirect_code == 308) &&
		location.redirect_url.empty())
	{
		throw std::runtime_error("Redirect URL is required for redirect codes 301, 302, 303, 307 and 308.");
	}
	if (!location.redirect_url.empty())
	{
		if (location.redirect_url[0] != '/' && location.redirect_url.find("http://") != 0 && location.redirect_url.find("https://") != 0)
			throw std::runtime_error("Redirect URL must be an absolute path or start with http:// or https://.");
	}
}
