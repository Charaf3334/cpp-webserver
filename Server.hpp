#ifndef SERVER_HPP
#define SERVER_HPP

#include "Webserv.hpp"
#include "CGI.hpp"

#define MAX_EVENTS 64
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

class Server : public Webserv
{
    private:
        struct ClientState
        {
            std::string pending_response;
            size_t bytes_sent;
            bool keep_alive;
            bool is_streaming;
            int file_fd;
            size_t file_offset;
            size_t file_size;
            char buffer[8192];
            size_t buffer_offset;
            size_t buffer_len;
            std::string response_headers;
            size_t headers_sent;
            bool headers_complete;
        };
        struct client_read
        {
            bool is_request_full;
            bool isParsed;
            bool content_lenght_present;
            bool has_start_time;
            bool is_post;
            bool boundary_found;
            bool end_boundary_found;
            int file_fd;
            ssize_t bytes;
            size_t total_bytes_written;
            size_t content_len;
            std::string body_buffer;
            std::string headers;
            std::string temporary_body;
            std::string to_write;
            char buffer[20480];
            time_t start_time;
            Request request;
            bool keep_alive;
            int loop_counter;
            bool packet_ended;
            bool first_call;
        };
        struct CgiState
        {
            CGI::State state;
            time_t start_time;
            bool added_to_epoll;
            CgiState();
        };
        std::map<int, sockaddr_in> client_addresses; // zakaria
        static Server* instance;
        std::vector<int> socket_fds;
        std::vector<int> client_fds;
        std::map<int, Webserv::Server*> sockfd_to_server;
        std::map<int, Webserv::Server*> clientfd_to_server; // KEY=FD this is the map we will use to serve clients per servers
        std::map<int, ClientState> client_states;
        std::map<int, client_read> read_states;
        std::map<int, Request> clientfd_to_request;
        bool shutdownFlag;
        std::vector<int> fileFdstoClose;
        std::map<int, CgiState> cgi_states; // Key:pipe_out[0] (read end)

        bool fileValid(std::string path);
        bool isRequestLineValid(std::string request_line);
        unsigned int getBinaryAddress(std::string address);
        bool allUppercase(std::string method);
        bool validURI(std::string uri);
        std::string decodeURI(std::string uri);
        std::vector<std::string> get_bodyheaders_Lines(const std::string req);
        std::string simplifyPath(std::string path);
        void checkTimeoutClients(int epoll_fd);
        std::string currentDate(void) const;
        bool setNonBlockingFD(const int fd) const;
        sockaddr_in infos(const Webserv::Server server);
        void closeSockets(void);
        static void handlingSigint(int sig);
        std::string readRequest(int epoll_fd, int client_fd, bool &been_here);
        std::string _trim(std::string str) const;
        bool isContentLengthValid(std::string value);
        bool parseRequest(int client_fd, std::string request_string, Request &request, bool inside_read_request, bool &been_here);
        std::string getExtension(std::string file_path);
        std::vector<std::string> getheadersLines(const std::string req, bool &flag, int &error_status, std::string &body);
        bool parse_lines(std::vector<std::string> lines, Request &request, int &error_status);
        bool parse_headers(std::string &line, std::map<std::string, std::string> &map, int &error_status, int option, const std::string boundary);
        std::string str_tolower(std::string str);
        bool check_allowedfirst(std::string &first);
        bool parse_methode(std::string *words, int &error_status, Request &request);
        bool parse_path(std::string &path);
        std::string tostring(size_t num) const;
        bool serveClient(int client_fd, Request request, int epoll_fd);
        bool isUriExists(std::string uri, Webserv::Server server, bool flag) const;
        Webserv::Location getLocation(std::string uri, Webserv::Server server);
        bool atleastOneFileExists(Webserv::Location location) const;
        std::string getFilethatExists(Webserv::Location location) const;
        bool isMethodAllowed(std::string method, Webserv::Location location) const;
        void closeClient(int epoll_fd, int client_fd, bool inside_loop);
        bool sendResponse(int client_fd, const std::string response, bool keep_alive);
        bool continueSending(int client_fd);
        void modifyEpollEvents(int epoll_fd, int client_fd, unsigned int events);
        bool sendFileResponse(int client_fd, const std::string file_path, const std::string extension, int status, bool keep_alive, const std::vector<std::pair<std::string, std::string> > &extra_headers = std::vector<std::pair<std::string, std::string> >());
        bool setupCGI(Request &request, std::string &script_path, std::string &extension, int client_fd, int epoll_fd);
        void handleCGIOutput(int epoll_fd, int pipe_fd);
        void cleanupCGI(int epoll_fd, int pipe_fd, bool kill_process = false);
        void checkTimeoutCGI(int epoll_fd);
        std::string buildSearchingFile(std::string root, std::string uri, Location location);
        void mergeIndexes(Location &location, std::string toSearch);

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