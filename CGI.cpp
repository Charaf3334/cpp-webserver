#include "CGI.hpp"

std::string CGI::trim(std::string &s)
{
    std::string result = s;
    // Trim left
    size_t start = result.find_first_not_of(" \t\n\r");
    if (start != std::string::npos)
    {
        result = result.substr(start);
    }
    // Trim right
    size_t end = result.find_last_not_of(" \t\n\r");
    if (end != std::string::npos)
    {
        result = result.substr(0, end + 1);
    }
    return result;
}

// Constructor
CGI::CGI(Server *srv, Server::Request &req, std::string &abs_path, std::string &ext)
    : server(srv), request(req), script_path(abs_path), extension(ext),
      env_cgi(NULL), argv(NULL), pid(-1)
{

    // Initialize pipes to invalid values
    pipe_in[0] = pipe_in[1] = -1;
    pipe_out[0] = pipe_out[1] = -1;
    pipe_err[0] = pipe_err[1] = -1;

    if (extension == ".php")
        cgi_path = "/usr/bin/php-cgi";
    else
        cgi_path = "/usr/bin/python3";
}

// Destructor
CGI::~CGI()
{
    cleanup();
}

// Main execution method
std::string CGI::execute(Server::Request &req, std::string &abs_path, std::string &extension)
{
    // Update internal state in case the same CGI instance is reused
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
            executeChildProcess();
            exit(1);
        }
        else
        {
            executeParentProcess();
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

// Set up communication pipes
void CGI::setupPipes()
{
    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1 || pipe(pipe_err) == -1)
    {
        perror("pipe");
        handlePipeErrors();
        throw std::runtime_error("Error: CGI: Failed to create pipes");
    }
}

// Set up environment variables for CGI
void CGI::setupEnvironment()
{
    env_vars.clear();

    env_vars.push_back("REQUEST_METHOD=" + request.method);
    env_vars.push_back("SCRIPT_FILENAME=" + script_path);
    // For now, leave QUERY_STRING empty – can be filled from URI parsing if needed
    env_vars.push_back("QUERY_STRING=");
    env_vars.push_back("REDIRECT_STATUS=200");
    env_vars.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env_vars.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env_vars.push_back("SERVER_SOFTWARE=Webserv/1.0");

    env_vars.push_back("PYTHONUNBUFFERED=1");

    setupAuthEnvironment();
    setupServerEnvironment();
    setupContentEnvironment();
    setupHeadersEnvironment();
    setupPathEnvironment();
}

void CGI::setupAuthEnvironment()
{
    std::map<std::string, std::string>::iterator it = request.headers.find("Authorization");
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

    std::map<std::string, std::string>::iterator it = request.headers.find("Host");
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
    env_vars.push_back("REMOTE_ADDR=127.0.0.1"); // Should be set from actual connection
}

void CGI::setupContentEnvironment()
{
    if (request.method == "POST")
    {
        if (request.headers.count("Content-Length"))
        {
            env_vars.push_back("CONTENT_LENGTH=" + request.headers.find("Content-Length")->second);
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

        if (request.headers.count("Content-Type"))
        {
            env_vars.push_back("CONTENT_TYPE=" + request.headers.find("Content-Type")->second);
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
        std::string key = "HTTP_" + it->first;
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);
        std::replace(key.begin(), key.end(), '-', '_');
        env_vars.push_back(key + "=" + it->second);
    }
}

void CGI::setupPathEnvironment()
{
    // We don't currently keep the original path separate from URI; leave generic values.
    env_vars.push_back("SCRIPT_NAME=");
    env_vars.push_back("PATH_INFO=");
    env_vars.push_back("PATH_TRANSLATED=");
}

void CGI::convertEnvVarsToCharPtr()
{
    env_cgi = new char *[env_vars.size() + 1];
    for (size_t i = 0; i < env_vars.size(); i++)
    {
        env_cgi[i] = const_cast<char *>(env_vars[i].c_str());
    }
    env_cgi[env_vars.size()] = NULL;
}

void CGI::setupArguments()
{
    // Allocate argv array (interpreter + script + NULL)
    argv = new char *[3];

    // argv[0] = cgi_path (interpreter)
    argv[0] = new char[cgi_path.size() + 1];
    std::strcpy(argv[0], cgi_path.c_str());

    // argv[1] = script file
    argv[1] = new char[script_path.size() + 1];
    std::strcpy(argv[1], script_path.c_str());

    // argv[2] = NULL
    argv[2] = NULL;
}

void CGI::changeToScriptDirectory()
{
    size_t last_slash = script_path.find_last_of("/");
    if (last_slash != std::string::npos)
    {
        std::string dir = script_path.substr(0, last_slash);
        if (chdir(dir.c_str()) == -1)
        {
            perror("chdir");
        }
    }
}

void CGI::executeChildProcess()
{
    // Set up pipes for child
    dup2(pipe_in[0], STDIN_FILENO);
    dup2(pipe_out[1], STDOUT_FILENO);
    dup2(pipe_err[1], STDERR_FILENO);

    // Close unused pipe ends in child
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);
    close(pipe_err[0]);
    close(pipe_err[1]);

    // Set up environment and arguments
    setupEnvironment();
    convertEnvVarsToCharPtr();
    setupArguments();
    changeToScriptDirectory();

    // Execute the CGI script
    execve(cgi_path.c_str(), argv, env_cgi);

    // If execve fails, clean up and exit
    delete[] argv[0];
    delete[] argv[1];
    delete[] argv;
    delete[] env_cgi;

    exit(1);
}

void CGI::executeParentProcess()
{
    // Close unused pipe ends in parent
    close(pipe_in[0]);
    close(pipe_out[1]);
    close(pipe_err[1]);

    // Write request body to CGI stdin if POST method
    if (request.method == "POST" && !request.body.empty())
    {
        ssize_t bytes_written = write(pipe_in[1], request.body.c_str(), request.body.size());
        if (bytes_written != static_cast<ssize_t>(request.body.size()))
        {
            std::cerr << "Warning: Could not write full body to CGI stdin" << std::endl;
        }
    }
    close(pipe_in[1]);

    // Read output from pipes
    readFromPipes();

    // Wait for child process to finish
    int status;
    waitpid(pid, &status, 0);

    // Log errors if any
    if (!error_output.empty())
    {
        std::cerr << "CGI stderr: " << error_output << std::endl;
    }
}

void CGI::readFromPipes()
{
    char buffer[4096];
    ssize_t bytes_read;

    // Read from stdout
    while ((bytes_read = read(pipe_out[0], buffer, sizeof(buffer))) > 0)
    {
        cgi_output.append(buffer, bytes_read);
    }
    close(pipe_out[0]);

    // Read from stderr
    while ((bytes_read = read(pipe_err[0], buffer, sizeof(buffer))) > 0)
    {
        error_output.append(buffer, bytes_read);
    }
    close(pipe_err[0]);
}

std::string CGI::parseCGIOutput(std::string &cgi_output)
{
    size_t header_end = cgi_output.find("\r\n\r\n");
    size_t delimiter_len = 4;
    if (header_end == std::string::npos) // why is that
    {
        std::cerr << "didn't find /r/n/r/n\n";
        header_end = cgi_output.find("\n\n");
        delimiter_len = 2;
    }

    if (header_end != std::string::npos)
    {
        std::string headers_str = cgi_output.substr(0, header_end);
        std::string body = cgi_output.substr(header_end + delimiter_len);
        
        std::map<std::string, std::string> cgi_headers = parseCGIHeaders(headers_str);
        // print cgi headers
        std::cerr << "\nCGI HEADERS:\n";
        for (std::map<std::string, std::string>::const_iterator it = cgi_headers.begin(); it != cgi_headers.end(); it++)
        {
            std::cerr << "key: |" << it->first
                    << "| value: |" << it->second << "|" << std::endl;
        }


        // Parse Status header
        int status_code = 200;
        std::map<std::string, std::string>::iterator status_it = cgi_headers.find("status");
        if (status_it != cgi_headers.end())
        {
            std::istringstream ss(status_it->second);
            ss >> status_code;
            if (status_code < 100 || status_code > 599)
                status_code = 200;
        }

        // Handle Location header (redirect)
        std::map<std::string, std::string>::iterator location_it = cgi_headers.find("location");
        if (location_it != cgi_headers.end())
        {
            std::cerr << "test1\n";
            return server->buildResponse("", ".html", 302, true, location_it->second, request.keep_alive);
        }

        // Determine extension based on Content-Type header or use script extension
        std::string ext = ".txt";
        std::map<std::string, std::string>::iterator content_type_it = cgi_headers.find("content-type");
        if (content_type_it != cgi_headers.end())
        {
            std::string ct = content_type_it->second;
            // use map lli fcontent type
            if (ct == "text/html") {
                ext = ".html";
            }
            else if (ct.find("text/plain") != std::string::npos)
                ext = ".txt";
            else if (ct.find("application/json") != std::string::npos)
                ext = ".json";
        }

        return server->buildResponse(body, ext, status_code, false, "", request.keep_alive);
    }
    else
    {
        // No headers found, treat entire output as body
        return server->buildResponse(cgi_output, ".txt", 200, false, "", request.keep_alive);
    }
}

std::map<std::string, std::string> CGI::parseCGIHeaders(std::string &headers)
{
    std::map<std::string, std::string> cgi_headers;
    std::istringstream header_stream(headers);
    std::string line;

    while (std::getline(header_stream, line))
    {
        size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // Trim whitespace
            key = trim(key);
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            size_t semicolon = value.find(';');
            if (semicolon != std::string::npos)
                value = value.substr(0, semicolon);
            value = trim(value);


            cgi_headers[key] = value;
        }
    }

    return cgi_headers;
}

void CGI::cleanup()
{
    // Clean up allocated memory
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

    // Close any open pipes
    handlePipeErrors();
}

void CGI::handlePipeErrors()
{
    // Close all pipes if they're open
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