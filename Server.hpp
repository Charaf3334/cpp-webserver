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
        bool shutdownFlag;
        bool setNonBlockingFD(const int fd) const;
        sockaddr_in infos(const Webserv::Server server) const;
        void closeSockets(void);
        static void handlingSigint(int sig);
        std::string readRequest(int client_fd);
        std::vector<std::string> splitRequest(std::string request_string);
        bool parseRequest(int client_fd, std::string request_string, Server::Request request);
        std::string readFile(const std::string file_path, int &status) const;
        std::string getExtension(std::string file_path);
        std::string buildResponse(std::string file_content, std::string extension, int status);
        std::string tostring(size_t num) const;
    public:
        Server(Webserv webserv);
        Server(const Server &theOtherObject);
        Server& operator=(const Server &theOtherObject);
        ~Server();
        void initialize(void);
};

#endif