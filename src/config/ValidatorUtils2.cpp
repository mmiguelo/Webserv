#include "config/Validator.hpp"
#include "config/UtilsConfig.hpp"
#include <stdexcept>

void Validator::validateLocationPath(const LocationConfig &location) {
	if (location.path.empty())
		throw std::runtime_error("Location path is required.");
	//normalize path 
	if (location.path[0] != '/')
		throw std::runtime_error("Location path must start with '/'.");
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
	for (std::map<std::string,std::string>::const_iterator it = location.cgi_ext.begin();
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
	if((location.redirect_code == 301 ||
	   location.redirect_code == 302 ||
	   location.redirect_code == 303 ||
	   location.redirect_code == 307 ||
	   location.redirect_code == 308)
	   && location.redirect_url.empty())
	   throw std::runtime_error("Redirect URL must start with '/'.");
}
