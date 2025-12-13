#include "Server.hpp"

Server::Server(Webserv webserv) : Webserv(webserv)
{
    instance = this;
    shutdownFlag = false;
    signal(SIGINT, Server::handlingSigint);
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

std::string Server::buildResponse(std::string file_content, std::string extension, int status, bool inRedirection, std::string newPath)
{
    std::string CRLF = "\r\n";
    std::string start_line = "HTTP/1.1 " + tostring(status) + " " + status_codes[status] + CRLF;
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
    std::string connection = "Connection: close" + CRLF + CRLF; // two CRLF mean ending the headers , still need to check what does this mean and when to make it keep-alive vs close
    std::string response = start_line + content_type + location + content_length + connection + file_content;
    return response;
}

std::string Server::readRequest(int client_fd)
{
    std::string request;
    char temp_buffer[1024];
    ssize_t bytes;
    size_t content_length = 0;
    bool isParsed = false;

    while (true)
    {
        bytes = read(client_fd, temp_buffer, sizeof(temp_buffer));
        if (bytes <= 0) // connection closed or error
        {
            if (bytes == 0)
                // std::cerr << "User Disconnected" << std::endl;
            break;
        }
        request.append(temp_buffer, bytes);
        if (!isParsed)
        {
            size_t pos = request.find("\r\n\r\n");
            if (pos != std::string::npos)
            {
                isParsed = true;
                size_t idx = request.find("Content-Length:");
                if (idx != std::string::npos && idx < pos) // ensure it's in headers
                {
                    size_t line_end = request.find("\r\n", idx);
                    if (line_end != std::string::npos)
                    {
                        std::string len_str = request.substr(idx + 15, line_end - (idx + 15));
                        size_t first = len_str.find_first_not_of(" \t");
                        if (first != std::string::npos)
                            len_str = len_str.substr(first);
                        content_length = std::atoll(len_str.c_str());
                    }
                }
                else // no content-length header or no body
                    break;
            }
        }
        // check if full body received
        if (isParsed)
        {
            size_t header_end = request.find("\r\n\r\n");
            size_t total_expected = header_end + 4 + content_length;
            if (request.size() >= total_expected)
                break;
        }
    }
    return request;
}

std::vector<std::string> Server::getheadersLines(const std::string req, bool &flag, int &error_status)
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
	if (lines.size() == 1)
    {
		flag = false;
        error_status = 400;
		return lines;
    }
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
		error_status = 505;
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
    if (request.http_version == "HTTP/1.1")
    {
            std::map<std::string, std::string>::iterator found = request.headers.find(first);
            if (found != request.headers.end() && (first == "host" || first == "content-length" || first == "transfer-encoding"))
            {
                    error_status = 400;
                    return false;
            }
    }
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
    if (request.http_version == "HTTP/1.1")
    {
        std::map<std::string, std::string>::iterator found = request.headers.find("host");
        if (found == request.headers.end())
        {
            error_status = 400;
            return false;
        }
        if (request.method == "POST")
        {
            // here we need to check for content-length and transfer-encoding
        }
    }
	return true;
}

bool Server::parseRequest(int client_fd, std::string request_string, Server::Request &request) // this is the core function that processes the http requests our webserver receives
{
    bool flag = true;
    int error_status = 0;
    if (request_string.empty())
    {
        error_status = 400;
        return false;
    }
    std::vector<std::string> lines = getheadersLines(request_string, flag, error_status);
    if (!flag)
    {
        std::string response = buildResponse("", "", error_status, false, "");
        send(client_fd, response.c_str(), response.length(), 0);
        return false;
    }
    if (!parse_lines(lines, request, error_status))
    {
        std::string response = buildResponse("", "", error_status, false, "");
        send(client_fd, response.c_str(), response.length(), 0);
        return false;
    }
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
            counter++;
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

bool Server::serveClient(int client_fd, Server::Request request)
{
    Webserv::Server server;
    if (request.method == "GET")
    {
        server = *clientfd_to_server[client_fd];
        struct stat st;
        std::cout << request.uri << std::endl;

        if (!isUriExists(request.uri, server, false))
        {
            std::string response = buildResponse("Error: " + status_codes[404] + "\n", "", 404, false, "");
            send(client_fd, response.c_str(), response.length(), 0);
            return false;            
        }
        else
        {
            Webserv::Location location = getLocation(request.uri, server);
            if (!isMethodAllowed("GET", location))
            {
                std::string response = buildResponse("Error: " + status_codes[405] + "\n", "", 405, false, "");
                send(client_fd, response.c_str(), response.length(), 0);
                return false;
            }
            if (location.isRedirection) {
                if (!location.redirectionIsText) {
                    if (location.redirect_relative) {
                        if (!isUriExists(location.redirection.second, server, true)) // hadi drnaha 3la hsab return location wach exists
                        {
                            std::string response = buildResponse("Error: " + status_codes[404] + "\n", "", 404, false, "");
                            send(client_fd, response.c_str(), response.length(), 0);
                            return false;            
                        }
                        std::string response = buildResponse("", "", location.redirection.first, true, location.redirection.second);
                        send(client_fd, response.c_str(), response.length(), 0);
                        return true;
                    }
                    else if (location.redirect_absolute) {
                        std::string response = buildResponse("", "", location.redirection.first, true, location.redirection.second);
                        send(client_fd, response.c_str(), response.length(), 0);
                        return true;
                    }
                }
                else
                {
                    std::string response = buildResponse(location.redirection.second, "", location.redirection.first, false, "");
                    send(client_fd, response.c_str(), response.length(), 0);
                    return true;
                }
            }
            std::string toSearch = location.root + request.uri;
            if (stat(toSearch.c_str(), &st) == -1)
            {
                std::string response = buildResponse("Error: " + status_codes[404] + "\n", "", 404, false, "");
                send(client_fd, response.c_str(), response.length(), 0);
                return false;
            }
            else
            {
                if (S_ISDIR(st.st_mode) && toSearch[toSearch.size() - 1] == '/')
                {
                    if (!atleastOneFileExists(location))
                    {
                        if (!location.autoindex){
                            std::string response = buildResponse("Error: " + status_codes[403] + "\n", "", 403, false, "");
                            send(client_fd, response.c_str(), response.length(), 0);
                            return false;
                        }
                        else{
                            std::string body = dirlisntening_gen(request.uri, location.root + request.uri);
                            std::string response = buildResponse(body, ".html", 200, false, "");
                            send(client_fd, response.c_str(), response.length(), 0);
                            return true;
                        }
                    }
                    else
                    {
                        std::string index_file = getFilethatExists(location);
                        if (!index_file.empty())
                        {
                            std::string response = buildResponse(readFile(index_file), getExtension(index_file), 200, false, "");
                            send(client_fd, response.c_str(), response.length(), 0);
                            return true;
                        }
                        else
                        {
                            std::string response = buildResponse("Error: " + status_codes[403] + "\n", "", 403, false, "");
                            send(client_fd, response.c_str(), response.length(), 0);
                            return false;
                        }
                    }
                }
                else if (S_ISREG(st.st_mode))
                {
                    if (access(toSearch.c_str(), R_OK) == 0)
                    {
                        std::string response = buildResponse(readFile(toSearch), getExtension(toSearch), 200, false, "");
                        send(client_fd, response.c_str(), response.length(), 0);
                        return true;
                    }
                    else
                    {
                        std::string response = buildResponse("Error: " + status_codes[403] + "\n", "", 403, false, "");
                        send(client_fd, response.c_str(), response.length(), 0);
                        return false;
                    }
                }
                else
                {
                    std::string response = buildResponse("Error: " + status_codes[404] + "\n", "", 404, false, "");
                    send(client_fd, response.c_str(), response.length(), 0);
                    return false;
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
            std::string response = buildResponse("Error: " + status_codes[404] + "\n", "", 404, false, "");
            send(client_fd, response.c_str(), response.length(), 0);
            return false;            
        }
        else
        {
            Webserv::Location location = getLocation(request.uri, server);
            if (!isMethodAllowed("DELETE", location))
            {
                std::string response = buildResponse("Error: " + status_codes[405] + "\n", "", 405, false, "");
                send(client_fd, response.c_str(), response.length(), 0);
                return false;
            }
            std::string filepath = location.root + request.uri;
            if (stat(filepath.c_str(), &st) == -1)
            {
                std::string response = buildResponse("Error: " + status_codes[404] + "\n", "", 404, false, "");
                send(client_fd, response.c_str(), response.length(), 0);
                return false;
            }
            else
            {
                if (S_ISDIR(st.st_mode))
                {
                    std::string response = buildResponse("Error: " + status_codes[403] + "\n", "", 403, false, "");
                    send(client_fd, response.c_str(), response.length(), 0);
                    return false;
                }
                std::string file_dire = filepath.substr(0, filepath.find_last_of('/'));
                if (access(file_dire.c_str(), W_OK | X_OK) != 0)
                {
                    std::string response = buildResponse("Error: " + status_codes[403] + "\n", "", 403, false, "");
                    send(client_fd, response.c_str(), response.length(), 0);
                    return false;
                }
                if (unlink(filepath.c_str()) != 0){
                    std::string response = buildResponse("Error: " + status_codes[403] + "\n", "", 403, false, "");
                    send(client_fd, response.c_str(), response.length(), 0);
                    return false;
                }
                std::string response = buildResponse("", "", 204, false, "");
                send(client_fd, response.c_str(), response.length(), 0);
                return false;
            }
        }
    }
    else if (request.method == "POST")
    {

    }
    return true;
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
                    epoll_event ev;
                    ev.events = EPOLLIN;
                    ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                }
            }
            else
            {
                std::string request_string = readRequest(fd);
                if (request_string.empty()) // keeping connection alive until user disconnects
                { 
                    close(fd);
                    continue;
                }
                // std::cout << request_string << std::endl;
                Server::Request request;
                if (!parseRequest(fd, request_string, request))
                {
                    close(fd);
                    continue;
                }
                if (!serveClient(fd, request))
                {
                    close(fd);
                    continue;
                }
            }
        }
    }
    this->closeSockets();
    close(epoll_fd);
}