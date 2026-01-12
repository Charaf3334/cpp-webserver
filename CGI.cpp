#include "CGI.hpp"
#include "Server.hpp"

std::map<std::string, std::string> CGI::ext_map;

CGI::State::State() : pid(-1), client_fd(-1), headers_complete(false), process_complete(false), response_sent_to_client(false), syntax_error(false), exit_status(-1)
{
    pipe_out[0] = pipe_out[1] = -1;
    pipe_err[0] = pipe_err[1] = -1;
    pipe_in[0] = pipe_in[1] = -1;
}

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

CGI::CGI(Server *srv, Request &req, std::string &abs_path, std::string &ext)
    : server(srv), request(req), script_path(abs_path), extension(ext),
      env_cgi(NULL), argv(NULL)
{
    if (extension == ".php")
        cgi_path = "/usr/bin/php-cgi";
    else
        cgi_path = "/usr/bin/python3";
    assignExtension();
}

CGI::~CGI()
{
    if (argv)
    {
        delete[] argv;
        argv = NULL;
    }

    if (env_cgi)
    {
        delete[] env_cgi;
        env_cgi = NULL;
    }
}

bool CGI::start(State &state)
{
    state.script_path = script_path;
    state.extension = extension;
    state.request = request;
    state.headers_complete = false;
    state.process_complete = false;
    state.response_sent_to_client = false;
    state.cgi_path = cgi_path;
    
    if (cgi_path.empty())
        return false;

    if (pipe(state.pipe_out) == -1 || pipe(state.pipe_err) == -1 || ((request.method == "POST" || request.method == "DELETE") && pipe(state.pipe_in) == -1))
    {
        perror("pipe");
        cleanupPipes(state.pipe_in, state.pipe_out, state.pipe_err);
        return false;
    }
    
    state.pid = fork();
    if (state.pid == -1)
    {
        perror("fork");
        cleanupPipes(state.pipe_in, state.pipe_out, state.pipe_err);
        return false;
    }
    
    if (state.pid == 0)
        childProcess(state.pipe_in, state.pipe_out, state.pipe_err);
    else {
        close(state.pipe_out[1]);
        close(state.pipe_err[1]);
        state.pipe_out[1] = -1;
        state.pipe_err[1] = -1;
        
        if ((request.method == "POST" || request.method == "DELETE")) {
            close(state.pipe_in[0]);
            state.pipe_in[0] = -1;
            if (!request.body.empty()) {
                ssize_t bytes_written = write(state.pipe_in[1], request.body.c_str(), request.body.size());
                if (bytes_written == -1)
                    return false;
                else if (bytes_written == 0) {}
            }
            close(state.pipe_in[1]);
            state.pipe_in[1] = -1;
        }
    }
    // std::cout << "Started CGI process " << state.pid << " for client " << state.client_fd << std::endl;
    return true;
}
void CGI::childProcess(int pipe_in[2], int pipe_out[2], int pipe_err[2])
{
    close(pipe_out[0]);
    close(pipe_err[0]);
    if (request.method == "POST" || request.method == "DELETE")
        close(pipe_in[1]);
    
    dup2(pipe_out[1], STDOUT_FILENO);
    dup2(pipe_err[1], STDERR_FILENO);
    if (request.method == "POST" || request.method == "DELETE")
        dup2(pipe_in[0], STDIN_FILENO);
    
    close(pipe_out[1]);
    close(pipe_err[1]);
    if (request.method == "POST" || request.method == "DELETE")
        close(pipe_in[0]);
    
    setupEnvironment();
    convertEnvVarsToCharPtr();
    setupArguments();
    
    if (!changeToScriptDirectory())
        execve(cgi_path.c_str(), argv, env_cgi);
    
    perror("execve");
    delete[] argv;
    delete[] env_cgi;
    exit(1);
}

void CGI::handleOutput(State &state)
{
    if (state.process_complete)
        return;
    
    bool data_read = false;
    char buffer[65536];
    ssize_t bytes_read = read(state.pipe_out[0], buffer, sizeof(buffer));
    if (bytes_read > 0) {
        state.stdout_output.append(buffer, bytes_read);
        data_read = true;
        
        if (!state.headers_complete) {
            size_t header_end = state.stdout_output.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                state.headers_complete = true;
                std::string headers_str = state.stdout_output.substr(0, header_end);
                state.cgi_headers = parseCGIHeaders(headers_str);
            }
        }
    }
    else if (bytes_read == 0) {} // write end in child closed
    else if (bytes_read == -1) {} // read it ta salit

    char err_buffer[65536];
    ssize_t err_bytes_read = read(state.pipe_err[0], err_buffer, sizeof(err_buffer));
    if (err_bytes_read > 0) {
        state.stderr_output.append(err_buffer, err_bytes_read);
        data_read = true;
    }
    else if (err_bytes_read == 0) {} // write end in child closed
    else if (err_bytes_read == -1) {} // read it ta salit
    
    if (!data_read) {
        int status;
        pid_t result = waitpid(state.pid, &status, WNOHANG);
        
        if (result > 0) {
            state.process_complete = true;
            
            if (WIFEXITED(status)) { // child exited
                state.exit_status = WEXITSTATUS(status);
                if (state.exit_status != 0)
                    state.syntax_error = true;
            } 
            else if (WIFSIGNALED(status)) // child killed by parent
                state.syntax_error = true;
        }
        else if (result == -1) {
            state.process_complete = true;
            state.syntax_error = true;
        }
    }
}

std::string CGI::getExtensionFromContentType(const std::string &content_type)
{
    std::map<std::string, std::string>::iterator it = ext_map.find(content_type);
    if (it != ext_map.end())
        return it->second;
    return ".txt";
}

std::string CGI::buildResponseFromState(Server *server, State &state, bool keep_alive, bool &error_status_in_cgi, int &error_status_code_cgi)
{
    bool redirect = false;
    std::string location;
    std::string response = parseCGIOutput(state, redirect, location);
    
    if (redirect)
        return server->buildResponse("", ".html", 302, true, location, keep_alive);
    
    int status_code = 200;
    std::string content_type = ".txt";
    
    for (size_t i = 0; i < state.cgi_headers.size(); i++)
    {
        if (state.cgi_headers[i].first == "status") {
            std::stringstream ss(state.cgi_headers[i].second);
            ss >> status_code;
            if (status_code < 100 || status_code > 599)
                status_code = 200;
        }
        else if (state.cgi_headers[i].first == "content-type")
        {
            std::string type = state.cgi_headers[i].second;
            size_t semicolon = type.find(';');
            if (semicolon != std::string::npos)
                type = type.substr(0, semicolon);
            type = trim(type);

            content_type = getExtensionFromContentType(type);
        }
    }

    if (status_code >= 400) {
        error_status_code_cgi = status_code;
        error_status_in_cgi = true;
        return server->buildResponse(server->buildErrorPage(status_code), content_type, status_code, false, "", keep_alive, state.cgi_headers);
    }
    
    return server->buildResponse(response, content_type, status_code, false, "", keep_alive, state.cgi_headers);
}

std::string CGI::buildErrorResponse(Server *server, State &state)
{
    std::stringstream ss;

    ss << "<!DOCTYPE html>"
        << "<html lang='en'>"
        << "<head>"
        << "<meta charset='UTF-8' />"
        << "<title>Syntax Error</title>"
        << "<style>"
        << "body { margin:0; height:100vh; display:flex; align-items:center; justify-content:center;"
        << "background:#f5f5f5; font-family:Arial, Helvetica, sans-serif; }"
        << ".error-box { padding:24px 32px; background:#fdeaea; color:#a40000;"
        << "border:1px solid #f5c2c2; border-radius:6px; text-align:center; }"
        << "</style>"
        << "</head>"
        << "<body>"
        << "<div class='error-box'>"
        << "<p><strong>"
        << state.stderr_output
        << "</strong>.</p>"
        << "</div>"
        << "</body>"
        << "</html>";

    std::string response = ss.str();
    
    return server->buildResponse(response, ".html", 500, false, "", false);
}

std::string CGI::parseCGIOutput(State &state, bool &redirect, std::string &location)
{
    size_t header_end = state.stdout_output.find("\r\n\r\n");
    if (header_end != std::string::npos)
    {
        for (size_t i = 0; i < state.cgi_headers.size(); i++)
        {
            if (state.cgi_headers[i].first == "location")
            {
                redirect = true;
                location = state.cgi_headers[i].second;
                return "";
            }
        }
        
        return state.stdout_output.substr(header_end + 4);
    }
    
    return state.stdout_output;
}

std::vector<std::pair<std::string, std::string> > CGI::parseCGIHeaders(std::string &headers)
{
    std::vector<std::pair<std::string, std::string> > cgi_headers;
    std::stringstream header_stream(headers);
    std::string line;
    
    while (std::getline(header_stream, line))
    {
        if (line.empty() || (line[0] == '\r' && line[1] == '\n'))
            continue;
            
        size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
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

void CGI::cleanup(State &state, bool kill_process)
{
    if (kill_process && state.pid > 0)
    {
        // std::cerr << "cgi killed\n";
        kill(state.pid, SIGKILL);
        waitpid(state.pid, NULL, 0);
    }
    
    cleanupPipes(state.pipe_in, state.pipe_out, state.pipe_err);
    
    state.pid = -1;
    state.stdout_output.clear();
    state.stderr_output.clear();
    state.cgi_headers.clear();
}

void CGI::cleanupPipes(int pipe_in[2], int pipe_out[2], int pipe_err[2])
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

void CGI::setupEnvironment()
{
    env_vars.clear();

    env_vars.push_back("REQUEST_METHOD=" + request.method);
    env_vars.push_back("SCRIPT_FILENAME=" + script_path); // for php
    env_vars.push_back("SCRIPT_NAME=" + script_path);
    env_vars.push_back("QUERY_STRING=" + request.queries);
    env_vars.push_back("PATH_INFO=");
    env_vars.push_back("PATH_TRANSLATED=");
    env_vars.push_back("REDIRECT_STATUS=200"); // php
    env_vars.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env_vars.push_back("SERVER_PROTOCOL=HTTP/1.0");
    env_vars.push_back("SERVER_SOFTWARE=Webserv/1.0");

    setupAuthEnvironment();
    setupServerEnvironment();
    setupContentEnvironment();
    setupHeadersEnvironment();
    setupPathEnvironment();
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
        env_vars.push_back("AUTH_TYPE=");
}

void CGI::setupServerEnvironment()
{
    std::string server_name = "";
    std::string server_port = "80";
        
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
            server_name = host;
    }

    env_vars.push_back("SERVER_NAME=" + server_name);
    env_vars.push_back("SERVER_PORT=" + server_port);

    std::string remote_addr = "";
    int remote_port = 0;
    if (server) {
        remote_addr = request.remote_addr;
        remote_port = request.remote_port;
    }
    std::stringstream ss;
    ss << remote_port;
    env_vars.push_back("REMOTE_ADDR=" + remote_addr);
    env_vars.push_back("REMOTE_PORT=" + ss.str());
}

void CGI::setupContentEnvironment()
{
    if (request.method == "POST" || request.method == "DELETE")
    {
        if (request.headers.count("content-length"))
            env_vars.push_back("CONTENT_LENGTH=" + request.headers.find("content-length")->second);
        else if (!request.body.empty())
        {
            std::stringstream ss;
            ss << request.body.size();
            env_vars.push_back("CONTENT_LENGTH=" + ss.str());
        }
        else
            env_vars.push_back("CONTENT_LENGTH=0");

        if (request.headers.count("content-type"))
            env_vars.push_back("CONTENT_TYPE=" + request.headers.find("content-type")->second);
    }
    else
    {
        env_vars.push_back("CONTENT_LENGTH=");
        env_vars.push_back("CONTENT_TYPE=");
    }
}

void CGI::setupHeadersEnvironment()
{
    for (std::map<std::string, std::string>::const_iterator it = request.headers.begin(); it != request.headers.end(); it++)
    {
        if (it->first == "authorization" || it->first == "content-length" || it->first == "content-type" || it->first == "connection")
            continue;
        std::string key = "HTTP_" + it->first;
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);
        std::replace(key.begin(), key.end(), '-', '_');
        env_vars.push_back(key + "=" + it->second);
    }
}

void CGI::setupPathEnvironment()
{
    env_vars.push_back("PATH_INFO=");
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
    argv = new char*[3];

    argv[0] = const_cast<char *>(cgi_path.c_str());
    argv[1] = const_cast<char *>(script_path.c_str());
    argv[2] = NULL;
}

int CGI::changeToScriptDirectory()
{
    size_t last_slash = script_path.find_last_of("/");
    if (last_slash != std::string::npos)
    {
        std::string dir = script_path.substr(0, last_slash);
        if (chdir(dir.c_str()) == -1)
        {
            std::cerr << "Error: CGI: Failed to change to directory: " << dir << std::endl;
            return 1;
        }
    }
    return 0;
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