*This project has been created as part of the 42 curriculum by ddias-fe, mmiguelo and pmachado.*

# Webserv

## Description

Webserv is a lightweight HTTP/1.1 web server written in C++98, built from scratch as part of the 42 school curriculum. The goal of the project is to gain a deep understanding of how web servers work by implementing one — from raw socket handling to HTTP request parsing, response generation, and CGI execution.

The server is designed around a non-blocking, event-driven architecture using Linux's `epoll` system call, allowing it to handle multiple simultaneous client connections efficiently without spawning threads or processes per connection.

**Key capabilities:**
- HTTP/1.1 compliant request/response handling
- Non-blocking I/O with `epoll`
- Support for GET, POST, and DELETE methods
- Static file serving with MIME type detection
- Automatic directory listing (autoindex)
- File uploads and deletion
- Custom error pages
- HTTP redirects
- CGI execution (e.g., Python scripts, custom binaries)
- Nginx-inspired configuration file syntax
- Multiple virtual servers on different host/port combinations

---

## Instructions

### Compilation

```bash
# Clone the repository
git clone <repo-url>
cd Webserv

# Build the binary
make

# Clean object files
make clean

# Full clean (objects + binary)
make fclean

# Rebuild from scratch
make re

# Run with Valgrind (memory leak check)
make val
```

The resulting binary is named `webserv`.

### Execution

```bash
./webserv [config_file]
```

If no configuration file is provided, the server will look for a default config. It is recommended to always specify a config explicitly:

```bash
./webserv config/default.conf
```

---

### Configuration

The server is configured through a `.conf` file that uses an Nginx-inspired block syntax. The file is composed of one or more `server` blocks, each of which may contain `location` sub-blocks.

#### Basic Structure

```nginx
server {
    listen 8080;
    root www/;
    server_name example.com;

    location / {
        methods GET;
        index index.html;
    }
}
```

#### Comments

Lines beginning with `#` are treated as comments and ignored by the parser.

---

### Directives Reference

#### Server-Level Directives

These directives are set inside a `server { }` block and apply to the entire virtual server unless overridden at the location level.

---

##### `listen`
**Required.** Defines the host and port the server listens on. Multiple `listen` directives are allowed per server block.

```nginx
listen 8080;
listen localhost:9090;
listen 127.0.0.1:8080;
```

- Port must be in the range 1–65535.
- If only a port is given, the host defaults to `0.0.0.0` (all interfaces).
- Each `host:port` combination must be unique across all server blocks.

---

##### `root`
**Required.** Sets the base directory from which files are served.

```nginx
root www/;
```

- Can be a relative or absolute path.
- Overridable at the location level.

---

##### `server_name`
**Optional.** Associates one or more domain names with this server block. Informational — used for documentation and logging purposes.

```nginx
server_name example.com www.example.com;
```

---

##### `methods`
**Optional.** Restricts the HTTP methods accepted by this server. If omitted, all methods are allowed.

```nginx
methods GET POST DELETE;
```

- Supported methods: `GET`, `POST`, `DELETE`.
- Overridable at the location level.

---

##### `client_max_body_size`
**Optional.** Sets the maximum allowed size for a client request body (e.g., file uploads). Requests exceeding this limit are rejected with `413 Content Too Large`.

```nginx
client_max_body_size 10M;
```

- Units: `K` (kilobytes), `M` (megabytes), `G` (gigabytes).
- Overridable at the location level.

---

##### `large_client_header_buffers`
**Optional.** Sets the buffer size allocated for large request headers.

```nginx
large_client_header_buffers 8K;
```

- Units: `K`, `M`, `G`.

---

##### `error_page`
**Optional.** Maps one or more HTTP status codes to a custom error page. Multiple `error_page` directives are allowed.

```nginx
error_page 404 www/html/errors/404.html;
error_page 403 404 405 www/html/errors/default.html;
```

- Codes must be in the range 100–599.
- A single directive can handle multiple codes, all pointing to the same file.

---

#### Location-Level Directives

These directives are set inside a `location /path { }` block and apply only to requests matching that path. Location matching uses longest-prefix matching.

---

##### `root`
**Optional.** Overrides the server-level root for this location.

```nginx
location /uploads/ {
    root www/uploads/;
}
```

---

##### `methods`
**Optional.** Restricts HTTP methods for this location, overriding the server-level setting.

```nginx
location /uploads/ {
    methods GET POST DELETE;
}
```

---

##### `autoindex`
**Optional.** Enables or disables automatic directory listing. When enabled, requesting a directory will return an HTML page listing its contents.

```nginx
autoindex on;   # enable directory listing
autoindex off;  # disable (default)
```

---

##### `index`
**Optional.** Specifies the default file to serve when a directory is requested. Multiple filenames can be listed; the server tries each in order.

```nginx
index index.html;
index index.html index.htm;
```

---

##### `upload_dir`
**Optional.** Specifies the directory where uploaded files (via POST) are stored, and from which files can be deleted (via DELETE). This overrides `root` for write operations.

```nginx
location /uploads/ {
    methods POST DELETE;
    upload_dir www/uploads/;
}
```

---

##### `client_max_body_size`
**Optional.** Overrides the server-level body size limit for this location specifically.

```nginx
location /post_body {
    methods POST;
    client_max_body_size 100M;
}
```

---

##### `cgi_ext`
**Optional.** Maps a file extension to a CGI interpreter. When a file matching the extension is requested, it is executed by the specified interpreter. Multiple `cgi_ext` directives are allowed in the same location.

```nginx
location /cgi-bin {
    methods GET POST;
    root /www/cgi-bin;
    cgi_ext .py /usr/bin/python3;
    cgi_ext .bla ./cgi_tester;
}
```

- The extension must start with `.`.
- The interpreter must be an absolute or relative path to an executable.

---

##### `return`
**Optional.** Immediately redirects the client to another URL or returns a specific status code, without serving any file.

```nginx
return 301 http://example.com/;
return 404;
```

- The status code must be in the range 100–599.
- The URL is optional; for redirect codes (301, 302), it sets the `Location` header.

---

##### `error_page`
**Optional.** Same as the server-level `error_page`, but scoped to this location only.

```nginx
error_page 404 /errors/not-found.html;
```

---

### Full Configuration Example

```nginx
server {
    root www/;
    listen localhost:8080;
    server_name example.com;
    client_max_body_size 1M;

    error_page 404 www/html/errors/default.html;
    error_page 500 www/html/errors/default.html;

    location / {
        methods GET;
        index index.html;
    }

    location /post_body {
        methods POST;
        client_max_body_size 100M;
    }

    location /uploads/ {
        autoindex on;
        methods GET POST DELETE;
        root www/uploads/;
        upload_dir www/uploads/;
    }

    location /cgi-bin {
        methods GET POST;
        root /www/cgi-bin;
        cgi_ext .py /usr/bin/python3;
    }

    location /old-page {
        return 301 /new-page;
    }
}
```

---

## Features

| Feature | Description |
|---|---|
| Non-blocking I/O | All I/O is handled via `epoll` with edge-triggered mode |
| Multiple servers | Run several virtual servers on different host/port pairs from one config |
| Static file serving | Serves HTML, CSS, JS, images, PDFs, and more with correct MIME types |
| Directory listing | Configurable autoindex generates browsable HTML directory pages |
| File uploads | POST requests save files to a configured directory |
| File deletion | DELETE requests remove files from upload directories |
| CGI support | Executes external scripts/binaries and streams their output as HTTP responses |
| Custom error pages | Map any HTTP status code to a custom HTML file |
| HTTP redirects | Redirect clients using the `return` directive |
| Config validation | Strict config file parsing with descriptive error messages |

---

## Resources

- **Peers at 42 Porto** — for collaboration, feedback, and helping test edge cases during development.
- **[Nginx documentation](https://nginx.org/en/docs/)** — used as a reference for the configuration file syntax and server behavior semantics.
- **HTTP RFCs** — particularly [RFC 7230](https://www.rfc-editor.org/rfc/rfc7230) (HTTP/1.1 Message Syntax), [RFC 7231](https://www.rfc-editor.org/rfc/rfc7231) (Semantics and Content), and [RFC 3875](https://www.rfc-editor.org/rfc/rfc3875) (CGI/1.1) for understanding the protocol requirements.
- **Linux manual pages** — `epoll(7)`, `epoll_create(2)`, `epoll_ctl(2)`, `epoll_wait(2)`, `recv(2)`, `send(2)`, `socket(2)`, `bind(2)`, `listen(2)`, `accept(2)`, `fork(2)`, `execve(2)`, `pipe(2)`, `fcntl(2)`, and related system calls used throughout the server implementation.
- **AI assistance** — AI was used to ask questions about project requirements and protocol details, to organize development work into sprints, and to get detailed explanations of how individual features (epoll event loop, CGI forking, chunked transfer encoding, etc.) should be implemented. AI was also used to generate additional test cases beyond our own test suite, in order to verify that edge cases and corner cases were handled correctly.
