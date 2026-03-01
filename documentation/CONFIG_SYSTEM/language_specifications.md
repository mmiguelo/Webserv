# Configuration Language Specification (v1)

### Overall Structure

The configuration file is intentionally minimal to keep it easy to manage (see example config file at /config). 

It is composed only of `server` blocks. There are no global directives outside these blocks.
Each `server` block represents an independent behavioral unit, which basically means - one website instance with its own rules.

Multiple server blocks can exist in the same file. This allows our webserver to 

- Listen on different ports
- Serve different content.
- Apply different behaviors.

Keeping everything contained inside `server` blocks avoids ambiguity and keeps the structure predictable.

### Server Block

A `server` block defines two main things:

- How to connect to it (ports).
- The default behavior for a website.

##### Directives:

- `listen`: Specifies the port the server accepts connections on. You can use multiple listen directives in the same block if the site should respond on more than one port.
- `root` (Mandatory): Defines the base folder on your computer where the website's files are located.
- `server_name` (Optional): Used when multiple servers share the same port. It checks the HTTP Host header to decide which server block to use. We use exact string matching for this.
- `methods` (Optional): Restricts which HTTP methods (GET, POST, DELETE, etc.) are allowed for the entire server. These act as a restriction layer.

Example:

```jsx
server {
    listen 8080;
    listen 8081;
    server_name qualquer.com;
    root /var/www/html;
    methods GET POST;
}
```

### Location Block

A `location` block defines behavior for a specific URI prefix.
When we find multiple location blocks in the same file, we match the request path (case-sensitive) with the most specific one as explained in the design principles.

A location directive may override the `root` and `methods` directives defined at server level. If a directive is not specified in a location block, the value defined at server level applies.

Example:

```jsx
location /images {
		root /data/images;
}

location /images/icons {
    root /data/icons;
	methods GET; #overrides the server default and only allows GET here
}
```

A request for `/images/icons/logo.png` will use the second block, because `/images/icons` is a longer, more specific match than just `/images`.

### Resolution Rules (How the server decides)

When a request comes in, the server follows a simple, deterministic 2-step process to decide what to do.

##### Select the server

1. It looks at the port the request came in on.
2. If multiple server blocks are listening on that port, it compares the request's `Host` header against the `server_name` directives.
3. If no `server_name` matches, the **first** server block declared in the file for that port is used as the default.

##### Select the location

Once the server block is chosen, it looks for the best matching location block using the "longest prefix" rule described above.

### Default Behavior and Fallback

So what will happen if a request doesn't match any specific rules?

##### Location fallbacjk

This mirrors the behavior of NGINX, where the absence of the `location` directive falls back to the server defaults.

Example:

```jsx
server {
		listen 8080;
		root /var/www/html;
}
```

Even without any location blocks, this server is fully functional. A request to `/index.html` resolves to `/var/www/html/index.html`.

##### Server fallback

The same logic applies to selecting a server. If multiple servers share a port and no hostname matches, the first one defined in the file wins.

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
		methods GET POST; #global restriction

		location /somefolder {
		    methods POST; #specific override
		}

}
```

In this configuration:

- A request to `/index.html` allows GET and POST
- A request to `/somefolder/file.txt` allows only POST

