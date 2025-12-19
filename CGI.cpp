#include "CGI.hpp"

std::string CGI::trim(std::string &s)
{
    std::string result = s;
    size_t start = result.find_first_not_of(" \t\n\r");
    if (start != std::string::npos)
        result = result.substr(start);
    size_t end = result.find_last_not_of(" \t\n\r");
    if (end != std::string::npos)
        result = result.substr(0, end + 1);
    return result;
}

CGI::CGI(Server *srv, Server::Request &req, std::string &abs_path, std::string &ext)
    : server(srv), request(req), script_path(abs_path), extension(ext),
      env_cgi(NULL), argv(NULL), pid(-1)
{
    pipe_in[0] = pipe_in[1] = -1;
    pipe_out[0] = pipe_out[1] = -1;
    pipe_err[0] = pipe_err[1] = -1;

    if (extension == ".php")
        cgi_path = "/usr/bin/php-cgi";
    else
        cgi_path = "/usr/bin/python3";

    assignExtension();
}

CGI::~CGI()
{
    cleanup();
}

static bool findHeader(
    const std::vector<std::pair<std::string, std::string> > &headers,
    const std::string &key,
    std::string &outValue
) {
    for (size_t i = 0; i < headers.size(); i++) {
        if (headers[i].first == key) {
            outValue = headers[i].second;
            return true;
        }
    }
    return false;
}


std::string CGI::execute(Server::Request &req, std::string &abs_path, std::string &extension)
{
    request = req;
    script_path = abs_path;
    extension = extension;

    try
    {
        setupPipes();

        pid = fork();
        if (pid == -1)
        {
            perror("fork");
            handlePipeErrors();
            return server->buildResponse(server->buildErrorPage(500), ".html", 500, false, "", false);
        }

        if (pid == 0)
        {
            childProcess();
            exit(1);
        }
        else
        {
            parentProcess();
            return parseCGIOutput(cgi_output);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "CGI Error: " << e.what() << std::endl;
        return server->buildResponse(server->buildErrorPage(500), ".html", 500, false, "", false);
    }

    return server->buildResponse(server->buildErrorPage(500), ".html", 500, false, "", false);
}

void CGI::setupPipes()
{
    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1 || pipe(pipe_err) == -1)
    {
        perror("pipe");
        handlePipeErrors();
        throw std::runtime_error("Error: Failed to create pipes");
    }

    // don't block the main thread in parent 
    // fcntl(pipe_out[0], F_SETFL, O_NONBLOCK);
    // fcntl(pipe_err[0], F_SETFL, O_NONBLOCK);
    // fcntl(pipe_in[1], F_SETFL, O_NONBLOCK); 
}

void CGI::setupEnvironment()
{
    env_vars.clear();

    env_vars.push_back("REQUEST_METHOD=" + request.method);
    env_vars.push_back("SCRIPT_FILENAME=" + script_path);
    // ask old promo, should it be implemented
    env_vars.push_back("QUERY_STRING=");
    env_vars.push_back("REDIRECT_STATUS=200");
    env_vars.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env_vars.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env_vars.push_back("SERVER_SOFTWARE=Webserv/1.0");

    setupAuthEnvironment();
    setupServerEnvironment();
    setupContentEnvironment();
    setupHeadersEnvironment();
    setupPathEnvironment();

    // print env vars in child
    // std::cerr << "ENV VARS:\n";
    // for (size_t i = 0; i < env_vars.size(); i++)
    //     std::cerr << env_vars[i] << std::endl;
}

void CGI::setupAuthEnvironment()
{
    std::map<std::string, std::string>::iterator it = request.headers.find("authorization");
    if (it != request.headers.end() && !it->second.empty())
    {
        std::string auth = trim(it->second);
        size_t pos = auth.find(' ');

        if (pos != std::string::npos)
        {
            std::string scheme = auth.substr(0, pos);
            env_vars.push_back("AUTH_TYPE=" + scheme);
            env_vars.push_back("HTTP_AUTHORIZATION=" + auth);
        }
        else
        {
            env_vars.push_back("AUTH_TYPE=" + auth);
            env_vars.push_back("HTTP_AUTHORIZATION=" + auth);
        }
    }
    else
    {
        env_vars.push_back("AUTH_TYPE=");
    }
}

void CGI::setupServerEnvironment()
{
    std::string server_name = "localhost";
    std::string server_port = "80";

    // print request headers
    // for (std::map<std::string, std::string>::iterator it = request.headers.begin(); it != request.headers.end(); it++) {
    //     std::cerr << "\nfirst: |" << it->first << "| second: |" <<  it->second << "|" ;
    // }
        
    std::map<std::string, std::string>::iterator it = request.headers.find("host");
    if (it != request.headers.end())
    {
        std::string host = trim(it->second);
        size_t colon = host.find(':');

        if (colon != std::string::npos)
        {
            server_name = host.substr(0, colon);
            server_port = host.substr(colon + 1);
        }
        else
        {
            server_name = host;
        }
    }

    env_vars.push_back("SERVER_NAME=" + server_name);
    env_vars.push_back("SERVER_PORT=" + server_port);

    std::string remote_addr = "127.0.0.1";
    int remote_port = 0;
    if (server) {
        remote_addr = request.remote_addr;
        remote_port = request.remote_port;
    }
    std::stringstream ss;
    ss << remote_port;
    env_vars.push_back("REMOTE_ADDR=" + remote_addr);
    env_vars.push_back("REMOTE_PORT=" + ss.str());
//     std::cerr << "remote addr: " << remote_addr << std::endl;
//     std::cerr << "remote port: " << ss.str() << std::endl;
}

void CGI::setupContentEnvironment()
{
    if (request.method == "POST")
    {
        if (request.headers.count("content-length"))
        {
            env_vars.push_back("CONTENT_LENGTH=" + request.headers.find("content-length")->second);
        }
        else if (!request.body.empty())
        {
            std::ostringstream oss;
            oss << request.body.size();
            env_vars.push_back("CONTENT_LENGTH=" + oss.str());
        }
        else
        {
            env_vars.push_back("CONTENT_LENGTH=0");
        }

        if (request.headers.count("content-type"))
        {
            env_vars.push_back("CONTENT_TYPE=" + request.headers.find("content-type")->second);
        }
    }
    else
    {
        env_vars.push_back("CONTENT_LENGTH=");
        env_vars.push_back("CONTENT_TYPE=");
    }
}

void CGI::setupHeadersEnvironment()
{
    for (std::map<std::string, std::string>::const_iterator it = request.headers.begin();
         it != request.headers.end(); ++it)
    {
        if (it->first == "authorization" || it->first == "content-length")
            continue;
        std::string key = "HTTP_" + it->first;
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);
        std::replace(key.begin(), key.end(), '-', '_');
        env_vars.push_back(key + "=" + it->second);
    }
}

void CGI::setupPathEnvironment()
{
    env_vars.push_back("SCRIPT_NAME=" + request.uri);
    env_vars.push_back("PATH_INFO="); // if path info is empty same should be for path translated
    env_vars.push_back("PATH_TRANSLATED=");
}

void CGI::convertEnvVarsToCharPtr()
{
    env_cgi = new char *[env_vars.size() + 1];
    for (size_t i = 0; i < env_vars.size(); i++)
        env_cgi[i] = const_cast<char *>(env_vars[i].c_str());
    env_cgi[env_vars.size()] = NULL;
}

void CGI::setupArguments()
{
    // cgi/path + script/location + NULL for python script
    argv = new char *[3];

    argv[0] = new char[cgi_path.size() + 1];
    std::strcpy(argv[0], cgi_path.c_str());

    argv[1] = new char[script_path.size() + 1];
    std::strcpy(argv[1], script_path.c_str());

    argv[2] = NULL;
}

int CGI::changeToScriptDirectory() // what if the script includes another file (includes "./index.js")
{
    size_t last_slash = script_path.find_last_of("/");
    if (last_slash != std::string::npos)
    {
        std::string dir = script_path.substr(0, last_slash);
        if (chdir(dir.c_str()) == -1)
        {
            std::cerr << "Error: CGI: Failed to change to directory: " << dir << " - " << strerror(errno) << std::endl;
            return 1;
        }
    }
    return 0;
}

void CGI::childProcess()
{
    dup2(pipe_in[0], STDIN_FILENO);
    dup2(pipe_out[1], STDOUT_FILENO);
    dup2(pipe_err[1], STDERR_FILENO);

    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);
    close(pipe_err[0]);
    close(pipe_err[1]);

    setupEnvironment();
    convertEnvVarsToCharPtr();
    setupArguments();
    
    if (!changeToScriptDirectory())
        execve(cgi_path.c_str(), argv, env_cgi);

    delete[] argv[0];
    delete[] argv[1];
    delete[] argv;
    delete[] env_cgi;

    exit(1);
}

void CGI::parentProcess()
{
    close(pipe_in[0]);
    close(pipe_out[1]);
    close(pipe_err[1]);

    if (request.method == "POST" && !request.body.empty()) // if post is fixed, it must be fixed in here too (charaf)
    {
        ssize_t bytes_written = write(pipe_in[1], request.body.c_str(), request.body.size());
        if (bytes_written != static_cast<ssize_t>(request.body.size()))
            std::cerr << "Warning: Could not write full body to CGI stdin" << std::endl;
    }
    close(pipe_in[1]);

    readFromPipes();

    int status;
    waitpid(pid, &status, 0);

    if (!error_output.empty())
        std::cerr << "CGI stderr: " << error_output << std::endl;
}

void CGI::readFromPipes()
{
    char buffer[4096];
    ssize_t bytes_read;

    // same here, read is gonna failt if it exceeds buffer (charaf) + i think i must set it to nonblocking
    while ((bytes_read = read(pipe_out[0], buffer, sizeof(buffer))) > 0)
        cgi_output.append(buffer, bytes_read);
    close(pipe_out[0]);

    while ((bytes_read = read(pipe_err[0], buffer, sizeof(buffer))) > 0)
        error_output.append(buffer, bytes_read);
    close(pipe_err[0]);
}

void CGI::assignExtension(void)
{
    ext_map["audio/aac"] = ".aac";
    ext_map["application/x-abiword"] = ".abw";
    ext_map["image/apng"] = ".apng";
    ext_map["application/x-freearc"] = ".arc";
    ext_map["image/avif"] = ".avif";
    ext_map["application/vnd.amazon.ebook"] = ".azw";
    ext_map["video/x-msvideo"] = ".avi";
    ext_map["application/octet-stream"] = ".bin";
    ext_map["image/bmp"] = ".bmp";
    ext_map["application/x-bzip"] = ".bz";
    ext_map["application/x-bzip2"] = ".bz2";
    ext_map["application/x-cdf"] = ".cda";
    ext_map["application/x-csh"] = ".csh";
    ext_map["text/css"] = ".css";
    ext_map["text/csv"] = ".csv";
    ext_map["application/msword"] = ".doc";
    ext_map["application/vnd.openxmlformats-officedocument.wordprocessingml.document"] = ".docx";
    ext_map["application/vnd.ms-fontobject"] = ".eot";
    ext_map["application/epub+zip"] = ".epub";
    ext_map["application/gzip"] = ".gz";
    ext_map["image/gif"] = ".gif";
    ext_map["text/html"] = ".htm";
    ext_map["text/html"] = ".html";
    ext_map["image/vnd.microsoft.icon"] = ".ico";
    ext_map["text/calendar"] = ".ics";
    ext_map["application/java-archive"] = ".jar";
    ext_map["image/jpeg"] = ".jpeg";
    ext_map["image/jpeg"] = ".jpg";
    ext_map["text/javascript"] = ".js";
    ext_map["application/json"] = ".json";
    ext_map["application/ld+json"] = ".jsonld";
    ext_map["text/markdown"] = ".md";
    ext_map["audio/midi"] = ".mid";
    ext_map["audio/midi"] = ".midi";
    ext_map["text/javascript"] = ".mjs";
    ext_map["audio/mpeg"] = ".mp3";
    ext_map["video/mp4"] = ".mp4";
    ext_map["video/x-matroska"] = ".mkv";
    ext_map["video/mpeg"] = ".mpeg";
    ext_map["application/vnd.apple.installer+xml"] = ".mpkg";
    ext_map["application/vnd.oasis.opendocument.presentation"] = ".odp";
    ext_map["application/vnd.oasis.opendocument.spreadsheet"] = ".ods";
    ext_map["application/vnd.oasis.opendocument.text"] = ".odt";
    ext_map["audio/ogg"] = ".oga";
    ext_map["video/ogg"] = ".ogv";
    ext_map["application/ogg"] = ".ogx";
    ext_map["audio/ogg"] = ".opus";
    ext_map["font/otf"] = ".otf";
    ext_map["image/png"] = ".png";
    ext_map["application/pdf"] = ".pdf";
    ext_map["application/x-httpd-php"] = ".php";
    ext_map["application/vnd.ms-powerpoint"] = ".ppt";
    ext_map["application/vnd.openxmlformats-officedocument.presentationml.presentation"] = ".pptx";
    ext_map["application/vnd.rar"] = ".rar";
    ext_map["application/rtf"] = ".rtf";
    ext_map["application/x-sh"] = ".sh";
    ext_map["image/svg+xml"] = ".svg";
    ext_map["application/x-tar"] = ".tar";
    ext_map["image/tiff"] = ".tif";
    ext_map["image/tiff"] = ".tiff";
    ext_map["video/mp2t"] = ".ts";
    ext_map["font/ttf"] = ".ttf";
    ext_map["text/plain"] = ".txt";
    ext_map["application/vnd.visio"] = ".vsd";
    ext_map["audio/wav"] = ".wav";
    ext_map["audio/webm"] = ".weba";
    ext_map["video/webm"] = ".webm";
    ext_map["application/manifest+json"] = ".webmanifest";
    ext_map["image/webp"] = ".webp";
    ext_map["font/woff"] = ".woff";
    ext_map["font/woff2"] = ".woff2";
    ext_map["application/xhtml+xml"] = ".xhtml";
    ext_map["application/vnd.ms-excel"] = ".xls";
    ext_map["application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"] = ".xlsx";
    ext_map["application/xml"] = ".xml";
    ext_map["application/vnd.mozilla.xul+xml"] = ".xul";
    ext_map["application/zip"] = ".zip";
    ext_map["video/3gpp"] = ".3gp";
    ext_map["video/3gpp2"] = ".3g2";
    ext_map["application/x-7z-compressed"] = ".7z";
}

std::string CGI::parseCGIOutput(std::string &cgi_output)
{
    size_t header_end = cgi_output.find("\r\n\r\n");
    size_t delimiter_len = 4;

    if (header_end != std::string::npos)
    {
        std::string headers_str = cgi_output.substr(0, header_end);
        std::string body = cgi_output.substr(header_end + delimiter_len);
        
        std::vector<std::pair<std::string, std::string> > cgi_headers = parseCGIHeaders(headers_str);
        // print cgi headers
        // std::cerr << "\nCGI HEADERS:\n";
        // for (std::vector<std::pair<std::string, std::string> >::const_iterator it = cgi_headers.begin(); it != cgi_headers.end(); it++)
        // {
        //     std::cerr << "key: |" << it->first
        //             << "| value: |" << it->second << "|" << std::endl;
        // }

        int status_code = 200;
        std::string value;
        if (findHeader(cgi_headers, "status", value))
        {
            std::istringstream ss(value);
            ss >> status_code;
            if (status_code < 100 || status_code > 599)
                status_code = 200;
        }

        std::string location;
        if (findHeader(cgi_headers, "location", location)) {
            return server->buildResponse("", ".html", 302, true, location, request.keep_alive);
        }

        std::string ext = ".txt";
        if (findHeader(cgi_headers, "content-type", value)) {
            size_t semicolon = value.find(';');
            if (semicolon != std::string::npos)
                value = value.substr(0, semicolon);
            value = trim(value);

            if (ext_map.count(value))
                ext = ext_map[value];
        }

        return server->buildResponse(body, ext, status_code, false, "", request.keep_alive, cgi_headers);
    }
    else // header not set in cgi output
        return server->buildResponse(cgi_output, ".txt", 200, false, "", request.keep_alive);
}

std::vector<std::pair<std::string, std::string> > CGI::parseCGIHeaders(std::string &headers)
{
    std::vector<std::pair<std::string, std::string> > cgi_headers;
    std::istringstream header_stream(headers);
    std::string line;

    while (std::getline(header_stream, line)) {
        size_t colon = line.find(':');

        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            key = trim(key);
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            value = trim(value);

            cgi_headers.push_back(std::pair<std::string, std::string>(key, value));
        }
    }

    return cgi_headers;
}

void CGI::cleanup()
{
    if (argv)
    {
        delete[] argv[0];
        delete[] argv[1];
        delete[] argv;
        argv = NULL;
    }

    if (env_cgi)
    {
        delete[] env_cgi;
        env_cgi = NULL;
    }

    handlePipeErrors();
}

void CGI::handlePipeErrors()
{
    for (int i = 0; i < 2; i++)
    {
        if (pipe_in[i] != -1)
        {
            close(pipe_in[i]);
            pipe_in[i] = -1;
        }
        if (pipe_out[i] != -1)
        {
            close(pipe_out[i]);
            pipe_out[i] = -1;
        }
        if (pipe_err[i] != -1)
        {
            close(pipe_err[i]);
            pipe_err[i] = -1;
        }
    }
}