#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>
#include <iterator>
#include <cstdlib>

class Webserv
{
    private:
        std::map<std::string, std::vector<std::string> > Data; // hadi ghaliban athyd

        struct Location
        {
            std::string path;
            std::string root;
            std::vector<std::string> methods;
            bool autoindex;
        };
        struct Server
        {
            int port;
            std::vector<std::string> names;
            std::vector<Location> locations; 
        };

        std::ifstream config_file;
        std::vector<std::string> tokens;
        std::vector<Webserv::Server> servers;

        size_t countParts(const std::string line) const;
        bool isFileEmpty(void);
    public:
        Webserv();
        Webserv(const std::string config_file_path);
        Webserv(const Webserv &theOtherObject);
        Webserv& operator=(const Webserv &theOtherObject);
        ~Webserv();

        std::string* split(const std::string line);
        void read_file(void);
        Server parseServer(size_t &i, int &brackets_flag);
};

#endif