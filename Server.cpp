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
    int current_fd_flags = fcntl(fd, F_GETFL, 0);
    if (current_fd_flags == -1)
    {
        std::cerr << "Error: getting current fd flags." << std::endl;
        return false;
    }
    if (fcntl(fd, F_SETFL, current_fd_flags | O_NONBLOCK) == -1)
    {
        std::cerr << "Error: setting flags non-blocking mode." << std::endl;
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

std::string Server::readFile(const std::string file_path, int &status) const
{
    std::ifstream file(file_path.c_str());
    std::stringstream content;
    if (!file.is_open())
    {
        status = 404;
        return "";
    }

    content << file.rdbuf();
    status = 200;
    file.close();
    return content.str();
}

std::string Server::getExtension(std::string file_path)
{
    size_t dot = file_path.rfind('.');
    return file_path.substr(dot);
}

std::string Server::tostring(size_t num) const
{
    std::stringstream strnum;

    strnum << num;
    return strnum.str();
}

std::string Server::buildResponse(std::string file_content, std::string extension, int status)
{
    std::string CRLF = "\r\n";
    std::string start_line = "HTTP/1.1 " + tostring(status) + " " + status_codes[status] + CRLF;
    std::string type;
    if (this->content_type.find(extension) == this->content_type.end())
        type = "text/plain";
    else
        type = this->content_type[extension];

    std::string content_type = "Content-Type: " + type + CRLF;
    std::string content_length = "Content-Length: " + tostring(file_content.length()) + CRLF;
    std::string connection = "Connection: close" + CRLF + CRLF; // two CRLF mean ending the headers , still need to check what does this mean and when to make it keep-alive vs close
    std::string response = start_line + content_type + content_length + connection + file_content;
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
            break;
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

std::vector<std::string> Server::splitRequest(std::string request_string)
{
    std::vector<std::string> lines;
    size_t start = 0;
    size_t pos;

    while ((pos = request_string.find("\r\n", start)) != std::string::npos) 
    {
        lines.push_back(request_string.substr(start, pos - start));
        start = pos + 2;
    }
    if (start < request_string.size())
        lines.push_back(request_string.substr(start));
    return lines;
}

bool Server::parseRequest(int client_fd, std::string request_string, Server::Request request) // this is the core function that processes the http requests our webserver receives
{
    (void)request;
    std::vector<std::string> lines = splitRequest(request_string); // this function needs a solid logic to not pass when \r\n are not in appropriate places
    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string line = lines[i];
        std::string *parts = split(line);
        if (line.empty()) // process request body here if present ..
        {
            // std::cout << "ending of headers: " << std::endl;

        }
        if (i == 0) // process starting line : METHOD URI HTTP-VERSION
        {
            size_t count = countParts(line);
            if (count != 3)
            {
                std::string response = buildResponse("", "", 400);
                send(client_fd, response.c_str(), response.length(), 0);
                delete[] parts;
                return false;
            }
            for (size_t j = 0; j < count; j++)
            {
                if (j == 0)
                {
                    if (parts[j] != "GET" && parts[j] != "POST" && parts[j] != "DELETE" && parts[j] != "PUT" 
                        && parts[j] != "PATCH" && parts[j] != "HEAD" && parts[j] != "OPTIONS")
                    {
                        std::string response = buildResponse("", "", 400);
                        send(client_fd, response.c_str(), response.length(), 0);
                        delete[] parts;
                        return false;
                    }
                    request.method = parts[j]; // still need to check if this method is allowed in config file
                } 
                if (j == 1)
                {
                    if (!checkPath(parts[j]))
                    {
                        std::string response = buildResponse("", "", 400);
                        send(client_fd, response.c_str(), response.length(), 0);
                        delete[] parts;
                        return false;
                    }
                    request.uri = parts[j];
                }
                if (j == 2)
                {
                    if (parts[j] != "HTTP/1.0" && parts[j] != "HTTP/1.1")
                    {
                        std::string response = buildResponse("", "", 505);
                        send(client_fd, response.c_str(), response.length(), 0);
                        delete[] parts;
                        return false;
                    }
                    request.http_version = parts[j];
                }
            }
        }
        else // process headers: KEY: VALUE
        {
            // std::cout << "headers: " << line << std::endl;

        }
        delete[] parts;
    }
    return true;
}

void Server::initialize(void)
{
    size_t countingFailedSockets = 0;
    Server::Request request;
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
                    epoll_event ev;
                    ev.events = EPOLLIN;
                    ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                }
            }
            else
            {
                std::string request_string = readRequest(fd);
                std::cout << request_string << std::endl;
                if (!parseRequest(fd, request_string, request))
                    continue;
                
                // testing dynamic responses
                int status = 0;
                std::string path = "errors/error.html";
                std::string extension = getExtension(path);
                std::string file_content = readFile(path, status); // check for error to return http error code
                std::string response = buildResponse(file_content, extension, status);
                send(fd, response.c_str(), response.length(), 0);
                close(fd);
            }
        }
    }
    this->closeSockets();
    close(epoll_fd);
}