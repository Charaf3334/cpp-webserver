#ifndef CGI_HPP
#define CGI_HPP

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "Webserv.hpp"
#include "Server.hpp"

class CGI
{
private:
    Server *server; // Pointer to server for using buildResponse
    Server::Request request;
    std::string script_path;
    std::string extension;
    std::string cgi_path;
    std::vector<std::string> env_vars;
    char **env_cgi;
    char **argv;

    // Pipes
    int pipe_in[2];  // Parent -> Child (stdin)
    int pipe_out[2]; // Child -> Parent (stdout)
    int pipe_err[2]; // Child -> Parent (stderr)

    pid_t pid;
    std::string cgi_output;
    std::string error_output;

    // Helper methods
    std::string trim(std::string &s);
    void determineInterpreter();
    void setupPipes();
    void setupEnvironment();
    void setupAuthEnvironment();
    void setupServerEnvironment();
    void setupContentEnvironment();
    void setupHeadersEnvironment();
    void setupPathEnvironment();
    void convertEnvVarsToCharPtr();
    void setupArguments();
    void changeToScriptDirectory();
    void executeChildProcess();
    void executeParentProcess();
    void cleanup();
    std::string parseCGIOutput(std::string &cgi_output);
    std::map<std::string, std::string> parseCGIHeaders(std::string &headers);
    void readFromPipes();
    void handlePipeErrors();

public:
    CGI(Server *srv, Server::Request &req, std::string &abs_path, std::string &extension);
    ~CGI();

    std::string execute(Server::Request &req, std::string &abs_path, std::string &extension);
};

#endif