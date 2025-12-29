#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>
#include <vector>
#include "Webserv.hpp"

struct Request
{
    std::string method;
    std::string uri;
    std::string http_version;
    std::map<std::string, std::string> headers;
    bool keep_alive;
    bool is_uri_dir;
    bool is_uri_reg;
    std::string body;
    std::map<std::string, std::string> body_headers;
    std::string real_body;
    std::string body_boundary;
    std::string bodyfile_name;
    std::string remote_addr;
    int remote_port;
    bool take_boundry_check_upload_dir;
    bool body_headers_done;
    Webserv::Location location;
};

#endif