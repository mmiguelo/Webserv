#include "HttpRouter.hpp"
#include <limits.h>
#include <stdlib.h>
#include "utils.hpp"

HttpRouteMatch HttpRouter::route(const HttpRequest& request, const ServerConfig& serverConfig) {
	HttpRouteMatch match;

	//LOCATION
	const LocationConfig* bestLocation = findBestLocation(request, serverConfig);
	if (!bestLocation) {
		match.errorCode = 404; // Not Found
		return match;
	}
	match.location = bestLocation;

	// URI TOO LONG
	if (request.getMethod() == METHOD_POST
		&& match.location->has_client_max_body_size &&
		request.getBody().size() > match.location->client_max_body_size) {
		match.errorCode = 413;
		return match;
	}

	//REDIRECT
	if(bestLocation->has_redirect) {
		match.errorCode = bestLocation->redirect_code;
		match.redirectTarget = bestLocation->redirect_url;
		return match;
	}

	//METHOD
	if (!checkMethodAllowed(*bestLocation, request)) {
		match.errorCode = 405;
		return match;
	}

	//PATH
	std::string effectiveRoot = bestLocation->upload_dir.empty() ? bestLocation->root : bestLocation->upload_dir;
	std::string path = resolveFilesystemPath(*bestLocation, request);
	if(!validatePath(path, effectiveRoot)) {
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
	match.autoindex = bestLocation->autoindex;
	match.upload_dir = bestLocation->upload_dir;
	match.errorCode = 0;
	return match;
}

const LocationConfig* HttpRouter::findBestLocation(const HttpRequest& request, const ServerConfig& serverConfig) {
	const std::vector<LocationConfig>& locations = serverConfig.getLocations();
	const std::string& requestPath = request.getPath();

	std::cout << "path: " << requestPath << std::endl;

	const LocationConfig* bestMatch = NULL; //nao temos match aind
	size_t bestMatchLength = 0;

	for (size_t i = 0; i < locations.size(); i++) {
		const LocationConfig& locationBlock = locations[i];
		const std::string& locationBlockPath = locationBlock.path;

		if (requestPath.compare(0, locationBlockPath.length(), locationBlockPath) == 0) {
			bool validPrefix = true;
			// Root location should match any path that starts with '/'
			if (locationBlockPath != "/" && requestPath.length() > locationBlockPath.length()) {
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
	const std::string& root = location.upload_dir.empty() ? location.root : location.upload_dir;
	const std::string& locationPath = location.path;
	const std::string& requestPath = request.getPath();

	std::string relativePath = requestPath.substr(locationPath.length());
	if (relativePath.empty())
        relativePath = "/";

	if (!relativePath.empty() && relativePath[0] == '/')
		relativePath.erase(0, 1);

	std::string fullPath = root;
	if (!fullPath.empty() && fullPath[fullPath.length() - 1] != '/')
		fullPath += '/';

	fullPath += relativePath;
	return fullPath;
}

bool HttpRouter::validatePath(const std::string& path, const std::string& root) {
    char realRoot[PATH_MAX];
    if (realpath(root.c_str(), realRoot) == NULL)
        return false;
    std::string rootStr(realRoot);
    char realPath[PATH_MAX];
    if (realpath(path.c_str(), realPath) == NULL)
        return true;
    std::string pathStr(realPath);
    return pathStr.compare(0, rootStr.length(), rootStr) == 0;
}
