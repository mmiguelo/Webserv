#include "http/HttpRouter.hpp"
#include <limits.h>
#include <stdlib.h>


static std::string methodToString(HttpMethod method);

HttpRouteMatch HttpRouter::route(const HttpRequest& request, const ServerConfig& serverConfig) {
	HttpRouteMatch match;

	//LOCATUION
	const LocationConfig* bestLocation = findBestLocation(request, serverConfig);
	if (!bestLocation) {
		match.errorCode = 404; // Not Found
		return match;
	}
	match.location = bestLocation;

	//REDIRECT
	if(bestLocation->has_redirect) {
		match.errorCode = bestLocation->redirect_code;
		return match;
	}

	//METHOD
	if (!checkMethodAllowed(*bestLocation, request)) {
		match.errorCode = 405;
		return match;
	}

	//PATH
	std::string path = resolveFilesystemPath(*bestLocation, request);
	if(!validatePath(path, bestLocation->root)) {
		match.errorCode = 404;
		return match;
	}
	match.path = path;

	//CGI
	std::string cgiInterpreter;
	if (isCGI(*bestLocation, request, cgiInterpreter)) {
		match.executeCGI = true;
		match.cgiInterpreter = cgiInterpreter;
	}


	match.errorCode = 0;
	return match;
}

const LocationConfig* HttpRouter::findBestLocation(const HttpRequest& request, const ServerConfig& serverConfig) {
	const std::vector<LocationConfig>& locations = serverConfig.getLocations();
	const std::string& requestPath = request.getPath();

	const LocationConfig* bestMatch = NULL; //nao temos match aind
	size_t bestMatchLength = 0;

	for (size_t i = 0; i < locations.size(); i++) {
		const LocationConfig& locationBlock = locations[i];
		const std::string& locationBlockPath = locationBlock.path;

		if (requestPath.compare(0, locationBlockPath.length(), locationBlockPath) == 0) {
			bool validPrefix = true;
			if(requestPath.length() > locationBlockPath.length()) {
				char nextChar = requestPath[locationBlockPath.length()];
				if (nextChar != '/' && nextChar != '?')
					validPrefix = false; //esta location path nao é um match valido
			}
			if (validPrefix && locationBlockPath.length() > bestMatchLength) {
				bestMatch = &locationBlock;
				bestMatchLength = locationBlockPath.length();
			}
		}
	}
	std::cout << "Best match: " << bestMatch->path << std::endl;
	return bestMatch;
}

bool HttpRouter::checkMethodAllowed(const LocationConfig& location, const HttpRequest& request) {
	const std::vector<std::string>& locMethods = location.methods;
	if(locMethods.empty())
		return true; //todos permitidos
	std::string methodStr = methodToString(request.getMethod());
	for (size_t i = 0; i < locMethods.size(); i++) {
		if (locMethods[i] == methodStr)
			return true;
	}
	return false;
}

bool HttpRouter::isCGI(const LocationConfig& location, const HttpRequest& request, std::string& cgiInterpreter) {
	const std::string& requestPath = request.getPath();
	if (location.cgi_ext.empty())
		return false;

	for (std::map<std::string, std::string>::const_iterator it = location.cgi_ext.begin(); it != location.cgi_ext.end(); ++it) {
		const std::string& extension = it->first;
		if (requestPath.length() >= extension.length()) {
			if (requestPath.substr(requestPath.length() - extension.length()) == extension) {
				cgiInterpreter = it->second;
				return true;
			}
		}
	}
	return false;
}

std::string HttpRouter::resolveFilesystemPath(const LocationConfig& location, const HttpRequest& request) {
	const std::string& root = location.root;
	const std::string& locationPath = location.path;
	const std::string& requestPath = request.getPath();

	std::cout << "[Router] root: '" << root << "', locationPath: '" << locationPath << "', requestPath: '" << requestPath << "'" << std::endl;

	std::string relativePath = requestPath.substr(locationPath.length());
	std::cout << "[Router] relativePath after strip: '" << relativePath << "'" << std::endl;
	if (!relativePath.empty() && relativePath[0] != '/')
		relativePath = "/" + relativePath;
	std::cout << "[Router] relativePath after ensure '/': '" << relativePath << "'" << std::endl;
	if (!root.empty() && root[root.length()-1] == '/' && !relativePath.empty() && relativePath[0] == '/') {
		std::cout << "[Router] Returning: '" << (root + relativePath.substr(1)) << "'" << std::endl;
		return root + relativePath.substr(1);
	}
	std::cout << "[Router] Returning: '" << (root + relativePath) << "'" << std::endl;
	return root + relativePath;
}

bool HttpRouter::validatePath(const std::string& path, const std::string& root) {
	std::cout << "[validatePath] path: '" << path << "', root: '" << root << "'" << std::endl;
	char realRoot[PATH_MAX];
	if (realpath(root.c_str(), realRoot) == NULL) {
		std::cout << "[validatePath] realpath(root) failed" << std::endl;
		return false;
	}
	std::string rootStr(realRoot);
	std::cout << "[validatePath] realRoot: '" << rootStr << "'" << std::endl;
	char realPath[PATH_MAX];
	if (realpath(path.c_str(), realPath) == NULL) {
		std::cout << "[validatePath] realpath(path) failed, returning true" << std::endl;
		return false;
	}
	std::string pathStr(realPath);
	std::cout << "[validatePath] realPath: '" << pathStr << "'" << std::endl;
	bool result = pathStr.compare(0, rootStr.length(), rootStr) == 0;
	std::cout << "[validatePath] result: " << result << std::endl;
	return result;
}

static std::string methodToString(HttpMethod method)
{
    switch (method)
    {
        case METHOD_GET:    return "GET";
        case METHOD_POST:   return "POST";
        case METHOD_DELETE: return "DELETE";
        case METHOD_HEAD:   return "HEAD";
        default:            return "";
    }
}