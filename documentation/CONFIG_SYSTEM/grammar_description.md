# Grammar Description (v2)

This document is the official rulebook for our configuration language syntax.
Just like languages have rules for spelling words and structuring sentences, our configuration file has to follow rules too. This document defines exactly what is allowed and what isn't when writing a config file.
The grammar is expressed using a standard notation called EBNF (Extended Backus–Naur form). Since we are implementing a recursive descent parser, this grammar will map directly to the functions we need to write in our code.

------------------------------------------------------------------------

## 1. Lexical Structure (Tokenization)

Before the server understands what the config file means, it reads the raw text and breaks it into meaningful chunks called tokens.
Our strategy here is to keep the tokenizer intentionally simple. It doesn't know the difference between a keyword (like "server") and a value (like "/var/www"). It just groups characters together. The parser will figure out what the words mean later.

### Token Types

The tokenizer is intentionally simple.

- `WORD` → any sequence of characters that does not contain whitespace or symbols.
- `LBRACE` → `{` 
- `RBRACE` → `}`
- `SEMICOLON` → `;`

The tokenizer does **not** classify words as numbers, paths, methods, etc.  
All semantic interpretation happens later during parsing.

Each token must store the line number where it was found so we can give precise error messages to the user

---

### Comments

-   Comments begin with `#`
-   Everything from `#` to `\n` is ignored

### Whitespace

Whitespace, tabs and newlines are ignored except as token separators.

------------------------------------------------------------------------

## 2. Structural Grammar  

Once we collect the tokens, this section defines how they are allowed to fit together. This is the sentence structure of our language.
It will tell us for example, that a server block must start with the word `server`, followed by `{` (LBRACE), some directives, and end with `}` (RBRACE).

_Note: In the rules below, strings in quotes like `"listen"` mean the parser expects a `WORD` token containing exactly those characters._

### Configuration

    config → server_block+

A configuration file consists of one or more server blocks.

------------------------------------------------------------------------

### Server Block

    server_block → "server" LBRACE server_body RBRACE  

_A block starts with the word "server", an opening brace, the body content, and a closing brace._

------------------------------------------------------------------------

### Server Body

    server_body → (
          listen_directive
        | server_name_directive
        | root_directive
        | client_max_body_size_directive
        | error_page_directive
        | methods_directive
        | location_block
    )*

_Directives can appear in any order inside the server block._

---

### Listen Directive

    listen_directive → "listen" WORD SEMICOLON

The `WORD` may represent a `PORT` (e.g., `8080`) or a `HOST:PORT` (e.g., `127.0.0.1:8080`).
_Example: `listen 8080;`_  
_Note : If only a PORT is provided (like `listen 8080;`), we assume the host is `0.0.0.0`. That basically means “listen everywhere.”_

---

### Server Name Directive

    server_name_directive → "server_name" WORD+ SEMICOLON  

_Example: `server_name something.com localhost;`. One or more names are allowed._

---

### Root Directive

    root_directive → "root" WORD SEMICOLON  

_Example: `root /var/www;`. Only one `root` directive is allowed per server block_.

---

### Client Max Body Size Directive

    client_max_body_size_directive → "client_max_body_size" WORD SEMICOLON

_Examples: `client_max_body_size 10M;` or `client_max_body_size 1024k;`_

---

### Error Page Directive

    error_page_directive → "error_page" WORD+ WORD SEMICOLON

_Example: `error_page 500 502 503 /errors/50x.html;` (Multiple error codes followed by one path)_

---

### Methods Directive

    methods_directive → "methods" WORD+ SEMICOLON  

_Example: `methods GET POST;`. If a `methods` directive is present, it must contain at least one method._

---

### Location Block

    location_block → "location" WORD LBRACE location_body RBRACE  

_Example: `location /images { ... }`_

---

### Location Body

    location_body → (
          root_directive
        | methods_directive
        | autoindex_directive
        | upload_dir_directive
        | cgi_ext_directive
        | return_directive
    )*

_Nested `location` blocks are forbidden._

---

### Autoindex Directive

    autoindex_directive → "autoindex" WORD SEMICOLON

_Allowed values: "on" | "off"_

---

### Upload Directory Directive

    upload_dir_directive → "upload_dir" WORD SEMICOLON

_Example: `upload_dir /var/www/uploads;`_

---

### CGI Extension Directive

    cgi_ext_directive → "cgi_ext" WORD WORD SEMICOLON

_Example: `cgi_ext .py /usr/bin/python3;`. May appear multiple times inside a location block._
---

### Return Directive

    return_directive → "return" WORD WORD SEMICOLON

_Example: `return 301 /new-path;`_

---

## 3. Semantic Constraints (Validated During Parsing)

The grammar above only defines the shape of the sentences. This section defines the logic and data validation rules. Even if the brackets match perfectly, the config might still be invalid if the data inside doesn't make sense (like setting a port to 999999).

The following rules are enforced after structural parsing:

### Structural Constraints
- At least one `server` block must exist.
- Each `server` block must contain at least one `listen` directive.
- A server may define `root` at most once.
- A server may define `methods` at most once.
- A server may define `server_name` at most once.
- Nested `location` blocks are forbidden.

### Data Validation Constraints
- Two server blocks can't share the same `host:port` combination.
- Location paths (the `WORD` after "location") must start with /.
- Paths in `root` and `upload_dir` must start with `/`.
- Ports must be ints between `1` and `65535`.
- `client_max_body_size` must be a valid numeric format.
- `error_page` codes must be valid ints.
- `cgi_ext` extensions must start with a dot (e.g. `.py`).
- `return` code must be `301` or `302`.
- Any unknown directives cause immediate parsing error.

### Error Handling
- All errors must report the correct line number.
- The server must exit cleanly with error code `1` if parsing fails.

---

This grammar defines the valid syntax of the configuration language.\
Behavioral resolution (server selection, location matching, method
fallback) is defined separately in the language specification document.
