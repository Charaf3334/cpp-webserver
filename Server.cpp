#include "Server.hpp"
#include "CGI.hpp"

Server::Server(Webserv webserv) : Webserv(webserv)
{
    instance = this;
    shutdownFlag = false;
    signal(SIGINT, Server::handlingSigint);
    signal(SIGPIPE, SIG_IGN);
}

Server::Server(const Server &theOtherObject) : Webserv(theOtherObject)
{
   
}

Server& Server::operator=(const Server &theOtherObject)
{
    if (this != &theOtherObject)
        static_cast<void>(theOtherObject);
    return *this;
}

Server::~Server()
{

}

bool Server::setNonBlockingFD(const int fd) const
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
    {
        std::cerr << "Error: setting fd to non-blocking mode." << std::endl;
        return false;
    }
    return true;
}

sockaddr_in Server::infos(const Webserv::Server server) const
{
    sockaddr_in address;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(server.ip_address.c_str());
    address.sin_port = htons(server.port);
    return address;
}

void Server::closeSockets(void)
{
    for (size_t i = 0; i < socket_fds.size(); i++)
        close(socket_fds[i]);
}

void Server::handlingSigint(int sig)
{
    static_cast<void>(sig);
    if (instance)
    {
        std::cout << std::endl;
        instance->shutdownFlag = true;
    }
}

std::string Server::readFile(const std::string file_path) const
{
    std::ifstream file(file_path.c_str());
    std::stringstream content;

    content << file.rdbuf();
    file.close();
    return content.str();
}

std::string Server::getExtension(std::string file_path)
{
    size_t dot = file_path.rfind('.');
    if (dot == std::string::npos)
        return ".txt";
    return file_path.substr(dot);
}

std::string Server::tostring(size_t num) const
{
    std::stringstream strnum;

    strnum << num;
    return strnum.str();
}

std::string Server::buildResponse(std::string file_content, std::string extension, int status, bool inRedirection, std::string newPath, bool keep_alive)
{
    std::string CRLF = "\r\n";
    std::string start_line = "HTTP/1.0 " + tostring(status) + " " + status_codes[status] + CRLF;
    std::string type;
    if (this->content_type.find(extension) == this->content_type.end())
        type = "text/plain";
    else
        type = this->content_type[extension];

    std::string content_type = "Content-Type: " + type + CRLF;
    std::string location;
    if (inRedirection)
        location = "Location: " + newPath + CRLF;
    std::string content_length = "Content-Length: " + tostring(file_content.length()) + CRLF;
    std::string connection = std::string("Connection: ") + (keep_alive ? "keep-alive" : "close") + CRLF + CRLF; // two CRLF mean ending the headers , still need to check what does this mean and when to make it keep-alive vs close
    std::string response = start_line + content_type + location + content_length + connection + file_content;
    return response;
}

std::string Server::readRequest(int client_fd)
{
    client_read &client_ref = read_states[client_fd];
    char temp_buffer[4096];
    ssize_t bytes;
    client_ref.isParsed = false;
    client_ref.content_lenght_present = false;

    while (true)
    {
        bytes = read(client_fd, temp_buffer, sizeof(temp_buffer));
        if (bytes <= 0) // connection closed or error
        {
            if (bytes == 0)
                std::cerr << "User Disconnected" << std::endl;
            break;
        }
        client_ref.request.append(temp_buffer, bytes);
        if (!client_ref.isParsed)
        {
            size_t pos = client_ref.request.find("\r\n\r\n");
            if (pos != std::string::npos)
            {
                client_ref.isParsed = true;
                client_ref.headers_end = pos;
                std::string header = client_ref.request.substr(0, pos);
                size_t idx = header.find("Content-Length:");
                if (idx != std::string::npos && idx < pos) // ensure it's in headers
                {
                    size_t line_end = client_ref.request.find("\r\n", idx);
                    if (line_end != std::string::npos)
                    {
                        client_ref.content_lenght_present = true;
                        std::string value = client_ref.request.substr(idx + 15, line_end - (idx + 15));
                        size_t first = value.find_first_not_of(" \t");
                        if (first != std::string::npos)
                            value = value.substr(first);
                        client_ref.content_len = std::atoll(value.c_str());
                    }
                }
            }
        }
        if (client_ref.isParsed)
        {
            if (!client_ref.content_lenght_present)
            {
                client_ref.is_request_full = true;
                break;
            }
            size_t total_expected = client_ref.headers_end + 4 + client_ref.content_len;
            if (client_ref.request.size() >= total_expected)
            {
                client_ref.is_request_full = true;
                break;
            }
        }
        if (bytes < static_cast<ssize_t>(sizeof(temp_buffer)))
            break;
    }
    return client_ref.request;
}

std::vector<std::string> Server::getheadersLines(const std::string req, bool &flag, int &error_status, std::string &body)
{
    std::string sub_req;
	std::vector<std::string> lines;
	size_t pos = req.find("\r\n\r\n");
	if (pos == std::string::npos)
    {
        flag = false;
        error_status = 400;
		return lines;
    }
	if (pos == 0)
    {
		flag = false;
        error_status = 400;
		return lines;
    }
	sub_req = req.substr(0, pos);
	int start = 0;
	for (size_t j = 0; j < sub_req.size(); j++)
	{
		if ((sub_req[j] == '\r' && sub_req[j + 1] == '\n') || j + 1 == sub_req.size())
		{
			lines.push_back(sub_req.substr(start, (j + 1 == sub_req.size()) ? j - start + 1 : j - start));
			j++;
			start = j + 1;
		}
		else if (!std::isprint(sub_req[j]))
        {
			flag = false;
            error_status = 400;
		    return lines;
        }
	}
	// if (lines.size() == 1) // hadi khsha t7yd bach ndwzo GET / HTTP/1.0\r\n\r\n
    // {
	// 	flag = false;
    //     error_status = 400;
	// 	return lines;
    // }
    body = req.substr(pos + 4);
	return lines;
}

bool Server::parse_path(std::string &path)
{
    if (path.empty() || path[0] != '/')
		return false;
	if (path.find("//") != std::string::npos)
		return false;
    for (size_t i = 0; i < path.length(); i++)
    {
        if (!isalnum(path[i]) && path[i] != '/' && path[i] != '_' && path[i] != '-' && path[i] != '.')
            return false;
    }
    return true;
}

bool Server::parse_methode(std::string *words, int &error_status, Server::Request &request)
{
    std::string http_versions[] = {"HTTP/0.9", "HTTP/1.0", "HTTP/1.1", "HTTP/2.0", "HTTP/3.0"};
    size_t http_versions_length = sizeof(http_versions) / sizeof(http_versions[0]);
	std::string http_methodes[] = {"GET", "POST", "DELETE", "PATCH", "PUT", "HEAD", "OPTIONS"};
    size_t http_methodes_length = sizeof(http_methodes) / sizeof(http_methodes[0]);
	for (size_t j = 0; j < http_methodes_length; j++)
    {
		if (words[0] == http_methodes[j])
			break;
		if (j == 6)
        {
			error_status = 400;
			return false;
		}
	}
    if (words[0] == "PATCH" || words[0] == "PUT" || words[0] == "HEAD" || words[0] == "OPTIONS")
    {
        error_status = 501;
        return false;
    }
	if (words[0] != "GET" && words[0] != "POST" && words[0] != "DELETE")
    {
		error_status = 400;
		return false;
	}
	if (!parse_path(words[1]))
    {
		error_status = 400;
		return false;
	}
	for (size_t i = 0; i < http_versions_length; i++)
    {
		if (words[2] == http_versions[i])
			break;
		if (i == 4)
        {
			error_status = 400;
			return false;
		}
	}
	if (words[2] != "HTTP/1.0" && words[2] != "HTTP/1.1")
    {
		error_status = 400;
		return false;
	}
    request.method = words[0];
    request.uri = words[1];
    request.http_version = words[2];
	return true;
}

std::string Server::str_tolower(std::string str)
{
    std::string result = str;
	for (size_t i = 0; i < str.size(); i++)
		result[i] = std::tolower((unsigned char)str[i]);
	return result;
}

bool Server::check_allowedfirst(std::string &first)
{
    // std::string allowed_first = "!#$%&'*+-.^_`|~:";
	for (size_t i = 0; i < first.size(); i++)
	{
		if (isalnum(first[i]) || first[i] == '-' || first[i] == ':')
			continue ;
		else
			return false;
		// for (int j = 0; j < allowed_first.size(); j++)
		// {
		// 	if (first[i] == allowed_first[j])
		// 		break;
		// 	if (j == allowed_first.size() - 1)
		// 		return false;
		// }
	}
	return true;
}

bool Server::parse_headers(std::string &line, Server::Request &request, int &error_status)
{
    if (line.empty())
    {
        error_status = 400;
		return false;
    }
	size_t pos = line.find(':');
	if (pos == std::string::npos)
    {
        error_status = 400;
		return false;
    }
	if (pos == line.size() - 1)
    {
        error_status = 400;
		return false;
    }
	if (line[pos + 1] == ':')
    {
        error_status = 400;
		return false;
    }
	std::string first = line.substr(0, pos + 1);
	first = str_tolower(first);
	if (first.size() == 1 || !check_allowedfirst(first))
    {
        error_status = 400;
		return false;
    }
    first = first.substr(0, pos);
    pos++;
    while (line[pos] && line[pos] == ' ')
        pos++;
    if (line[pos] == ':')
    {
        error_status = 400;
        return false;
    }
    std::string second = line.substr(pos);
    request.headers[first] = second;
	return true;
}


bool Server::parse_lines(std::vector<std::string> lines, Server::Request &request, int &error_status)
{
    std::string *words;

	for (size_t i = 0; i < lines.size(); i++)
	{
        if (i == 0)
        {
            size_t size = countParts(lines[i]);
            words = split(lines[i]);
            if (size != 3)
            {
                error_status = 400;
                delete[] words;
                return false;
            }
            else
            {
                if (!parse_methode(words, error_status, request))
                {
                    delete[] words;
                    return false;
                }
            }
            delete[] words;
        }
		else if (i > 0)
		{
            if (!parse_headers(lines[i], request, error_status))
				return false;
		}
	}
    if (request.method == "POST")
    {
        std::map<std::string, std::string>::iterator found = request.headers.find("content-length");
        found = request.headers.find("content-length");
        if (found == request.headers.end())
        {
            error_status = 400;
            return false;
        }
    }
	return true;
}

bool Server::parseRequest(int client_fd, std::string request_string, Server::Request &request) // this is the core function that processes the http requests our webserver receives
{
    bool flag = true;
    int error_status = 0;
    std::string body;
    std::vector<std::string> lines = getheadersLines(request_string, flag, error_status, body);
    if (!flag)
    {
        std::string response = buildResponse(buildErrorPage(error_status), ".html", error_status, false, "", false);
        return sendResponse(client_fd, response, false);
    }
    if (!parse_lines(lines, request, error_status))
    {
        std::string response = buildResponse(buildErrorPage(error_status), ".html", error_status, false, "", false);
        return sendResponse(client_fd, response, false);
    }
    if (!isstrdigit(request.headers["content-length"])) // check content-length
    {
        std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
        return sendResponse(client_fd, response, false);
    }
    request.body = body.substr(0, atoll(request.headers["content-length"].c_str()));
    std::map<std::string, std::string>::iterator found = request.headers.find("connection");
    request.keep_alive = false;
    if (found != request.headers.end() && found->second == "keep-alive")
        request.keep_alive = true;
    return true;
}

bool Server::isUriExists(std::string uri, Webserv::Server server, bool flag) const
{
    for (size_t i = 0; i < server.locations.size(); i++)
    {
        if (server.locations[i].path == uri)
            return true;
    }
    if (!flag)
    {
        for (size_t i = 0; i < server.locations.size(); i++)
        {
            if (uri.compare(0, server.locations[i].path.length(), server.locations[i].path) == 0)
                return true;
        }
    }
    return false;
}

Webserv::Location Server::getLocation(std::string uri, Webserv::Server server)
{
    Webserv::Location location;

    for (size_t i = 0; i < server.locations.size(); i++)
    {
        if (server.locations[i].path == uri)
            return server.locations[i];
    }
    for (size_t i = 0; i < server.locations.size(); i++)
    {
        if (uri.compare(0, server.locations[i].path.length(), server.locations[i].path) == 0)
        {
            location = server.locations[i];
            break;
        }
    }
    return location;
}

bool Server::atleastOneFileExists(Webserv::Location location) const
{
    size_t counter = 0;
    struct stat st;

    for (size_t i = 0; i < location.index.size(); i++)
    {
        if (stat(location.index[i].c_str(), &st) == -1)
        {
            counter++;
            continue;
        }
        if (S_ISDIR(st.st_mode))
            counter++;
    }
    return !(counter == location.index.size());
}

std::string Server::getFilethatExists(Webserv::Location location) const
{
    struct stat st;

    for (size_t i = 0; i < location.index.size(); i++)
    {
        if (stat(location.index[i].c_str(), &st) != -1 && access(location.index[i].c_str(), R_OK) == 0)
            return location.index[i];
    }
    return "";
}

bool Server::isMethodAllowed(std::string method, Webserv::Location location) const
{
    for (size_t i = 0; i < location.methods.size(); i++)
    {
        if (location.methods[i] == method)
            return true;
    }
    return false;
}

std::string dirlisntening_gen(std::string request_uri, std::string path)
{
    DIR *directory = opendir(path.c_str());
    if (!directory)
        return "<html><body><h1>Forbidden</h1></body></html>";
    std::string html_text;
    html_text += "<html>\n<head><title>Index of " + request_uri + "</title></head>\n<body>\n<h1>Index of " + request_uri + "</h1>\n<ul>\n";
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL)
    {
        std::string name = entry->d_name;
        std::string d_name = path + "/" + name;
        if (d_name == "." || d_name == "..")
            continue;
        struct stat st;
        stat(d_name.c_str(), &st);
        if (S_ISDIR(st.st_mode))
        {
            name += "/";
        }
        html_text += " <li><a href=" + request_uri + name + ">" + name + "</a></li>\n";
    }
    html_text += "</ul>\n</body>\n</html>\n";
    closedir(directory);
    return html_text;
}

std::string Server::buildErrorPage(int code)
{
    std::string html = 
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "    <title>" + tostring(code) + std::string(" ") + status_codes[code] + "</title>\n"
        "    <style>\n"
        "        body {\n"
        "            display: flex;\n"
        "            flex-direction: column;\n"
        "            justify-content: center;\n"
        "            align-items: center;\n"
        "            height: 100vh;\n"
        "            margin: 0;\n"
        "            font-family: Arial, sans-serif;\n"
        "            background-color: #f8f8f8;\n"
        "            color: #333;\n"
        "        }\n"
        "        h1 {\n"
        "            font-size: 6em;\n"
        "            margin: 0;\n"
        "        }\n"
        "        p {\n"
        "            font-size: 1.5em;\n"
        "        }\n"
        "        a {\n"
        "            margin-top: 20px;\n"
        "            text-decoration: none;\n"
        "            color: #007BFF;\n"
        "        }\n"
        "        a:hover {\n"
        "            text-decoration: underline;\n"
        "        }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <h1>" + tostring(code) + "</h1>\n"
        "    <p>" + status_codes[code] + "</p>\n"
        "</body>\n"
        "</html>\n";
    return html;
}

bool Server::serveClient(int client_fd, Server::Request request)
{
    Webserv::Server server;
    if (request.method == "GET")
    {
        server = *clientfd_to_server[client_fd];
        struct stat st;

        if (!isUriExists(request.uri, server, false))
        {
            std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
            return sendResponse(client_fd, response, request.keep_alive);
        }
        else
        {
            Webserv::Location location = getLocation(request.uri, server);
            if (!isMethodAllowed("GET", location))
            {
                std::string response = buildResponse(buildErrorPage(405), ".html", 405, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
            if (location.isRedirection) 
            {
                if (!location.redirectionIsText) 
                {
                    if (location.redirect_relative) 
                    {
                        if (!isUriExists(location.redirection.second, server, true)) // hadi drnaha 3la hsab return location wach exists
                        {
                            std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
                            return sendResponse(client_fd, response, request.keep_alive);
                        }
                        std::string response = buildResponse("", "", location.redirection.first, true, location.redirection.second, request.keep_alive);
                        return sendResponse(client_fd, response, request.keep_alive);
                    }
                    else if (location.redirect_absolute) 
                    {
                        std::string response = buildResponse("", "", location.redirection.first, true, location.redirection.second, request.keep_alive);
                        return sendResponse(client_fd, response, request.keep_alive);
                    }
                }
                else
                {
                    std::string response = buildResponse(location.redirection.second, "", location.redirection.first, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
            }
            std::string toSearch = location.root + request.uri;
            if (stat(toSearch.c_str(), &st) == -1)
            {
                std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
            else
            {
                if (S_ISDIR(st.st_mode) && toSearch[toSearch.size() - 1] == '/')
                {
                    if (!atleastOneFileExists(location))
                    {
                        if (!location.autoindex)
                        {
                            std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                            return sendResponse(client_fd, response, request.keep_alive);
                        }
                        else
                        {
                            std::string body = dirlisntening_gen(request.uri, location.root + request.uri);
                            std::string response = buildResponse(body, ".html", 200, false, "", request.keep_alive);
                            return sendResponse(client_fd, response, request.keep_alive);
                        }
                    }
                    else
                    {
                        std::string index_file = getFilethatExists(location);
                        if (!index_file.empty())
                        {
                            std::string response = buildResponse(readFile(index_file), getExtension(index_file), 200, false, "", request.keep_alive);
                            return sendResponse(client_fd, response, request.keep_alive);
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                            return sendResponse(client_fd, response, request.keep_alive);
                        }
                    }
                }
                else if (S_ISREG(st.st_mode))
                {
                    if (access(toSearch.c_str(), R_OK) == 0)
                    {
                        std::string ext = getExtension(toSearch);
                        std::string response;
                        if (getLocation(request.uri, server).hasCgi && (ext == ".py" || ext == ".php")) // cgi block
                        {
                            std::cout << "cgi block must be here\n";
                            CGI cgi(this, request, toSearch, ext);
                            response = cgi.execute(request, toSearch, ext);
                        } else //regular files
                            response = buildResponse(readFile(toSearch), getExtension(toSearch), 200, false, "", request.keep_alive);
                        return sendResponse(client_fd, response, request.keep_alive);
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                        return sendResponse(client_fd, response, request.keep_alive);
                    }
                }
                else
                {
                    std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
            }
        }
    }
    else if (request.method == "DELETE")
    {
        server = *clientfd_to_server[client_fd];
        struct stat st;
        if (!isUriExists(request.uri, server, false))
        {
            std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
            return sendResponse(client_fd, response, request.keep_alive);
        }
        else
        {
            Webserv::Location location = getLocation(request.uri, server);
            if (!isMethodAllowed("DELETE", location))
            {
                std::string response = buildResponse(buildErrorPage(405), ".html", 405, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
            std::string filepath = location.root + request.uri;
            if (stat(filepath.c_str(), &st) == -1)
            {
                std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
            else
            {
                if (S_ISDIR(st.st_mode))
                {
                    std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
                std::string file_dire = filepath.substr(0, filepath.find_last_of('/'));
                if (access(file_dire.c_str(), W_OK | X_OK) != 0)
                {
                    std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
                if (unlink(filepath.c_str()) != 0)
                {
                    std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
                std::string response = buildResponse("", "", 204, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
        }
    }
    else if (request.method == "POST")
    {
        server = *clientfd_to_server[client_fd];
        struct stat st;
        (void)st;

        if (!isUriExists(request.uri, server, false))
        {
            std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
            return sendResponse(client_fd, response, request.keep_alive);
        }
        else
        {
            Webserv::Location location = getLocation(request.uri, server);
            if (!isMethodAllowed("POST", location))
            {
                std::string response = buildResponse(buildErrorPage(405), ".html", 405, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
            
            // Check client_max_body_size for POST requests
            std::map<std::string, std::string>::iterator cl_it = request.headers.find("content-length");
            if (cl_it != request.headers.end())
            {
                size_t content_length = atol(cl_it->second.c_str());
                if (content_length > client_max_body_size)
                {
                    std::string response = buildResponse(buildErrorPage(413), ".html", 413, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
            }

            std::string toSearch = location.root + request.uri;
            
            // Handle CGI scripts for POST
            if (stat(toSearch.c_str(), &st) != -1 && S_ISREG(st.st_mode))
            {
                std::string ext = getExtension(toSearch);
                if (location.hasCgi && (ext == ".py" || ext == ".php"))
                {
                    CGI cgi(this, request, toSearch, ext);
                    std::string response = cgi.execute(request, toSearch, ext);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
                else
                {
                    std::string response = buildResponse(buildErrorPage(501), ".html", 501, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
            }
            else
            {
                std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
        }
    }
    return request.keep_alive;
}

void Server::closeClient(int epoll_fd, int client_fd, bool inside_loop)
{
    if (inside_loop)
        this->client_fds.erase(std::remove(client_fds.begin(), client_fds.end(), client_fd), client_fds.end());
    read_states.erase(client_fd); 
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
}

void Server::modifyEpollEvents(int epoll_fd, int client_fd, unsigned int events)
{
    epoll_event ev;
    ev.events = events;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
}

bool Server::sendResponse(int client_fd, const std::string response, bool keep_alive)
{
    ClientState &state = client_states[client_fd];
    state.pending_response = response;
    state.bytes_sent = 0;
    state.keep_alive = keep_alive;
    
    return continueSending(client_fd);
}

bool Server::continueSending(int client_fd)
{
    ClientState &state = client_states[client_fd];
    
    while (state.bytes_sent < state.pending_response.length())
    {
        ssize_t sent = send(client_fd, state.pending_response.c_str() + state.bytes_sent, state.pending_response.length() - state.bytes_sent, 0);
        if (sent == -1) // cant send now (buffer full), need to wait for EPOLLOUT
            return false; 
        if (sent == 0)
            return false;   
        state.bytes_sent += sent;
    }
    bool keep_alive = state.keep_alive;
    client_states.erase(client_fd);
    return keep_alive;
}

void Server::initialize(void)
{
    size_t countingFailedSockets = 0;
    int epoll_fd = epoll_create(MAX_EVENTS);
    if (epoll_fd == -1)
    {
        std::cerr << "Error: epoll_create failed" << std::endl;
        return;
    }
    for (size_t i = 0; i < servers.size(); i++)
    {
        int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd == -1)
        {
            std::cerr << "Error: Socket " << i + 1 << " failed." << std::endl;
            countingFailedSockets++;
            continue;
        }
        if (!setNonBlockingFD(sock_fd))
        {
            countingFailedSockets++;
            close(sock_fd);
            continue;
        }
        int option = 1;
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
        sockaddr_in address = infos(servers[i]);
        if (bind(sock_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1)
        {
            std::cerr << "Error: Bind failed in socket " << i + 1 << "." << std::endl;
            countingFailedSockets++;
            close(sock_fd);
            continue;
        }
        if (listen(sock_fd, SOMAXCONN) == -1)
        {
            std::cerr << "Error: Listen failed in socket " << i + 1 << "." << std::endl;
            countingFailedSockets++;
            close(sock_fd);
            continue;
        }
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sock_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &ev) == -1)
        {
            std::cerr << "Error: ctl failed in socket " << i + 1 << "." << std::endl;
            countingFailedSockets++;
            close(sock_fd);
            continue;
        }
        this->socket_fds.push_back(sock_fd);
        this->sockfd_to_server[sock_fd] = &servers[i];
        std::cout << "Server " << i + 1 << " listening on: " << servers[i].ip_address << ":" << servers[i].port << std::endl;
    }
    epoll_event events[MAX_EVENTS];
    while (!shutdownFlag && countingFailedSockets != servers.size())
    {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++)
        {
            int fd = events[i].data.fd;
            bool is_listening = false;
            for (size_t j = 0; j < socket_fds.size(); j++)
            {
                if (fd == socket_fds[j])
                {
                    is_listening = true;
                    break;
                }
            }
            if (is_listening)
            {
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
                if (client_fd != -1)
                {
                    // std::cout << "New client connected ..." << std::endl;
                    setNonBlockingFD(client_fd);
                    this->clientfd_to_server[client_fd] = this->sockfd_to_server[fd];
                    this->client_fds.push_back(client_fd);
                    epoll_event ev;
                    ev.events = EPOLLIN;
                    ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                bool keep_alive = continueSending(fd); // socket is ready to send more data
                if (client_states.find(fd) == client_states.end())
                {
                    modifyEpollEvents(epoll_fd, fd, EPOLLIN); // finished sending, switch back to reading only
                    if (!keep_alive)
                    {
                        closeClient(epoll_fd, fd, true);
                        continue;
                    }
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                std::string request_string = readRequest(fd);
                if (read_states.find(fd) != read_states.end())
                {
                    client_read &client_read_state = read_states[fd];
                    if (!client_read_state.is_request_full)
                        continue;
                    request_string = client_read_state.request;
                    read_states.erase(fd);
                }
                if (request_string.empty()) // client disconnected
                { 
                    closeClient(epoll_fd, fd, true);
                    continue;
                }
                std::cout << request_string << std::endl;
                Server::Request request;
                if (!parseRequest(fd, request_string, request))
                {
                    closeClient(epoll_fd, fd, true);
                    continue;
                }
                bool keep_alive = serveClient(fd, request);
                if (client_states.find(fd) != client_states.end()) // check if there's pending data to send
                {
                    modifyEpollEvents(epoll_fd, fd, EPOLLIN | EPOLLOUT); // couldn't send everything, register for EPOLLOUT
                }
                else if (!keep_alive)
                    closeClient(epoll_fd, fd, true);
            }
        }
    }
    this->closeSockets();
    for (size_t i = 0; i < this->client_fds.size(); i++)
        closeClient(epoll_fd, client_fds[i], false);
    close(epoll_fd);
}