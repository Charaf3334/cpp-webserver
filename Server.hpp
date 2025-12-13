#ifndef SERVER_HPP
#define SERVER_HPP

#include "Webserv.hpp"

#define MAX_EVENTS 64

class Server : public Webserv
{
    private:
        struct Request
        {
            std::string method;
            std::string uri;
            std::string http_version;
            std::map<std::string, std::string> headers;
            std::string body;
        };
        static Server* instance;
        std::vector<int> socket_fds;
        std::map<int, Webserv::Server*> sockfd_to_server;
        std::map<int, Webserv::Server*> clientfd_to_server; // KEY=FD this is the map we will use to serve clients per servers
        bool shutdownFlag;
        bool setNonBlockingFD(const int fd) const;
        sockaddr_in infos(const Webserv::Server server) const;
        void closeSockets(void);
        static void handlingSigint(int sig);
        std::string readRequest(int client_fd);
        bool parseRequest(int client_fd, std::string request_string, Server::Request &request);
        std::string readFile(const std::string file_path) const;
        std::string getExtension(std::string file_path);
        std::string buildResponse(std::string file_content, std::string extension, int status, bool inRedirection, std::string newPath);
        std::vector<std::string> getheadersLines(const std::string req, bool &flag, int &error_status);
        bool parse_lines(std::vector<std::string> lines, Server::Request &request, int &error_status);
        bool parse_headers(std::string &str, Server::Request &request, int &error_status);
        std::string str_tolower(std::string str);
        bool check_allowedfirst(std::string &first);
        bool parse_methode(std::string *words, int &error_status, Server::Request &request);
        bool parse_path(std::string &path);
        std::string tostring(size_t num) const;
        bool serveClient(int client_fd, Server::Request request);
        bool isUriExists(std::string uri, Webserv::Server server, bool flag) const;
        Webserv::Location getLocation(std::string uri, Webserv::Server server);
        bool atleastOneFileExists(Webserv::Location location) const;
        std::string getFilethatExists(Webserv::Location location) const;
        bool isMethodAllowed(std::string method, Webserv::Location location) const;
    public:
        Server(Webserv webserv);
        Server(const Server &theOtherObject);
        Server& operator=(const Server &theOtherObject);
        ~Server();
        void initialize(void);
};

#endif