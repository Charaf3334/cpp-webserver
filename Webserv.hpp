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
        };
        struct Server
        {
            int port;
            std::string name;
            std::vector<Location> locations; 
        };

        std::ifstream config_file;
        std::vector<std::string> tokens;
        std::vector<Webserv::Server> servers;
        std::stack<std::string> brackets;
        
        Server parseServer(size_t &i);
        void parseLocation(size_t &i, Webserv::Server &server, int &depth, bool &sawLocation);

        std::string* split(const std::string line);
        size_t countParts(const std::string line) const;
        bool isFileEmpty(void);
        bool checkSemicolon(void) const;
        bool checkValidPort(const std::string s) const;
        bool checkServerName(const std::string s) const;
        void serverDefaultInit(Webserv::Server &server);
        void locationDefaultInit(Location &location);
        bool checkPath(const std::string path) const;
        bool checkRoot(const std::string path) const;
        bool checkForBrackets(void);
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