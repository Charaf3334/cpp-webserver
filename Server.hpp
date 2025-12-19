#ifndef SERVER_HPP
#define SERVER_HPP

#include "Webserv.hpp"

#define MAX_EVENTS 64

class Server : public Webserv
{
    public:
        struct Request
        {
            std::string method;
            std::string uri;
            std::string http_version;
            std::map<std::string, std::string> headers;
            bool keep_alive;
            std::string body;
            std::string remote_addr; // zakaria
            int remote_port; // zakaria
        };
    private:
        struct ClientState
        {
            std::string pending_response;
            size_t bytes_sent;
            bool keep_alive;
        };
        struct client_read
        {
            std::string request;
            size_t headers_end;
            size_t content_len;
            bool is_request_full;
            bool isParsed;
            bool content_lenght_present;
            struct timeval start_time;
            bool has_start_time;
        };
        std::map<int, sockaddr_in> client_addresses; // zakaria
        static Server* instance;
        std::vector<int> socket_fds;
        std::vector<int> client_fds;
        std::map<int, Webserv::Server*> sockfd_to_server;
        std::map<int, Webserv::Server*> clientfd_to_server; // KEY=FD this is the map we will use to serve clients per servers
        std::map<int, ClientState> client_states;
        std::map<int, client_read> read_states;
        bool shutdownFlag;

        void checkTimeoutClients(int epoll_fd);
        bool setNonBlockingFD(const int fd) const;
        sockaddr_in infos(const Webserv::Server server) const;
        void closeSockets(void);
        static void handlingSigint(int sig);
        std::string readRequest(int client_fd);
        bool parseRequest(int client_fd, std::string request_string, Server::Request &request);
        std::string readFile(const std::string file_path) const;
        std::string getExtension(std::string file_path);
        std::vector<std::string> getheadersLines(const std::string req, bool &flag, int &error_status, std::string &body);
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
        void closeClient(int epoll_fd, int client_fd, bool inside_loop);
        bool sendResponse(int client_fd, const std::string response, bool keep_alive);
        bool continueSending(int client_fd);
        void modifyEpollEvents(int epoll_fd, int client_fd, unsigned int events);

    public:
        std::string buildResponse(std::string body, std::string extension, int status, bool inRedirection, std::string newPath, bool keep_alive, const std::vector<std::pair<std::string, std::string> > &extra_headers = std::vector<std::pair<std::string, std::string> >());

        std::string buildErrorPage(int code);
        
        Server(Webserv webserv);
        Server(const Server &theOtherObject);
        Server& operator=(const Server &theOtherObject);
        ~Server();
        void initialize(void);
};

#endif