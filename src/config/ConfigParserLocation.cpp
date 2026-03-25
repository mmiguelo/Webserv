#include "ConfigParser.hpp"
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"
#include "utils.hpp"
#include <stdexcept>
#include <iostream>

void ConfigParser::parseLocation(ServerConfig &serverBlock)
{
	LocationConfig location;

	location.path = normalizePath(expectWord());
	expect(LBRACE);
	while (!isEnd() && peek().type != RBRACE)
	{
		if (matchWord("root"))
		parseLocationRoot(location);
		else if (matchWord("upload_dir"))
			parseUploadDir(location);
		else if (matchWord("autoindex"))
			parseAutoindex(location);
		else if (matchWord("methods"))
			parseLocationMethods(location);
		else if (matchWord("client_max_body_size"))
			parseLocationClientMaxBodySize(location);
		else if (matchWord("cgi_ext"))
			parseCgiExt(location);
		else if (matchWord("index"))
			parseLocationIndex(location);
		else if (matchWord("error_page"))
			parseLocationError(location);
		else if (matchWord("return"))
			parseReturn(location);
		else
			throw parseError("Unexpected directive in location block");
	}
	expect(RBRACE);
	if (location.root.empty())
			location.root = serverBlock.getRoot();
	serverBlock.addLocation(location);
}
