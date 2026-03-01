# Grammar Description (v1)

This document is the initial rulebook of our config language syntax.
Just like any language has rules to form sentences and spell words, our configuration file will have rules too. The goal of this document is to soecify what's allowed and what isn't when writing a config file.

The grammar is expressed using a standard notation called EBNF (Extended Backus–Naur form), a common way to define computer languages, and since we are implementing a recursive descent parser, the grammar will map directly to our parsing functions.

------------------------------------------------------------------------

## 1. Lexical Structure (Tokenization)

Before our server can understand what the config file means, it will first read the raw text and break it down into meaningful pieces - tokens.
This section defines what the meaningful pieces will look like. 

### Keywords  
_(Reserved words that mean something specific to the server)_

-   SERVER
-   LISTEN
-   SERVER_NAME
-   ROOT
-   LOCATION
-   METHODS

### Symbols  
_(Punctuation used to structure the file)_

-   `{` → LBRACE
-   `}` → RBRACE
-   `;` → SEMICOLON

### Literals  
_(Variables provided, like numbers or paths)_

-   NUMBER → sequence of digits (0-9)+
-   IDENTIFIER → alphanumeric string\
-   PATH → string starting with `/` and containing valid path
    characters\
-   METHOD → UPPERCASE_IDENTIFIER

### Comments

-   Comments begin with `#`
-   Everything from `#` to `\n` is ignored

### Whitespace

Whitespace, tabs and newlines are ignored except as token separators.

------------------------------------------------------------------------

## 2. Structural Grammar  

Once we get the tokens, this section will define how they are allowed to fit together. It's like a sentence structure rules of any given language.
It will tell us for example, that a server block muyst start with the word `server`, followed by `{` (LBRACE), some directives, and end with `}` (RBRACE).

### Configuration

    config → server_block*

A configuration file consists of zero or more server blocks.

------------------------------------------------------------------------

### Server Block

    server_block → SERVER LBRACE server_body RBRACE  

_A block starts with the keyword, opening brace, the body content, and then a closing brace._

------------------------------------------------------------------------

### Server Body

    server_body → (listen_directive
                   | server_name_directive
                   | root_directive
                   | methods_directive
                   | location_block)*

_Directives can be placed in any order._

------------------------------------------------------------------------

### Listen Directive

    listen_directive → LISTEN NUMBER SEMICOLON  

_A listen directive looks like: `listen 8080;` and we will allow multiple `listen` directives are allowed within the same server block._

------------------------------------------------------------------------

### Server Name Directive

    server_name_directive → SERVER_NAME IDENTIFIER+ SEMICOLON  

_A `server_name` directive looks like: `server_name something.com localhost;`. One or more identifiers are allowed._

------------------------------------------------------------------------

### Root Directive

    root_directive → ROOT PATH SEMICOLON  

_A `root` directive looks like: `root /var/www;`.Only one `root` directive is allowed per server block_.

------------------------------------------------------------------------

### Methods Directive

    methods_directive → METHODS METHOD+ SEMICOLON  

_A methods directive looks like: `methods GET POST;`. If a `methods` directive is present, it must contain at least one method._

------------------------------------------------------------------------

### Location Block

    location_block → LOCATION PATH LBRACE location_body RBRACE  

_A location block looks like: `location /images { ... }`_

------------------------------------------------------------------------

### Location Body

    location_body → (root_directive
                     | methods_directive)*

_Location blocks can only contain root and methods directives. Nested location blocks are structurally forbidden here._

------------------------------------------------------------------------

## 3. Semantic Constraints (Validated During Parsing)

This section lists deeper logic rules that we want to check to ensure the configuration makes sense.

These are the rules that will be enforced during parsing:

-   A `server` block must contain at least one `listen` directive.
-   A `server` block must contain exactly one `root` directive.
-   Duplicate `root` directives in the same block are invalid.
-   Duplicate `methods` directives in the same block are invalid.
-   Duplicate `server_name` directives in the same block are invalid.
-   Nested `location` blocks are invalid.
-   Directives can appear in any order.
-   A `METHOD` must correspond to one of the supported HTTP methods
    defined by the server implementation.
-   A `location` block overrides `root` and `methods`, but cannot
    define `listen` or `server_name`.

------------------------------------------------------------------------

This grammar defines the valid syntax of the configuration language.\
Behavioral resolution (server selection, location matching, method
fallback) is defined separately in the language specification document.
