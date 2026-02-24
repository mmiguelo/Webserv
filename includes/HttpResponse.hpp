/* ************************************************************************** */
/*                                                                            */
/*   HttpResponse.hpp - PLACEHOLDER for Dev B                                 */
/*                                                                            */
/*   This header will hold the HTTP response builder. It should provide       */
/*   methods to set status code, headers, body, and serialize the full        */
/*   response into a string ready for send().                                 */
/*                                                                            */
/*   Dev B: replace this file with your real implementation.                  */
/*   Other devs can already #include "HttpResponse.hpp".                      */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HttpResponse
{
public:
    HttpResponse();
    ~HttpResponse();

    /* Dev B: implement these */
    // void        setStatus(int code, const std::string &reason);
    // void        setHeader(const std::string &key, const std::string &val);
    // void        setBody(const std::string &body);
    // std::string serialize() const;

private:
    int _statusCode;
    std::string _reason;
    std::map<std::string, std::string> _headers;
    std::string _body;
};

#endif
