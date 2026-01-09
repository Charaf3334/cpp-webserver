#ifndef CGI_HPP
#define CGI_HPP

#include <sys/wait.h>
#include "Webserv.hpp"
#include "Request.hpp" 

class Server;

class CGI {
    public:
        struct State
        {
            pid_t pid;
            int client_fd;
            int pipe_out[2];
            int pipe_err[2];
            int pipe_in[2];
            std::string stdout_output; 
            std::string stderr_output;
            std::string script_path;
            std::string extension;
            Request request;
            std::vector<std::pair<std::string, std::string> > cgi_headers;
            bool headers_complete;
            bool process_complete;
            bool response_sent_to_client;
            std::string cgi_path;
            bool syntax_error;
            int exit_status;
            State();
        };

    private:
        Server *server;
        Request request;
        std::string script_path;
        std::string extension;
        std::string cgi_path;
        std::vector<std::string> env_vars;
        char **env_cgi;
        char **argv;

        static std::map<std::string, std::string> ext_map;
        
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
        void assignExtension(void);
        void cleanupPipes(int pipe_in[2], int pipe_out[2], int pipe_err[2]);
        
    public:
        CGI(Server *srv, Request &req, std::string &abs_path, std::string &extension);
        ~CGI();
        
        bool start(State &state);
        void handleOutput(State &state);
        void cleanup(State &state, bool kill_process = false);
        static std::string parseCGIOutput(State &state, bool &redirect, std::string &location);
        static std::vector<std::pair<std::string, std::string> > parseCGIHeaders(std::string &headers);      
        static std::string buildResponseFromState(Server *server, State &state, bool keep_alive, bool &error_status_in_cgi, int &error_status_code_cgi);
        static std::string buildErrorResponse(Server *server, State &state);
        static std::string trim(std::string &s);
        static std::string getExtensionFromContentType(const std::string &content_type);
};

#endif