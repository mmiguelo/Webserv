/* ************************************************************************** */
/*                                                                            */
/*   HttpRequest.hpp - PLACEHOLDER for Dev B                                  */
/*                                                                            */
/*   This header will hold the HTTP request parser. It should be able to      */
/*   incrementally consume raw bytes and signal when a complete request        */
/*   (headers + body) has been received.                                      */
/*                                                                            */
/*   Dev B: replace this file with your real implementation.                  */
/*   Other devs can already #include "HttpRequest.hpp".                       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

class HttpRequest
{
public:
    HttpRequest();
    ~HttpRequest();

    /* Dev B: implement these */
    // void        feed(const char *data, size_t len);
    // bool        isComplete() const;
    // std::string getMethod() const;
    // std::string getUri() const;
    // std::string getVersion() const;
    // std::string getHeader(const std::string &key) const;
    // std::string getBody() const;

private:
    std::string _method;
    std::string _uri;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;
    bool _complete;
};

#endif
