#pragma once

#include <istream>
#include <iostream>
#include <string>
#include <map>
#include <cstdlib>
#include <cctype>
#include <sstream>
#include <vector>
#include <sstream>

/*=============================================================================#
#                                   ENUMS                                      #
#=============================================================================*/

enum HttpMethod {
    METHOD_GET,
    METHOD_POST,
    METHOD_DELETE,
    METHOD_HEAD,
    METHOD_UNKNOWN
};

enum HttpStatusCode {
    STATUS_CONTINUE                     = 100,
    STATUS_SWITCHING_PROTOCOLS          = 101,
    STATUS_OK                           = 200,
    STATUS_CREATED                      = 201,
    STATUS_NO_CONTENT                   = 204,
    STATUS_MOVED_PERMANENTLY            = 301,
    STATUS_FOUND                        = 302,
    STATUS_NOT_MODIFIED                 = 304,
    STATUS_BAD_REQUEST                  = 400,
    STATUS_FORBIDDEN                    = 403,
    STATUS_NOT_FOUND                    = 404,
    STATUS_METHOD_NOT_ALLOWED           = 405,
    STATUS_REQUEST_HEADER_TOO_LARGE     = 431,
    STATUS_INTERNAL_SERVER_ERROR        = 500,
    STATUS_HTTP_VERSION_NOT_SUPPORTED   = 505,
    STATUS_SERVICE_UNAVAILABLE          = 503
};

enum ParserState {
    PARSE_REQUEST_LINE,
    PARSE_HEADERS,
    PARSE_BODY_CONTENT_LENGTH,
    PARSE_BODY_CHUNKED,
    PARSE_COMPLETE,
    PARSE_ERROR
};

/*=============================================================================#
#                              UTILITY FUNCTIONS                               #
#=============================================================================*/

std::string         toLowerStr(const std::string& str);
std::string         trimWhitespace(const std::string& str);
HttpMethod          stringToMethod(const std::string& method);
std::string         methodToString(HttpMethod method);