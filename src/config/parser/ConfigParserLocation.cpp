#include "config/ConfigParser.hpp"
#include "config/ServerConfig.hpp"
#include "config/LocationConfig.hpp"
#include <stdexcept>

void ConfigParser::parseLocation(ServerConfig& serverBlock)
{
	LocationConfig location;

	location.path = expectWord();
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
		else if (matchWord("cgi_ext"))
			parseCgiExt(location);
		else if (matchWord("return"))
			parseReturn(location);
		else
			throw parseError("Unexpected directive in location block");
	}
	expect(RBRACE);
	serverBlock.locations.push_back(location);
}