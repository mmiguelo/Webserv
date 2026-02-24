#pragma once

#include "HttpRequest.hpp"
#include "utils.hpp"

#define MAX_HEADER_SIZE 8192

class HttpParser {
    private:
        ParserState _state;
        std::string _buffer;
        HttpRequest _request;
        size_t _contentLength;
        size_t _headerSize;
    
        bool _parseRequestLine();
        bool _parseHeaders();
        bool _parseBodyContentLength();
        bool _parseBodyChunked();

    public:
        HttpParser();
        ~HttpParser();
        HttpParser(const HttpParser& other);
        HttpParser& operator=(const HttpParser& other);

        // Feed raw data into the parser. Returns true when request is COMPLETE.
        bool            feed(const std::string& data);
        void            reset();

        ParserState     getState() const;
        HttpRequest&    getRequest();
        const HttpRequest& getRequest() const;
};