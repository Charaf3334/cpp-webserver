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


class Webserv 
{
    private:
        struct Location
        {
            std::string path;
            std::string root;
            std::vector<std::string> methods;
            bool autoindex;
            std::vector<std::string> index;
            bool isRedirection;
            bool redirectionIsText;
            std::map<int, std::string> redirection;
            std::string upload_dir;
        };
        struct Server
        {
            std::string ip_address;
            int port;
            std::string name;
            std::vector<Location> locations; 
            std::string root;
        };

        std::ifstream config_file;
        std::vector<std::string> tokens;
        std::vector<Webserv::Server> servers;
        std::map<std::string, std::string> error_pages;
        std::string client_max_body_size;
        std::stack<std::string> brackets;
        
        Server parseServer(size_t &i);
        void parseLocation(size_t &i, Webserv::Server &server, int &depth, bool &sawLocation);

        std::string* split(const std::string line);
        size_t countParts(const std::string line) const;
        bool isFileEmpty(void);
        bool checkSemicolon(void) const;
        bool checkValidListen(const std::string s, bool &isHost) const;
        bool isValidIp(const std::string ip) const;
        bool checkHost(const std::string host, bool &isHost) const;
        std::string convertHostToIp(const std::string host); 
        // bool checkServerName(const std::string s) const;
        void serverDefaultInit(Webserv::Server &server);
        void locationDefaultInit(Location &location);
        bool checkPath(const std::string path) const;
        bool checkRoot(const std::string path) const;
        bool checkForBrackets(void);
        bool checkDuplicatePorts(void) const;
        bool checkDuplicatePaths(void);
        bool checkFileExtension(const std::string path) const;
        bool checkMaxBodySize(const std::string value);
        bool checkStatusCode(const std::string code) const;
        bool checkUrlText(size_t i, Location &location) const;
        std::vector<std::string> semicolonBracketsFix(const std::vector<std::string> input);
    public:
        Webserv();
        Webserv(const std::string config_file_path);
        Webserv(const Webserv &theOtherObject);
        Webserv& operator=(const Webserv &theOtherObject);
        ~Webserv();

        void read_file(void);
};

#endif