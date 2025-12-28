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
#include "Request.hpp" 

class Server;

class CGI{
    public:
        struct State
        {
            pid_t pid;
            int client_fd;
            int pipe_out[2];    // CGI stdout -> server
            int pipe_err[2];    // CGI stderr -> server
            int pipe_in[2];     // server -> CGI stdin (POST)
            std::string output;
            std::string script_path;
            std::string extension;
            Request request;
            std::vector<std::pair<std::string, std::string> > cgi_headers;
            bool headers_complete;
            bool process_complete;
            bool response_sent_to_client;
            time_t start_time;
            std::string cgi_path;
            bool syntax_error;
            
            State() : pid(-1), client_fd(-1), headers_complete(false), process_complete(false), response_sent_to_client(false), start_time(0), syntax_error(false)
            {
                pipe_out[0] = pipe_out[1] = -1;
                pipe_err[0] = pipe_err[1] = -1;
                pipe_in[0] = pipe_in[1] = -1;
            }
        };

    private:
        Server *server; // Pointer to server for using buildResponse
        Request request;
        std::string script_path;
        std::string extension;
        std::string cgi_path;
        std::vector<std::string> env_vars;
        char **env_cgi;
        char **argv;
        
        std::string cgi_output;
        std::string error_output;

        static std::map<std::string, std::string> ext_map;
        
        void setupPipes();
        void setupEnvironment();
        void setupAuthEnvironment();
        void setupServerEnvironment();
        void setupContentEnvironment();
        void setupHeadersEnvironment();
        void setupPathEnvironment();
        void convertEnvVarsToCharPtr();
        void setupArguments();
        int  changeToScriptDirectory();
        void childProcess(int pipe_in[2], int pipe_out[2], int pipe_err[2]);
        void parentProcess();
        std::string parseCGIOutput(std::string &cgi_output);
        void readFromPipes();
        void handlePipeErrors();
        void assignExtension(void);
        void cleanup();
        void cleanupPipes(int pipe_in[2], int pipe_out[2], int pipe_err[2]);
        
    public:
        CGI(Server *srv, Request &req, std::string &abs_path, std::string &extension);
        ~CGI();
        
        // non blocking methods
        bool start(State &state);
        bool handleOutput(State &state);
        void cleanup(State &state, bool kill_process = false);
        static std::string parseCGIOutput(State &state, bool &redirect, std::string &location);
        static std::vector<std::pair<std::string, std::string> > parseCGIHeaders(std::string &headers);
        
        static std::string buildResponseFromState(Server *server, State &state, bool keep_alive);
        static std::string buildErrorResponse(Server *server, State &state);
        static std::string trim(std::string &s);

        static std::string getExtensionFromContentType(const std::string &content_type);
};

#endif