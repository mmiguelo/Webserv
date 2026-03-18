#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include "HttpRouter.hpp"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sstream>
#include <unistd.h>
#include <algorithm>
#include "utils.hpp"

//define the static member
std::map<int, std::string> HttpResponse::_codeMsg;

void HttpResponse::initCodeMsg() {
    if (!_codeMsg.empty())
        return;

    _codeMsg[100] = "Continue";
    _codeMsg[101] = "Switching Protocols";
    _codeMsg[200] = "OK";
    _codeMsg[201] = "Created";
    _codeMsg[204] = "No Content";
    _codeMsg[301] = "Moved Permanently";
    _codeMsg[302] = "Found";
    _codeMsg[304] = "Not Modified";
    _codeMsg[400] = "Bad Request";
    _codeMsg[403] = "Forbidden";
    _codeMsg[404] = "Not Found";
    _codeMsg[405] = "Method Not Allowed";
    _codeMsg[408] = "Request Timeout";
    _codeMsg[413] = "Payload Too Large";
    _codeMsg[414] = "Uri Too Long";
    _codeMsg[431] = "Request Header Too Large";
    _codeMsg[500] = "Internal Server Error";
    _codeMsg[501] = "Not Implemented";
    _codeMsg[503] = "Service Unavailable";
    _codeMsg[504] = "Gateway Timeout";
    _codeMsg[505] = "HTTP Version Not Supported";
}

HttpResponse::HttpResponse()
    :   _statusCode(200),
        _body(""),
        _contentType(""),
           _version(""),
           _connection("keep-alive") {
    initCodeMsg();
}

HttpResponse::~HttpResponse() {}

HttpResponse::HttpResponse(const HttpResponse& other)
    :   _statusCode(other._statusCode),
        _body(other._body),
        _contentType(other._contentType) {}

HttpResponse& HttpResponse::operator=(const HttpResponse& other) {
    if (this != &other) {
        _codeMsg = other._codeMsg;
        _statusCode = other._statusCode;
        _body = other._body;
        _contentType = other._contentType;
    }
    return *this;
}

std::string HttpResponse::_getMimeType(const std::string& path)
{
    size_t dot = path.rfind('.');
    if (dot == std::string::npos)
        return "application/octet-stream";
    
    std::string ext = path.substr(dot);
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".xml") return "application/xml";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".mp4") return "video/mp4";
    return "application/octet-stream";
}

bool HttpResponse::_fileExists(const std::string& path)
{
    std::ifstream file(path.c_str());
    return file.good();
}

std::string HttpResponse::_sanitizeFilename(const std::string& filename)
{
    size_t slashPos = filename.rfind('/');
    if (slashPos == std::string::npos)
        return filename;
    return filename.substr(slashPos + 1);
}

bool HttpResponse::_writeBinaryFile(const std::string& path, const std::string& data)
{
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        return false;
    out.write(data.data(), data.size());
    return out.good();
}

void HttpResponse::build(int statusCode, const std::string& body, const std::string& contentType, const std::string& version) {
    _statusCode = statusCode;
    _body = body;
    _contentType = contentType;
    _version = version;
}

std::string HttpResponse::replaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
    return str;
}

bool HttpResponse::_readFile(const std::string& path, std::string& out) const {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

int HttpResponse::checkFile(const struct stat& st) const
{    
    if(S_ISDIR(st.st_mode))
        return 300; // Special code to indicate it's a directory

    if (!(S_ISREG(st.st_mode))) //is not a reg file
        return 403;

    return 200; // OK
}

std::string HttpResponse::buildAutoIndex(const HttpRequest& request, const std::string& dirPath) {

    DIR* dir = opendir(dirPath.c_str());
    std::cout << "Building autoindex for directory: " << dirPath << std::endl;
    if (!dir)
        return buildError(403, request);
    
    std::vector<std::string> entries;
    struct dirent* entry; //this struct holds info about one directory entry. for example d_name (the filename)
    
    while ((entry = readdir(dir)) != NULL) 
    //readdir returns NULL when there are no more entries and it reads one entry each call
    {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;
        // skips the current and parent directory entries
        
        struct stat st;
        if (stat((dirPath + "/" + name).c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            entries.push_back(name + "/");
        else
            entries.push_back(name);
    }
    closedir(dir);

    std::sort(entries.begin(), entries.end());

    const std::string& uriPath = request.getPath();
    std::ostringstream fileList;

    if (uriPath != "/")
        fileList << "<a href=\"../\">../</a>\n";
    // Add a "go up" link to the parent directory
    // Only if we're not already at the root — root has no parent to go to

    for (size_t i = 0; i < entries.size(); i++) {
        std::string name = entries[i];
        std::string href = uriPath + name;
        fileList << "<a href=\"" << href << "\">" << name << "</a>\n";
    }

    for (size_t i = 0; i < entries.size(); i++)
        std::cout << entries[i] << std::endl;

    std::string templateHtml;
    if (!_readFile("www/html/autoindex.html", templateHtml)) {
        templateHtml = "<!DOCTYPE html><html><body>"
                       "<h1>Index of {{URI_PATH}}</h1><hr><pre>"
                       "{{FILE_LIST}}"
                       "</pre><hr></body></html>";
    }

    templateHtml = replaceAll(templateHtml, "{{URI_PATH}}", uriPath);
    templateHtml = replaceAll(templateHtml, "{{FILE_LIST}}", fileList.str());

    _statusCode = 200;
    _body = templateHtml;
    _contentType = "text/html; charset=utf-8";
    _version = request.getVersion();
    if (_version.empty())
        _version = "HTTP/1.1";
    
    return serialize(request.getMethod());
}

std::string HttpResponse::buildFromDirectory(const HttpRequest& request, const std::string& dirPath, bool autoindex)
{
    _version = request.getVersion();
    if (_version.empty())
        _version = "HTTP/1.1";

    const std::string& uriPath = request.getPath();

    // Redirect to slash-appended URI for directories
    if (uriPath.empty() || uriPath[uriPath.size() - 1] != '/')
    {
        _statusCode = 301;
        _body.clear();
        _contentType.clear();
        setLocation(uriPath + "/");
        return serialize(request.getMethod());
    }

    // Try to serve index.html inside dir
    std::string indexPath = dirPath;
    if (indexPath[indexPath.size() - 1] != '/')
        indexPath += '/';
    indexPath += "index.html";

    struct stat indexSt;

    if (stat(indexPath.c_str(), &indexSt) == 0)
    {
        int indexResult = checkFile(indexSt);
        if (indexResult == 200)
            return buildFromFile(request, indexPath, indexResult);
    }
    
    if (autoindex)
        return buildAutoIndex(request, dirPath);

    // No index.html and no autoindex support here yet
    return buildError(403, request);
}

std::string HttpResponse::buildFromFile(const HttpRequest& request, const std::string& filePath, int checkResult) {

    _version = request.getVersion();
    if (_version.empty())
        _version = "HTTP/1.1";

    std::string path = filePath;

    // If path ends with /, try to serve index.html
    if (!path.empty() && path[path.size() - 1] == '/')
        path += "index.html";

    if (checkResult != 200)
        return buildError(checkResult, request);
    if (!_readFile(path, _body))
        return buildError(403, request);

    _contentType = _getMimeType(path);
    _statusCode = 200;
    return serialize(request.getMethod());
}

std::string HttpResponse::handleDelete(const HttpRequest& request, const std::string& path, int checkResult) {
    _version = request.getVersion();
    if (_version.empty())
        _version = "HTTP/1.1";
    
    if (checkResult == 300) {
        if (rmdir(path.c_str()) != 0)
            return buildError(403, request); // Forbidden
    }
    else if (checkResult == 200) {
        if (unlink(path.c_str()) != 0)
            return buildError(403, request); // Forbidden
    }
    else
        return buildError(checkResult, request);

    _statusCode = 204; // No Content
    _body.clear();
    _contentType.clear();
    return serialize(request.getMethod());
}

/* POST /upload HTTP/1.1
Host: localhost:8080
Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryABC123
Content-Length: 200

------WebKitFormBoundaryABC123
Content-Disposition: form-data; name="file"; filename="hello.txt"
Content-Type: text/plain

Hello from webserv!
------WebKitFormBoundaryABC123-- */
std::string HttpResponse::handleUpload(const HttpRequest& request, const std::string& uploadDir)
{
    _version = request.getVersion();
    if (_version.empty())
        _version = "HTTP/1.1";

    std::string contentType = toLowerStr(request.getHeader("content-type"));
    std::string uploadBase = uploadDir;
    if (!uploadBase.empty() && uploadBase[uploadBase.size() - 1] != '/')
        uploadBase += '/';

    std::string uploadedFilename;

    if (contentType.find("multipart/form-data") != std::string::npos)
    {
        size_t boundaryPos = contentType.find("boundary=");
        if (boundaryPos == std::string::npos)
            return buildError(400, request);

        std::string boundary = contentType.substr(boundaryPos + 9);
        size_t semicolon = boundary.find(';');
        if (semicolon != std::string::npos)
            boundary = boundary.substr(0, semicolon);
        boundary = trimWhitespace(boundary);
        if (!boundary.empty() && boundary[0] == '"' && boundary[boundary.size() - 1] == '"')
            boundary = boundary.substr(1, boundary.size() - 2);

        std::string marker = "--" + boundary;
        const std::string& body = request.getBody();
        size_t pos = body.find(marker);
        while (pos != std::string::npos)
        {
            pos += marker.size();
            if (pos + 2 <= body.size() && body.compare(pos, 2, "--") == 0)
                break;
            if (pos + 2 <= body.size() && body.compare(pos, 2, "\r\n") == 0)
                pos += 2;

            size_t nextBoundary = body.find(marker, pos);
            if (nextBoundary == std::string::npos)
                break;

            std::string part = body.substr(pos, nextBoundary - pos);
            size_t headerEnd = part.find("\r\n\r\n");
            if (headerEnd == std::string::npos)
            {
                pos = nextBoundary;
                continue;
            }

            std::string headerBlock = part.substr(0, headerEnd);
            std::string partBody = part.substr(headerEnd + 4);
            if (partBody.size() >= 2 && partBody.compare(partBody.size() - 2, 2, "\r\n") == 0)
                partBody.erase(partBody.size() - 2);

            std::string disposition;
            std::istringstream headerStream(headerBlock);
            std::string line;
            while (std::getline(headerStream, line)) //iterates over header lines
            {
                if (!line.empty() && line[line.size() - 1] == '\r')
                    line.erase(line.size() - 1); //getline removes \n but leaves \r
                std::string lower = toLowerStr(line);
                if (lower.find("content-disposition:") == 0)
                    disposition = line;
            }

            size_t filenamePos = disposition.find("filename=");
            if (filenamePos == std::string::npos)
            {
                pos = nextBoundary;
                continue;
            }
            filenamePos += 9;
            std::string filename = disposition.substr(filenamePos);
            filename = trimWhitespace(filename);
            if (!filename.empty() && filename[0] == '"')
            {
                size_t endQuote = filename.find('"', 1);
                if (endQuote != std::string::npos)
                    filename = filename.substr(1, endQuote - 1);
            }
            else
            {
                size_t endSemi = filename.find(';');
                if (endSemi != std::string::npos)
                    filename = filename.substr(0, endSemi);
            }

            filename = _sanitizeFilename(filename);
            if (!filename.empty())
            {
                std::string outPath = uploadBase + filename;
                if (_writeBinaryFile(outPath, partBody))
                    uploadedFilename = filename;
                else
                    return buildError(403, request);
            }

            pos = nextBoundary;
        }
    }
    else
    {
        std::string path = request.getPath();
        if (!path.empty() && path[path.size() - 1] == '/')
            path.erase(path.size() - 1);
        size_t slashPos = path.rfind('/');
        std::string filename;

        if (slashPos == std::string::npos)
            filename = path;
        else
            filename = path.substr(slashPos + 1);
        filename = _sanitizeFilename(filename);
        if (filename.empty())
            filename = "upload.bin";

        std::string outPath = uploadBase + filename;
        if (!_writeBinaryFile(outPath, request.getBody()))
            return buildError(403, request);
        uploadedFilename = filename;
    }

    if (uploadedFilename.empty())
        return buildError(400, request);

    std::string location = request.getPath();
    if (!location.empty() && location[location.size() - 1] != '/')
        location += "/";
    location += uploadedFilename;

    _statusCode = 201;
    _body.clear();
    _contentType.clear();
    setLocation(location);
    return serialize(request.getMethod());
}

std::string HttpResponse::buildError(int statusCode, const HttpRequest& request) {
    _version = request.getVersion().empty() ? "HTTP/1.1" : request.getVersion();
    _statusCode = statusCode;
    _contentType = "text/html";

    // Convert status code to string
    std::ostringstream codeStr;
    codeStr << statusCode;

    // Try to read template file
    std::string templateHtml;
    if (!_readFile("www/html/errors/default.html", templateHtml)) {
        // Protecao caso alguem apague a pasta? nao sei se isto é necessario, mas é melhor do que retornar uma resposta vazia
        std::ostringstream oss;
        oss << "<!DOCTYPE html>\n<html>\n<head><title>"
            << statusCode << " " << getStatusMessage(statusCode)
            << "</title></head>\n<body>\n<h1>"
            << statusCode << " " << getStatusMessage(statusCode)
            << "</h1>\n</body>\n</html>";
        _body = oss.str();
    }
    else {
        // Replace variables with actual values
        templateHtml = replaceAll(templateHtml, "{{ERROR_CODE}}", codeStr.str());
        templateHtml = replaceAll(templateHtml, "{{ERROR_MESSAGE}}", getStatusMessage(statusCode));
        _body = templateHtml;
    }
    return serialize(request.getMethod());
}

std::string HttpResponse::serialize(HttpMethod method) const {
    std::ostringstream oss;

    const std::string httpVersion = _version.empty() ? "HTTP/1.1" : _version;
    oss << httpVersion << " " << _statusCode << " " << getStatusMessage(_statusCode) << "\r\n";
    oss << "Server: WebServ\r\n";
    oss << "Date: " << httpDate() << "\r\n";
    if (!_contentType.empty())
        oss << "Content-Type: " << _contentType << "\r\n";

    if (!_location.empty())
        oss << "Location: " << _location << "\r\n";
    if (!_allow.empty())
        oss << "Allow: " << _allow << "\r\n";

    if (hasBody()) // No Content should not have a body
    {
        oss << "Content-Length: " << _body.size() << "\r\n";
        if (!_connection.empty())
            oss << "Connection: " << _connection << "\r\n";
    }
    oss << "\r\n";

    if (hasBody() && method != METHOD_HEAD)
        oss << _body;
    return oss.str();
}

const std::string& HttpResponse::getStatusMessage(int code) const {
    
    std::map<int, std::string>::const_iterator it;
    it = _codeMsg.find(code);
    if (it != _codeMsg.end())
        return it->second;
    static std::string unknown = "Unknown Status";
    return unknown;
}

std::string HttpResponse::httpDate() const {
    char buffer[100];
    time_t now = time(NULL);
    struct tm* gmt = gmtime(&now);

    /*format is: "Mon, 02 Mar 2026 00:00:00 GMT"
    %a - abbreviated weekday name
    %d - day of the month
    %b - abbreviated month name
    %Y - year
    %H - hour (24h)
    %M - minute
    %S - second
    */
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return std::string(buffer);
}

bool HttpResponse::hasBody() const {
    return !(_statusCode == 204 || _statusCode == 304);
}

std::string HttpResponse::getVersion() const {
    return _version;
}

void HttpResponse::setLocation(const std::string& url) {
    _location = url;
}

void HttpResponse::setAllow(const std::vector<std::string>& methods) {
    _allow.clear();
    for (size_t i = 0; i < methods.size(); i++) {
        if (i > 0)
            _allow += ", ";
        _allow += methods[i];
    } //TODO: isto é para o teste 3 da task 2.3. Precisa de ser revisto
}

int HttpResponse::getStatusCode() const {
    return _statusCode;
}

void HttpResponse::setConnection(const std::string& conn) {
    _connection = conn;
}
