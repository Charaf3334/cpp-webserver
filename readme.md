# WebServer - HTTP/1.0 Server Implementation in C++98
![WebServer Demo](demo.gif)

> *This is when you finally understand why URLs start with HTTP*


A fully functional HTTP server written in C++98, capable of serving static websites, handling CGI scripts, managing file uploads, and processing cookies for session management.

## Project Overview

This project implements a custom HTTP server from scratch in C++98, following the HTTP/1.0 protocol specification. The server is non-blocking, uses I/O multiplexing with `epoll()`, and can handle multiple simultaneous connections while serving content across multiple ports.

## System Requirements

- Linux (epoll-based implementation)
- GCC / Clang with C++98 support
- PHP-CGI (`/usr/bin/php-cgi`)
- Python 3 (`/usr/bin/python3`)


## Team

This project was developed by a team of three developers, each focusing on critical components:

- **[Charaf](https://github.com/Charaf3334)**: Configuration parsing, Server initialization, and GET request handling
- **[Taoufiq](https://github.com/mid-taoufiq)**: POST request implementation and file upload functionality
- **[Zakaria](https://github.com/itsmeyeshua)**: CGI (Common Gateway Interface) execution and management

## Key Features

### Core Functionality
- **HTTP/1.0 Protocol Support** - Fully compliant HTTP/1.0 implementation
- **Non-blocking I/O** - Asynchronous request handling using `epoll()`
- **Multiple HTTP Methods** - GET, POST, and DELETE support
- **Static File Serving** - Efficient delivery of HTML, CSS, JavaScript, and media files
- **File Upload** - Client file upload with configurable storage locations
- **CGI Execution** - Support for dynamic content generation (PHP, Python scripts)
- **Multi-port Listening** - Serve different websites on different ports
- **Custom Error Pages** - Configurable error page handling
- **Directory Listing** - Optional autoindex functionality
- **HTTP Redirections** - Configurable URL redirections
- **Request Body Size Limiting** - Protection against oversized requests

### Advanced Features (Bonus)
- **Cookie & Session Management** - User authentication and session tracking
- **Multiple CGI Types** - PHP and Python CGI support
- **User Authentication** - Sign in/Sign up functionality with persistent sessions

## Project Structure

```
webserver/
├── main.cpp              # Entry point
├── Webserv.cpp/.hpp      # Main server orchestration
├── Server.cpp/.hpp       # Server configuration and socket management
├── Request.hpp           # HTTP request parsing
├── CGI.cpp/.hpp          # CGI script execution
├── Makefile              # Build configuration
├── default/
│   └── default.conf      # Default server configuration
└── prod/                 # Production demo sites
    ├── index.html        # Main landing page
    ├── style.css         # Styling
    ├── upload/           # File upload directory with autoindex demo
    ├── cgi_files/        # CGI test scripts (PHP, Python)
    └── pirate_stream/    # Full-featured streaming website
        ├── db/           # User and movie databases (JSON)
        ├── images/       # Movie posters and assets
        ├── movies/       # Video content
        └── *.php         # Dynamic pages (login, register, streaming)
```

## Known Limitations

- HTTP/1.1 features (chunked transfer, keep-alive) are not supported
- HTTPS / TLS is not implemented
- epoll-based implementation is Linux-only
- CGI timeout is fixed (3 seconds)


## Production Demo Sites

We have included **two fully functional websites** in the repository to demonstrate the server's capabilities. These sites are configured to run locally with the server and showcase all implemented features:

### 1. Main Demonstration Site
A comprehensive showcase of all server functionalities located in the **prod/** directory, this site includes:
- Static file serving
- File upload with directory navigation
- CGI script execution examples
- Error page handling
- Various HTTP methods demonstration

### 2. Pirate Stream (Featured Application)
A complete movie streaming platform located in **prod/pirate_stream/**, featuring:

- **User Authentication System**
  - Sign up with account creation
  - Sign in with cookie-based session management
  - Logout functionality
  
- **Dynamic Header**
  - Displays "Welcome, [Username]!" when logged in
  - Shows login/register options for guests
  
- **Movie Library**
  - Browse available movies with poster thumbnails
  - Stream full-length movies directly in browser
  - Movie metadata display
  
- **Session Persistence**
  - Cookies maintain user sessions across requests
  - Personalized experience based on login status

- **CGI-Powered Backend**
  - PHP scripts handle authentication, session management, and dynamic content
  - JSON-based user and movie databases

## Configuration

The server uses an **NGINX-inspired configuration file** format. You can create your own configuration file or use the default one located at `default/default.conf`.

### Configuration Structure

The configuration file consists of:
- **Global directives** (outside server blocks)
- **Server blocks** (can have multiple servers listening on different ports)
- **Location blocks** (route-specific settings within servers)

### Global Directives

These directives apply to all servers:

```nginx
client_max_body_size 12884901888;             # Maximum request body size in bytes
error_page 404 403 /path/to/error_page.html;  # Custom error pages for status codes
```

### Server Block Configuration

Each server block defines a listening port and contains location blocks:

```nginx
server {
    listen localhost:8080;              # Required: host:port to listen on
    root /path/to/root;                 # Optional: default root for all locations
    
    location / {
        # Location-specific directives
    }
}
```

**Important:** If `root` is not set in either the server block OR the location block, the parser will throw an error. At least one must be defined.

### Location Block Directives

Each location block supports the following directives:

| Directive | Default | Description |
|-----------|---------|-------------|
| `root` | *Required if not in server* | Root directory for this location |
| `index` | `index.html` | Default files to serve (space-separated list) |
| `allow_methods` | `GET` | Allowed HTTP methods (GET, POST, DELETE) |
| `autoindex` | `off` | Enable/disable directory listing (`on` or `off`) |
| `upload_dir` | `/uploads` | Directory where uploaded files are stored |
| `cgi` | `off` | Enable/disable CGI script execution (`on` or `off`) |
| `return` | - | HTTP redirection (status code and URL/text) |

### CGI Behavior

The `cgi` directive controls how `.php` and `.py` files are handled:

- **When `cgi off` (default):** The server serves `.php` and `.py` files as static files (downloads them)
- **When `cgi on`:** The server executes the script and returns the output to the client
  - PHP scripts are executed using `/usr/bin/php-cgi`
  - Python scripts are executed using `/usr/bin/python3`
  - CGI environment variables are properly set (REQUEST_METHOD, QUERY_STRING, etc.)

### Complete Configuration Example

Here's a full configuration file demonstrating all features:

```nginx
# Global configuration
client_max_body_size 12884901888;
error_page 404 403 /home/user/errors/error_page.html;

server {
    listen localhost:8080;
    root /home/user/goinfre/prod;

    # Main landing page
    location / {
        root /home/user/goinfre/prod;
        index index2.html index.html;
        allow_methods GET POST DELETE;
    }

    # Directory listing example
    location /autoindex {
        root /home/user/goinfre/prod/upload/autoindex;
        allow_methods GET DELETE;
        autoindex on;
    }

    # File upload endpoint
    location /upload {
        root /home/user/goinfre/prod/upload;
        allow_methods GET POST;
        upload_dir /autoindex/uploads;
    }

    # CGI scripts directory
    location /cgi_files {
        root /home/user/goinfre/prod/cgi_files;
        index index.html;
        allow_methods GET POST;
        cgi on;
    }

    # Pirate Stream application
    location /pirate_stream {
        root /home/user/goinfre/prod/pirate_stream;
        allow_methods GET POST;
        index main.php;
        cgi on;
    }
}
```

### Configuration Rules & Validation

The parser enforces strict rules:
- Proper bracket matching (`{` and `}`)
- Semicolons after each directive (`;`)
- Valid IP addresses or hostnames in `listen` directive
- Ports must be between 0-65535
- Valid HTTP status codes for error pages (400-599)
- No duplicate server ports (same host:port combination)
- No duplicate location paths within a server
- At least one server block must be defined
- `root` must be set in server block OR location block (or both)

Any syntax error or invalid configuration will be caught during parsing with a descriptive error message.

## Usage

### Compilation
```bash
make
```

### Running the Server
```bash
# With default configuration
./webserv

# With custom configuration file
./webserv path/to/config.conf
```

### Testing
Access the server through your web browser:
- Main site: `http://localhost:8080`
- Pirate Stream: `http://localhost:8080/pirate_stream/`

Or test with command-line tools:
```bash
# Simple GET request
curl http://localhost:8080/

# Upload a file
curl -X POST -F "file=@test.txt" http://localhost:8080/upload
```

## Technical Implementation

### Non-blocking Architecture with epoll
- **I/O Multiplexing:** Uses Linux's `epoll` for efficient, scalable non-blocking I/O operations
- **Single Event Loop:** One `epoll` instance manages all sockets, client connections, and CGI pipes
- **No Blocking Calls:** All socket operations (accept, read, write) are non-blocking
- **Efficient Scalability:** Can handle hundreds of simultaneous connections with minimal overhead
- **Event-Driven Design:** Responds to EPOLLIN (readable) and EPOLLOUT (writable) events

### HTTP Request Processing
1. Parse incoming HTTP request (method, URI, headers, body)
2. Match request to configured route
3. Determine response type (static file, CGI, redirect, error)
4. Generate and send HTTP response with appropriate status codes

### CGI Execution
- Fork child process for CGI script execution
- Set up environment variables (REQUEST_METHOD, QUERY_STRING, CONTENT_LENGTH, etc.)
- Handle stdin/stdout/stderr communication via pipes (also managed by epoll)
- Proper working directory management
- Timeout protection (3 seconds max execution time)
- Parse CGI headers (Status, Content-Type, Location for redirects)

### File Upload Handling
- **Multipart Form Data:** Parse `multipart/form-data` boundaries
- **Streaming Uploads:** Write file chunks directly to disk as they arrive
- **Large File Support:** Handle files up to the configured `client_max_body_size`
- **Automatic Filename Conflicts:** Append `(1)`, `(2)`, etc. to duplicate filenames
- **Non-blocking Writes:** File operations don't block the server

### Cookie & Session Management
- HTTP cookie parsing from request headers
- Session ID generation and validation
- Set-Cookie header generation in responses
- Persistent user state across requests

## Standards Compliance

- **C++98 Standard**: Full compliance with no C++11/14/17 features
- **HTTP/1.0**: Core protocol implementation
- **RFC References**: Guided by HTTP/1.0 RFC
- **NGINX Compatibility**: Configuration format and behavior comparison

## Testing

The server has been stress tested to ensure:
- No crashes or memory leaks under heavy load
- Proper handling of malformed requests
- Correct HTTP status codes
- Graceful client disconnection handling
- Concurrent request processing
- Large file upload/download stability

## Dependencies

**External Functions Used:**
- Socket operations: `socket`, `bind`, `listen`, `accept`, `send`
- I/O multiplexing: `epoll`
- Process management: `fork`, `execve`, `waitpid`, `kill`, `signal`
- File operations: `open`, `read`, `write`, `close`, `stat`, `access`
- Network utilities: `htons`, `htonl`, `ntohs`, `ntohl`, `getaddrinfo`

**No external libraries** - Pure C++98 implementation using only standard library and system calls.

## Project Achievements

- Fully functional HTTP server capable of production deployment
- Complete static website hosting
- Dynamic content generation via CGI
- File upload and management system
- User authentication and session management
- Multiple concurrent website hosting
- Bonus features implemented (cookies, sessions, multiple CGI types)

---

*"Understanding HTTP isn't just about knowing the protocol - it's about building it from the ground up."*