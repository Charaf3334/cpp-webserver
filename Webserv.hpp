#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <stack>
#include <algorithm>
#include <iterator>
#include <cstdlib>
#include <set>
#include <sstream>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <fcntl.h>
#include <dirent.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <ctime>

class Webserv 
{
    public:
        struct Location
        {
            std::string path;
            std::string root;
            std::vector<std::string> methods;
            bool autoindex;
            std::vector<std::string> index;
            bool isRedirection;
            bool redirectionIsText;
            bool redirect_relative;
            bool redirect_absolute;
            std::pair<int, std::string> redirection;
            std::string upload_dir;
            bool hasCgi;
            std::map<size_t, std::string> error_pages; // each location error pages
        };
    protected:
        struct Server
        {
            std::string ip_address;
            int port;
            std::vector<Location> locations; 
            std::string root;
        };
        std::ifstream config_file;
        std::vector<std::string> tokens;
        std::vector<Webserv::Server> servers;
        std::map<std::string, std::string> error_pages; // global ones
        size_t client_max_body_size; // values are in bytes
        std::stack<std::string> brackets;
        std::string http_root;
        std::map<int, std::string> status_codes;
        std::map<std::string, std::string> content_type;
        
        Server parseServer(size_t &i);
        void parseLocation(size_t &i, Webserv::Server &server, int &depth, bool &sawLocation);
        void mergePaths(void);
        std::string* split(const std::string line);
        size_t countParts(const std::string line) const;
        bool isFileEmpty(void);
        bool checkSemicolon(void) const;
        bool checkValidListen(const std::string s, bool &isHost) const;
        int isValidIp(const std::string ip) const;
        bool checkHost(const std::string host, bool &isHost) const;
        std::string convertHostToIp(const std::string host, const std::string message);
        void serverDefaultInit(Webserv::Server &server);
        void locationDefaultInit(Location &location);
        bool checkPath(const std::string path) const;
        bool checkRoot(const std::string path) const;
        bool checkForBrackets(void);
        bool checkDuplicatePorts(void) const;
        bool checkDuplicatePaths(void);
        bool checkFileExtension(const std::string path, const std::string extension) const;
        bool checkMaxBodySize(const std::string value);
        bool checkStatusCode(const std::string code) const;
        bool checkUrlText(size_t i, Location &location, bool code_present, int status_code);
        bool isstrdigit(std::string str) const;
        bool isValidStatusCode(const std::string code);
        std::vector<std::string> semicolonBracketsFix(const std::vector<std::string> input);
        void assignStatusCodes(void);
        bool isCodeInMap(int code);
        void assignContentType(void);
        void print_conf(void);
        void sortLocationPaths(Webserv::Server &server);
        static bool locationCmp(const Webserv::Location &l1, const Webserv::Location &l2);
        std::string getAddress(sockaddr_in *addr);
    public:
        Webserv();
        Webserv(const std::string config_file_path);
        Webserv(const Webserv &theOtherObject);
        Webserv& operator=(const Webserv &theOtherObject);
        ~Webserv();

        void read_file(void);
};

#endif