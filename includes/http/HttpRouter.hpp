#ifndef HTTPROUTER_HPP
#define HTTPROUTER_HPP

#include "HttpRouteMatch.hpp"
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"

class HttpRouter {
	private:
		static const LocationConfig* findBestLocation(const HttpRequest& request, const ServerConfig& serverConfig); //função para encontrar a melhor location para o request
		static std::string resolveFilesystemPath(const LocationConfig& location, const HttpRequest& request);
		static bool validatePath(const std::string& path, const std::string& root);
		static bool isCGI (const LocationConfig& location, const std::string& path, std::string& cgiInterpreter); //função para verificar se o request deve ser tratado como CGI
		static bool checkMethodAllowed(const LocationConfig& location, const HttpRequest& request); //função para verificar se o método do request é permitido na location
		static bool isFileExecutable(const std::string& path); //função para verificar se o arquivo é executável, usada para CGI
	public:
		HttpRouteMatch route(const HttpRequest& request, const ServerConfig& serverConfig); //a struct vai ser preenchida aqui, recebendo uma ref de um Request e d um ServerConfig, e vai ser retornada para o HttpHandler, que vai usar a struct para decidir como tratar o request
};

#endif