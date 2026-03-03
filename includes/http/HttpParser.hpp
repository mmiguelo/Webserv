#pragma once

#include <string>
#include "utils.hpp"

// to prevent memory abuse on header
// 8192 (8kb) is the default on NGINX
#define MAX_HEADER_SIZE 8192

enum ParserState {
    PARSE_REQUEST_LINE,
    PARSE_HEADERS,
    PARSE_BODY_CONTENT_LENGTH,
    PARSE_BODY_CHUNKED,
    PARSE_COMPLETE,
    PARSE_ERROR
};

class HttpParser {
    private:
        ParserState _state;
        std::string _buffer;
        HttpRequest _request;
        size_t _contentLength; //both have to be only positive
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
        size_t          getHeaderSize() const;
};