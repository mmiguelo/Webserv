#ifndef HTTPROUTEMATCH_HPP
#define HTTPROUTEMATCH_HPP

#include <string>
#include "LocationConfig.hpp"

struct HttpRouteMatch {
	const LocationConfig* location; //pointer pq pode ser nulo, ua referencia tem que ser inicializada com base num objeto
	std::string path; //o resolved filesystem path
	bool executeCGI; //boolean para indicar se o request deve ser tratado como CGI
	std::string cgiInterpreter; //se executeCGI for true, aqui fica o caminho do interpretador CGI a usar
	int errorCode; //0 se não houver erro, ou o código de erro HTTP a retornar
	std::string redirectTarget; //se for um redirect, aqui fica a URL de destino

	HttpRouteMatch() : location(NULL), executeCGI(false), errorCode(0) {};
};

#endif