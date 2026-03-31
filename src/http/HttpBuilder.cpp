#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include "HttpRouter.hpp"
#include "cgi.hpp"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>
#include "utils.hpp"
#include <sys/wait.h>

static std::string normalize(const std::string& name) {
    std::string result = name;
    for (size_t i = 0; i < result.size(); i++)
        result[i] = (result[i] == '-') ? '_' : std::toupper(result[i]);
    return result;
}

std::string HttpResponse::buildAutoIndex(const HttpRequest& request, const std::string& dirPath, ServerConfig &config) {

    DIR* dir = opendir(dirPath.c_str());
    if (!dir)
        return buildError(403, request, config);
    
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

    std::string templateHtml;
    if (!_readFile("www/html/autoindex.html", templateHtml) || templateHtml.empty()) {
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

std::string HttpResponse::buildFromDirectory(const HttpRequest& request, const std::string& dirPath, bool autoindex, const std::vector<std::string>& indexFiles, ServerConfig &config)
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

    // Try configured index files
    std::string base = dirPath;
    if (base[base.size() - 1] != '/')
        base += '/';

    const std::vector<std::string>& candidates = indexFiles.empty()
        ? std::vector<std::string>(1, "index.html")
        : indexFiles;

    for (size_t i = 0; i < candidates.size(); i++) {
        std::string indexPath = base + candidates[i];
        struct stat indexSt;
        if (stat(indexPath.c_str(), &indexSt) == 0) {
            int indexResult = checkFile(indexSt);
            if (indexResult == 200)
                return buildFromFile(request, indexPath, indexResult, config);
        }
    }

    if (autoindex)
        return buildAutoIndex(request, dirPath, config);

    // No index found: 404 if an explicit index was configured but missing, 403 if no index configured
    return buildError(indexFiles.empty() ? 403 : 404, request, config);
}

std::string HttpResponse::buildFromFile(const HttpRequest& request, const std::string& filePath, int checkResult, ServerConfig &config) {

    _version = request.getVersion();
    if (_version.empty())
        _version = "HTTP/1.1";

    std::string path = filePath;

    // If path ends with /, try to serve index.html
    if (!path.empty() && path[path.size() - 1] == '/')
        path += "index.html";

    if (checkResult != 200)
        return buildError(checkResult, request, config);
    if (!_readFile(path, _body))
        return buildError(403, request, config);

    _contentType = _getMimeType(path);
    _statusCode = 200;
    return serialize(request.getMethod());
}

std::string HttpResponse::handleDelete(const HttpRequest& request, const std::string& path, int checkResult, ServerConfig &config) {
    _version = request.getVersion();
    if (_version.empty())
        _version = "HTTP/1.1";
    
    if (checkResult == 300) {
        if (rmdir(path.c_str()) != 0)
            return buildError(403, request, config); // Forbidden
    }
    else if (checkResult == 200) {
        if (unlink(path.c_str()) != 0)
            return buildError(403, request, config); // Forbidden
    }
    else
        return buildError(checkResult, request, config);

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
std::string HttpResponse::handleUpload(const HttpRequest& request, const std::string& uploadDir, ServerConfig &config)
{
    _version = request.getVersion();
    if (_version.empty())
        _version = "HTTP/1.1";

    std::string rawContentType = request.getHeader("content-type");
    std::string contentType = toLowerStr(rawContentType);
    std::string uploadBase = uploadDir;

    if (!uploadBase.empty() && uploadBase[uploadBase.size() - 1] != '/')
        uploadBase += '/';

    std::string uploadedFilename;

    if (contentType.find("multipart/form-data") != std::string::npos)
    {
        size_t boundaryPos = contentType.find("boundary=");
        if (boundaryPos == std::string::npos)
            return buildError(400, request, config);

        std::string boundary = rawContentType.substr(boundaryPos + 9);
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
                    return buildError(403, request, config);
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
            return buildError(403, request, config);
        uploadedFilename = filename;
    }

    if (uploadedFilename.empty())
        return buildError(400, request, config);

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

std::string HttpResponse::handleCgi(const HttpRequest& request, ServerConfig &config, HttpRouteMatch& match, EpollClient *client) {

    Cgi cgi;

    std::ostringstream oss;
    oss << config.getPort(); // para ja retorna apenas _listen[0]
    cgi.set("REQUEST_METHOD", methodToString(request.getMethod()));
    cgi.set("QUERY_STRING", request.getQuery());
    cgi.set("CONTENT_TYPE", request.getHeader("content-type"));
    cgi.set("CONTENT_LENGTH", request.getHeader("content-length"));
    cgi.set("SCRIPT_FILENAME", match.path); //file path from root to filename
    cgi.set("PATH_INFO", request.getPath()); //URL path
    const std::vector<std::string>& name = config.getServerName();
    cgi.set("SERVER_NAME", name.empty() ? "" : name[0]);
    cgi.set("SERVER_PORT", oss.str());
    cgi.set("SERVER_PROTOCOL", request.getVersion());
    cgi.set("SERVER_SOFTWARE", "webserv/1.0");
    cgi.set("GATEWAY_INTERFACE", "CGI/1.1");

    const std::map<std::string, std::string> &headers = request.getAllHeaders();

    std::map<std::string, std::string>::const_iterator it;
    for (it = headers.begin(); it != headers.end(); ++it)
        cgi.set("HTTP_" + normalize(it->first), it->second);

    char **envp = cgi.getEnv();

    int stdin_pipe[2];
    int stdout_pipe[2];
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1) {
            cgi.freeEnv(envp);
            return buildError(500, request, config);
    }

    pid_t pid = fork();
    if (pid < 0) {
        cgi.freeEnv(envp);
        return buildError(500, request, config);
    }
    if (pid == 0)
    {
        if (dup2(stdin_pipe[0], STDIN_FILENO) == -1) {
            cgi.freeEnv(envp);
            exit(1);
        }
        if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
            cgi.freeEnv(envp);
            exit(1);
        }
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        char* argv[] = {(char*)match.cgiInterpreter.c_str(), (char*)match.path.c_str(), NULL};
        execve(match.cgiInterpreter.c_str(), argv, envp);
        exit(1);
    }
    cgi.freeEnv(envp);
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    client->startCgi(pid, stdin_pipe[1], stdout_pipe[0], request.getBody());
    return std::string();
}

std::string HttpResponse::parseCgiOutput(const std::string& output, const HttpRequest& request, ServerConfig& config) {
    size_t headerEnd = output.find("\r\n\r\n");
    size_t sep = 4;
    if (headerEnd == std::string::npos) {
        headerEnd = output.find("\n\n");
        sep = 2;
    }
    if (headerEnd == std::string::npos)
        return buildError(500, request, config);

    std::string headerBlock = output.substr(0, headerEnd);
    std::string body = output.substr(headerEnd + sep);

    // Parse CGI headers: extract Status: and collect the rest
    int statusCode = 200;
    std::string statusMsg = "OK";
    std::string filteredHeaders;
    bool hasContentLength = false;
    std::istringstream ss(headerBlock);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty()) continue;
        std::string lower = line;
        for (size_t i = 0; i < lower.size(); i++)
            lower[i] = static_cast<char>(std::tolower(lower[i]));
        if (lower.compare(0, 7, "status:") == 0) {
            std::string val = line.substr(7);
            size_t start = val.find_first_not_of(" \t");
            if (start != std::string::npos) {
                statusCode = std::atoi(val.c_str() + start);
                size_t sp = val.find(' ', start);
                statusMsg = (sp != std::string::npos) ? val.substr(sp + 1) : getStatusMessage(statusCode);
            }
        } else {
            if (lower.compare(0, 15, "content-length:") == 0)
                hasContentLength = true;
            if (!filteredHeaders.empty())
                filteredHeaders += "\r\n";
            filteredHeaders += line;
        }
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode << " " << statusMsg << "\r\n";
    if (!filteredHeaders.empty())
        oss << filteredHeaders << "\r\n";
    if (!hasContentLength)
        oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: keep-alive\r\n";
    oss << "\r\n";
    if (request.getMethod() != METHOD_HEAD)
        oss << body;
    return oss.str();
}

std::string HttpResponse::buildError(int statusCode, const HttpRequest& request, ServerConfig &config) {
    _version = request.getVersion().empty() ? "HTTP/1.1" : request.getVersion();
    _statusCode = statusCode;
    _contentType = "text/html";

    // Convert status code to string
    std::ostringstream codeStr;
    codeStr << statusCode;

    // Try to read template file
    std::string templateHtml;
    if (!_readFile(config.getErrorPage(_statusCode), templateHtml)) {
        std::ostringstream oss;
        oss << "<!DOCTYPE html>\n<html>\n<head><title>"
            << statusCode << " " << getStatusMessage(statusCode)
            << "</title></head>\n<body>\n<h1>"
            << statusCode << " " << getStatusMessage(statusCode)
            << "</h1>\n</body>\n</html>";
        _body = oss.str();
    }
    else {
        templateHtml = replaceAll(templateHtml, "{{ERROR_CODE}}", codeStr.str());
        templateHtml = replaceAll(templateHtml, "{{ERROR_DESCRIPTION}}", getStatusMessage(statusCode));
        templateHtml = replaceAll(templateHtml, "{{ERROR_MESSAGE}}", getErrorMessage(statusCode));
        _body = templateHtml;
    }
    return serialize(request.getMethod());
}