# Configuration Language Specification (v2)

### Overall Structure

The configuration file is intentionally minimal to keep it easy to manage. 

It is composed only of `server` blocks. There are no global directives outside these blocks.
Each `server` block represents an independent behavioral unit, which basically means - one website instance with its own rules.

Multiple server blocks can exist in the same file, as long as each one defines a unique host:port combination. This allows our webserver to:

- Listen on different ports
- Serve different content.
- Apply different behaviors.

Keeping everything contained inside `server` blocks avoids ambiguity and keeps the structure predictable.

### Server Block

A `server` block defines two main things:

- How to connect to it (ports).
- The default behavior for a website.

##### Core Server Directives:

- `listen`: Specifies the port the server accepts connections on. You can use multiple listen directives in the same block if the site should respond on more than one port.
	- If only a port is specified (e.g., `listen 8080;`), the host defaults to `0.0.0.0`.
- `root` (Mandatory): Defines the base folder on your computer where the website's files are located.
- `server_name` (Optional): Defines the domain name(s) associated with this server block.
	- _Note: Since the current grammar enforces unique host:port combinations for every server block, the server name is informational and not used for server selection logic._

##### Additonal Server-Level Directives:
These define default behaviors that can be overridden later in `location` blocks:
 
- `methods` (Optional): Restricts which HTTP methods (GET, POST, DELETE, etc.) are allowed for the entire server. These act as a restriction layer.
- `client_max_body_size`: Limits the allowed size of a client request body (e.g., for uploads).
- `error_page`: Defines custom HTML files to serve for specific error codes (e.g., 404, 500).

Example:

```jsx
server {
    listen 8080;
    server_name example.com;
    root /var/www/html;
    methods GET POST;
    client_max_body_size 5M;
    error_page 404 /errors/404.html;
}
```

### Location Block

A `location` block defines behavior for a specific URI prefix.

##### Matching Strategy:
When a request comes in, the server compares the request URI against all defined location blocks. It selects the block with the longest matching prefix. This is a case-sensitive string match.

##### Overrides and Specific Behavior:
A location block inherits settings from its parent server block, but it can define its own specific rules. The following directives can be used inside a location:

- `root` (Overrides server default)
- `methods` (Overrides server default)
- `autoindex` (Enables/disables directory listing)
- `upload_dir` (Sets a specific folder for file uploads)
- `cgi_ext` (Defines which file extensions should be handled as CGI scripts)
- `return` (Triggers an immediate HTTP redirection)

Example:

```jsx
location /images {
    root /data/images;
    autoindex on;
}

location /images/icons {
    # This is a longer, more specific match than /images
    root /data/icons;
    methods GET; # Overrides server default to restrict access here
}
```

A request for `/images/icons/logo.png` will use the second block, because `/images/icons` is a longer, more specific match than just `/images`.

### Resolution Rules (How the server decides)

When a request comes in, the server follows a simple, deterministic 2-step process to decide what to do.

##### Select the server

When a network connection is accepted, the server knows exactly which interface and port the request came in on.
Because IP:Port combinations are unique in the config file, the server immediately knows which single server block handles this connection. There is no ambiguity and no need to check the Host header for selection.

##### Select the location

Once the server block is identified, it looks for the best matching `location` block using the "longest prefix" rule described above.

### Default Behavior and Fallback

So what will happen if a request doesn't match any specific rules?

##### Location fallback

If no location block matches the request URI, the server-level configuration is applied.

Example:

```jsx
server {
		listen 8080;
		root /var/www/html;
}
```

Even without any location blocks, this server is fully functional. A request to `/index.html` resolves to `/var/www/html/index.html`.

### Method Resolution and Defaults

The `methods` directive is optional and may be declared at both:

- Server level
- Location level

If defined at server level, it restricts the allowed HTTP methods globally for that server.
A location block may override this restriction by declaring its own `methods` directive.
If no `methods` directive is declared anywhere (neither in the server nor the location), all HTTP methods are allowed by default.

Example:

```jsx
server {
    listen 8080;
    root /var/www/html;
    methods GET POST; # Default global restriction

    location /api/delete-stuff {
        methods DELETE; # Specific override for this path
    }
}
```

In this configuration:

- A request to `/index.html` allows only GET and POST (server default).
- A request to `/api/delete-stuff/id` allows only DELETE (location override).
