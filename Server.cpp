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

Server &Server::operator=(const Server &theOtherObject)
{
    if (this != &theOtherObject)
        static_cast<void>(theOtherObject);
    return *this;
}

Server::CgiState::CgiState() : start_time(0), added_to_epoll(false)
{
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

unsigned int Server::getBinaryAddress(std::string address)
{
    size_t pos = address.find('.');
    int shift = 24;
    unsigned int result = 0;
    while (pos != std::string::npos || shift >= 0)
    {
        int part = std::atoll(address.substr(0, pos).c_str());
        result |= (part << shift);
        address = address.substr(pos + 1);
        pos = address.find('.');
        shift -= 8;
    }
    return htonl(result);
}

sockaddr_in Server::infos(const Webserv::Server server)
{
    sockaddr_in address;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = getBinaryAddress(server.ip_address);
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

std::string Server::currentDate(void) const
{
    char buffer[64];

    std::time_t now = std::time(NULL);
    std::tm *gmt = std::gmtime(&now);
    std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return std::string(buffer);
}

std::string Server::buildResponse(std::string body, std::string extension, int status, bool inRedirection, std::string newPath, bool keep_alive, const std::vector<std::pair<std::string, std::string> > &extra_headers)
{
    std::string CRLF = "\r\n";
    std::string response;
    response += "HTTP/1.0 " + tostring(status) + " " + status_codes[status] + CRLF;
    std::string type = "text/plain";
    if (content_type.count(extension))
        type = content_type[extension];
    response += "Date: " + currentDate() + CRLF;
    if (status != 204)
        response += "Content-Type: " + type + CRLF;
    if (inRedirection)
        response += "Location: " + newPath + CRLF;
    for (size_t i = 0; i < extra_headers.size(); i++)
    {
        const std::string &key = extra_headers[i].first;
        if (key == "content-type" || key == "content-length" || key == "location")
            continue;
        response += key + ": " + extra_headers[i].second + CRLF;
    }
    response += "Content-Length: " + tostring(body.size()) + CRLF;
    response += "Connection: " + std::string(keep_alive ? "keep-alive" : "close") + CRLF;
    response += CRLF;
    response += body;

    return response;
}

std::string Server::_trim(std::string str) const
{
    size_t start = 0;
    size_t end = str.length();

    while (start < str.length())
    {
        if (!std::isspace(str[start]))
            break;
        start++;
    }
    while (end > start)
    {
        if (!std::isspace(str[end - 1]))
            break;
        end--;
    }
    return str.substr(start, end - start);
}

bool Server::isContentLengthValid(std::string value)
{
    value = _trim(value);
    for (size_t i = 0; i < value.length(); i++)
    {
        if (!isdigit(value[i]))
            return false;
    }
    return true;
}

std::string Server::readRequest(int epoll_fd, int client_fd, bool &been_here)
{
    client_read &client = read_states[client_fd];
    Webserv::Server server = *clientfd_to_server[client_fd];
    std::string CRLF = "\r\n\r\n";

    if (!client.first_call)
    {
        client.file_fd = -1;
        client.total_bytes_written = 0;
        client.content_len = 0;
        client.is_request_full = false;
        client.headers = "";
        client.temporary_body = "";
        client.isParsed = false;
        client.content_lenght_present = false;
        client.loop_counter = 0;
        client.is_post = false;
        client.request.is_uri_dir = false;
        client.request.is_uri_reg = false;
        client.request.body_headers_done = false;
        client.temporary_body = "";
        client.boundary_found = false;
        client.keep_alive = false;
        client.packet_ended = true;
        client.end_boundary_found = false;
        client.first_call = true;
        client.client_disconnected = false;
    }
    while (1)
    {
        client.bytes = read(client_fd, client.buffer, sizeof(client.buffer));

        if (client.bytes == -1 && client.packet_ended)
        {
            if (client.is_post)
            {
                client.start_time = std::time(NULL);
                client.has_start_time = true;
                client.is_request_full = false;
            }
            else
                client.is_request_full = true;
            return client.headers;
        }
        if (client.bytes == 0)
        {
            client.is_request_full = true;
            std::cout << "Client " << client_fd << " disconnected" << std::endl;
            client.client_disconnected = true;
            return client.headers;
        }
        client.has_start_time = false;
        client.packet_ended = true;
        if (!client.isParsed)
        {
            client.headers = std::string(client.buffer, client.bytes);
            size_t CRLF_pos = client.headers.find(CRLF);
            if (CRLF_pos == std::string::npos)
            {
                if (error_pages.count("400") && fileValid(error_pages["400"]))
                {
                    sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, false);
                    client.is_request_full = true;
                    return "";
                }
                else
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
                    sendResponse(client_fd, response, false);
                    client.is_request_full = true;
                    closeClient(epoll_fd, client_fd, true);
                    return "";
                }
            }
            client.isParsed = true;
            client.temporary_body = client.headers.substr(CRLF_pos + 4);
            if (!client.temporary_body.empty())
                client.packet_ended = false;
            client.headers = client.headers.substr(0, CRLF_pos + 4);
            std::string lowercase_headers = str_tolower(client.headers);
            size_t content_length_position = lowercase_headers.find("content-length:");
            if (content_length_position != std::string::npos)
            {
                size_t ending_CRLF = lowercase_headers.find("\r\n", content_length_position);
                if (ending_CRLF != std::string::npos)
                {
                    std::string content_length_value = lowercase_headers.substr(content_length_position + 15, ending_CRLF - (content_length_position + 15));
                    if (!isContentLengthValid(content_length_value))
                    {
                        if (error_pages.count("400") && fileValid(error_pages["400"]))
                        {
                            sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, false);
                            client.is_request_full = true;
                            return "";
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
                            sendResponse(client_fd, response, false);
                            client.is_request_full = true;
                            closeClient(epoll_fd, client_fd, true);
                            return "";
                        }
                    }
                    client.content_lenght_present = true;
                    client.content_len = std::atoll(content_length_value.c_str());
                }
            }
            if (!parseRequest(client_fd, client.headers, client.request, true, been_here))
            {
                client.is_request_full = true;
                return "";
            }
            client.is_post = client.request.method == "POST";
            clientfd_to_request[client_fd] = client.request;
            if (client.is_post)
            {
                struct stat st;
                if (!isUriExists(client.request.uri, server, false))
                {
                    if (error_pages.count("404") && fileValid(error_pages["404"]))
                    {
                        sendFileResponse(client_fd, error_pages["404"], getExtension(error_pages["404"]), 404, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                client.request.location = getLocation(client.request.uri, server);
                if (!isMethodAllowed("POST", client.request.location))
                {
                    if (error_pages.count("400") && fileValid(error_pages["400"]))
                    {
                        sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                if (client.request.location.isRedirection)
                {
                    if (!client.request.location.redirectionIsText)
                    {
                        if (client.request.location.redirect_relative)
                        {
                            if (!isUriExists(client.request.location.redirection.second, server, true)) // hadi drnaha 3la hsab return location wach exists
                            {
                                if (error_pages.count("404") && fileValid(error_pages["404"]))
                                {
                                    sendFileResponse(client_fd, error_pages["404"], getExtension(error_pages["404"]), 404, client.request.keep_alive);
                                    client.is_request_full = true;
                                    client.keep_alive = client.request.keep_alive;
                                    return "";
                                }
                                else
                                {
                                    std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", client.request.keep_alive);
                                    sendResponse(client_fd, response, client.request.keep_alive);
                                    client.is_request_full = true;
                                    client.keep_alive = client.request.keep_alive;
                                    return "";
                                }
                            }
                            std::string response = buildResponse("", "", client.request.location.redirection.first, true, client.request.location.redirection.second, client.request.keep_alive);
                            sendResponse(client_fd, response, client.request.keep_alive);
                            client.is_request_full = true;
                            client.keep_alive = client.request.keep_alive;
                            return "";
                        }
                        else if (client.request.location.redirect_absolute)
                        {
                            std::string response = buildResponse("", "", client.request.location.redirection.first, true, client.request.location.redirection.second, client.request.keep_alive);
                            sendResponse(client_fd, response, client.request.keep_alive);
                            client.is_request_full = true;
                            client.keep_alive = client.request.keep_alive;
                            return "";
                        }
                    }
                    else
                    {
                        std::string response = buildResponse(client.request.location.redirection.second, "", client.request.location.redirection.first, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                if (client.content_len > client_max_body_size)
                {
                    if (error_pages.count("400") && fileValid(error_pages["400"]))
                    {
                        sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                std::string toSearch = buildSearchingFile(client.request.location.root, client.request.uri, client.request.location);
                size_t pos = simplifyPath(toSearch).find(client.request.location.root);
                if (pos == std::string::npos || pos != 0)
                {
                    if (error_pages.count("403") && fileValid(error_pages["403"]))
                    {
                        sendFileResponse(client_fd, error_pages["403"], getExtension(error_pages["403"]), 403, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                if (stat(toSearch.c_str(), &st) == -1)
                {
                    if (error_pages.count("404") && fileValid(error_pages["404"]))
                    {
                        sendFileResponse(client_fd, error_pages["404"], getExtension(error_pages["404"]), 404, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                if (S_ISREG(st.st_mode))
                {
                    client.request.is_uri_reg = true;
                    continue;
                }
                else if (S_ISDIR(st.st_mode))
                    client.request.is_uri_dir = true;
                else
                {
                    if (error_pages.count("400") && fileValid(error_pages["400"]))
                    {
                        sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                if (client.request.is_uri_dir)
                {
                    client.request.location.upload_dir = client.request.location.root + client.request.location.upload_dir;
                    std::string upload_dir = client.request.location.upload_dir;
                    if (stat(upload_dir.c_str(), &st) == -1)
                    {
                        if (error_pages.count("404") && fileValid(error_pages["404"]))
                        {
                            sendFileResponse(client_fd, error_pages["404"], getExtension(error_pages["404"]), 404, client.request.keep_alive);
                            client.is_request_full = true;
                            return "";
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", client.request.keep_alive);
                            sendResponse(client_fd, response, client.request.keep_alive);
                            client.is_request_full = true;
                            client.keep_alive = client.request.keep_alive;
                            return "";
                        }
                    }
                    if (!S_ISDIR(st.st_mode))
                    {
                        if (error_pages.count("403") && fileValid(error_pages["403"]))
                        {
                            sendFileResponse(client_fd, error_pages["403"], getExtension(error_pages["403"]), 403, client.request.keep_alive);
                            client.is_request_full = true;
                            return "";
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", client.request.keep_alive);
                            sendResponse(client_fd, response, client.request.keep_alive);
                            client.is_request_full = true;
                            client.keep_alive = client.request.keep_alive;
                            return "";
                        }
                    }
                    if (access(upload_dir.c_str(), W_OK) == -1)
                    {
                        if (error_pages.count("403") && fileValid(error_pages["403"]))
                        {
                            sendFileResponse(client_fd, error_pages["403"], getExtension(error_pages["403"]), 403, client.request.keep_alive);
                            client.is_request_full = true;
                            return "";
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", client.request.keep_alive);
                            sendResponse(client_fd, response, client.request.keep_alive);
                            client.is_request_full = true;
                            client.keep_alive = client.request.keep_alive;
                            return "";
                        }
                    }
                    if (client.request.headers.find("content-type") == client.request.headers.end())
                    {
                        if (error_pages.count("400") && fileValid(error_pages["400"]))
                        {
                            sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                            client.is_request_full = true;
                            return "";
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                            sendResponse(client_fd, response, client.request.keep_alive);
                            client.is_request_full = true;
                            client.keep_alive = client.request.keep_alive;
                            return "";
                        }
                    }
                    size_t multipart_pos = client.request.headers["content-type"].find("multipart/form-data;");
                    if (multipart_pos == std::string::npos)
                    {
                        if (error_pages.count("400") && fileValid(error_pages["400"]))
                        {
                            sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                            client.is_request_full = true;
                            return "";
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                            sendResponse(client_fd, response, client.request.keep_alive);
                            client.is_request_full = true;
                            client.keep_alive = client.request.keep_alive;
                            return "";
                        }
                    }
                    size_t boundary_pos = client.request.headers["content-type"].find("boundary=");
                    if (boundary_pos == std::string::npos)
                    {
                        if (error_pages.count("400") && fileValid(error_pages["400"]))
                        {
                            sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                            client.is_request_full = true;
                            return "";
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                            sendResponse(client_fd, response, client.request.keep_alive);
                            client.is_request_full = true;
                            client.keep_alive = client.request.keep_alive;
                            return "";
                        }
                    }
                    client.request.body_boundary = "--" + client.request.headers["content-type"].substr(boundary_pos + 9);
                    continue;
                }
            }
        }
        if (!client.is_post)
            continue;
        if (client.is_post && !client.content_lenght_present)
        {
            if (error_pages.count("400") && fileValid(error_pages["400"]))
            {
                sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                client.is_request_full = true;
                return "";
            }
            else
            {
                std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
                sendResponse(client_fd, response, false);
                client.is_request_full = true;
                return "";
            }
        }
        if (client.content_lenght_present && client.is_post && client.request.is_uri_dir)
        {
            if (client.bytes == -1)
                client.body_buffer = client.temporary_body;
            else
                client.body_buffer = client.temporary_body + std::string(client.buffer, client.bytes);
            client.temporary_body = "";
            if (!client.request.body_headers_done)
            {
                size_t body_crlf = client.body_buffer.find("\r\n\r\n");
                if (body_crlf == std::string::npos)
                {
                    client.loop_counter++;
                    if (client.loop_counter > 1)
                    {
                        if (error_pages.count("400") && fileValid(error_pages["400"]))
                        {
                            sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                            client.is_request_full = true;
                            return "";
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                            sendResponse(client_fd, response, client.request.keep_alive);
                            client.is_request_full = true;
                            client.keep_alive = client.request.keep_alive;
                            return "";
                        }
                    }
                    client.temporary_body = client.body_buffer;
                    client.packet_ended = false;
                    continue;
                }
                client.loop_counter = 0;
                std::string body_headers_string = client.body_buffer.substr(0, body_crlf + 4);
                std::vector<std::string> body_headers_array = get_bodyheaders_Lines(body_headers_string);
                int no_use;
                for (size_t i = 0; i < body_headers_array.size(); i++)
                {
                    if (!parse_headers(body_headers_array[i], client.request.body_headers, no_use, i, client.request.body_boundary))
                    {
                        if (error_pages.count("400") && fileValid(error_pages["400"]))
                        {
                            sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                            client.is_request_full = true;
                            return "";
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                            sendResponse(client_fd, response, client.request.keep_alive);
                            client.is_request_full = true;
                            client.keep_alive = client.request.keep_alive;
                            return "";
                        }
                    }
                }
                if (client.request.body_headers.find("content-disposition") == client.request.body_headers.end())
                {
                    if (error_pages.count("400") && fileValid(error_pages["400"]))
                    {
                        sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                size_t filename_pos = client.request.body_headers["content-disposition"].find("filename=");
                if (filename_pos == std::string::npos)
                {
                    int i = 0;
                    while (client.request.body_boundary[i] == '-')
                        i++;
                    client.request.bodyfile_name = client.request.body_boundary.substr(i);
                }
                else
                {
                    client.request.bodyfile_name = client.request.body_headers["content-disposition"].substr(filename_pos + 9);
                    client.request.bodyfile_name.erase(0, 1);
                    client.request.bodyfile_name.erase(client.request.bodyfile_name.size() - 1, 1);
                }
                if (access((client.request.location.upload_dir + "/" + client.request.bodyfile_name).c_str(), F_OK) == 0)
                {
                    size_t dot_pos = client.request.bodyfile_name.find_last_of(".");
                    if (dot_pos == std::string::npos)
                        dot_pos = client.request.bodyfile_name.size();
                    int i = 1;
                    while (1)
                    {
                        std::string str_num = tostring(i);
                        client.request.bodyfile_name.insert(dot_pos, "(" + str_num + ")");
                        if (access((client.request.location.upload_dir + "/" + client.request.bodyfile_name).c_str(), F_OK) == 0)
                        {
                            int last_dot_pos = client.request.bodyfile_name.find_last_of("(" + str_num + ")");
                            client.request.bodyfile_name.erase(last_dot_pos - str_num.size() - 1, str_num.size() + 2);
                            i++;
                            continue;
                        }
                        break;
                    }
                }
                std::string upload_file = client.request.location.upload_dir + "/" + client.request.bodyfile_name;
                if (client.file_fd != -1)
                    close(client.file_fd);
                client.file_fd = open(upload_file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (client.file_fd == -1)
                {
                    if (error_pages.count("500") && fileValid(error_pages["500"]))
                    {
                        sendFileResponse(client_fd, error_pages["500"], getExtension(error_pages["500"]), 500, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                client.temporary_body = client.body_buffer.substr(body_crlf + 4);
                if (!client.temporary_body.empty())
                    client.packet_ended = false;
                client.total_bytes_written += body_headers_string.size();
                client.request.body_headers_done = true;
                continue;
            }
            size_t boundry_pos = client.body_buffer.find("\r\n" + client.request.body_boundary + "\r\n");
            size_t end_boundry_pos = client.body_buffer.find("\r\n" + client.request.body_boundary + "--\r\n");
            if (boundry_pos != std::string::npos)
            {
                client.to_write = client.body_buffer.substr(0, boundry_pos);
                client.temporary_body = client.body_buffer.substr(boundry_pos + 2);
                if (!client.temporary_body.empty())
                    client.packet_ended = false;
                client.request.body_headers_done = false;
                client.total_bytes_written += 2;
            }
            else if (end_boundry_pos != std::string::npos)
            {
                client.to_write = client.body_buffer.substr(0, end_boundry_pos);
                client.temporary_body = client.body_buffer.substr(end_boundry_pos + client.request.body_boundary.size() + 6);
                if (!client.temporary_body.empty())
                {
                    if (error_pages.count("400") && fileValid(error_pages["400"]))
                    {
                        sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                client.total_bytes_written += client.request.body_boundary.size() + 6;
                client.boundary_found = true;
                client.end_boundary_found = true;
            }
            else
            {
                size_t r_pos = client.body_buffer.find("\r");
                if (r_pos == std::string::npos)
                    client.to_write = client.body_buffer;
                else
                {
                    client.temporary_body = client.body_buffer.substr(r_pos);
                    client.packet_ended = true;
                    if (client.temporary_body.size() > client.request.body_boundary.size() + 6)
                    {
                        client.to_write = client.body_buffer;
                        client.temporary_body = "";
                    }
                    else
                        client.to_write = client.body_buffer.substr(0, r_pos);
                }
            }
            ssize_t written = write(client.file_fd, client.to_write.c_str(), client.to_write.size());
            if (written < (int)client.to_write.size())
            {
                if (written == -1)
                {
                    if (error_pages.count("500") && fileValid(error_pages["500"]))
                    {
                        sendFileResponse(client_fd, error_pages["500"], getExtension(error_pages["500"]), 500, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        close(client.file_fd);
                        client.file_fd = -1;
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                else
                {
                    if (write(client.file_fd, client.to_write.substr(written).c_str(), client.to_write.size() - written) == -1)
                    {
                        if (error_pages.count("500") && fileValid(error_pages["500"]))
                        {
                            sendFileResponse(client_fd, error_pages["500"], getExtension(error_pages["500"]), 500, client.request.keep_alive);
                            client.is_request_full = true;
                            return "";
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", client.request.keep_alive);
                            sendResponse(client_fd, response, client.request.keep_alive);
                            close(client.file_fd);
                            client.file_fd = -1;
                            client.is_request_full = true;
                            client.keep_alive = client.request.keep_alive;
                            return "";
                        }
                    }
                }
            }
            client.total_bytes_written += client.to_write.size();
            if (client.boundary_found)
            {
                if (!client.request.body_headers_done)
                {
                    close(client.file_fd);
                    client.file_fd = -1;
                    client.boundary_found = false;
                    continue;
                }
                close(client.file_fd);
                client.file_fd = -1;
            }
        }
        if (client.content_lenght_present && client.is_post && client.request.is_uri_reg)
        {
            if (client.content_len > 60000)
            {
                if (error_pages.count("400") && fileValid(error_pages["400"]))
                {
                    sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                    client.is_request_full = true;
                    return "";
                }
                else
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
            }
            if (client.bytes == -1)
                client.request.body = client.temporary_body;
            else
                client.request.body = client.temporary_body + std::string(client.buffer, client.bytes);
            client.temporary_body = "";
            client.total_bytes_written += client.request.body.size();
            if (client.total_bytes_written < client.content_len)
            {
                client.temporary_body = client.request.body;
                client.packet_ended = false;
                continue;
            }
            std::string toSearch = buildSearchingFile(client.request.location.root, client.request.uri, client.request.location);
            std::string ext = getExtension(toSearch);
            if (client.request.location.hasCgi && (ext == ".py" || ext == ".php"))
            {
                if (!setupCGI(client.request, toSearch, ext, client_fd, epoll_fd))
                {
                    if (error_pages.count("500") && fileValid(error_pages["500"]))
                    {
                        sendFileResponse(client_fd, error_pages["500"], getExtension(error_pages["500"]), 500, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                client.is_request_full = true;
                client.keep_alive = client.request.keep_alive;
                return "";
            }
            else
            {
                if (error_pages.count("501") && fileValid(error_pages["501"]))
                {
                    sendFileResponse(client_fd, error_pages["501"], getExtension(error_pages["501"]), 501, client.request.keep_alive);
                    client.is_request_full = true;
                    return "";
                }
                else
                {
                    std::string response = buildResponse(buildErrorPage(501), ".html", 501, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
            }
        }
        //===================================================POST END===============================================================
        if (client.isParsed)
        {
            if (!client.content_lenght_present)
            {
                if (client.total_bytes_written > 0)
                {
                    client.is_request_full = true;
                    return client.headers;
                }
                client.is_request_full = true;
                return client.headers;
            }
            else if (client.total_bytes_written >= client.content_len)
            {
                if (client.is_post && client.total_bytes_written > client.content_len)
                {
                    if (error_pages.count("400") && fileValid(error_pages["400"]))
                    {
                        sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                        client.is_request_full = true;
                        return "";
                    }
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
                        sendResponse(client_fd, response, false);
                        client.is_request_full = true;
                        closeClient(epoll_fd, client_fd, true);
                        return client.headers;
                    }
                }
                else if (client.is_post && client.total_bytes_written == client.content_len)
                {
                    if (client.end_boundary_found)
                    {
                        std::string response = buildResponse(buildErrorPage(201), ".html", 201, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.keep_alive = client.request.keep_alive;
                        client.temporary_body = "";
                        client.is_request_full = true;
                        break;
                    }
                    else if (!client.end_boundary_found)
                    {
                        if (error_pages.count("400") && fileValid(error_pages["400"]))
                        {
                            sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, client.request.keep_alive);
                            client.is_request_full = true;
                            return "";
                        }
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
                            sendResponse(client_fd, response, false);
                            client.is_request_full = true;
                            closeClient(epoll_fd, client_fd, true);
                            return client.headers;
                        }
                    }
                }
            }
            else if (client.total_bytes_written < client.content_len)
                continue;
        }
    }
    return client.headers;
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
    body = req.substr(pos + 4);
    return lines;
}

bool Server::parse_path(std::string &path)
{
    if (path.empty() || path[0] != '/')
        return false;
    if (path.length() >= 2 && path[0] == '/' && path[1] == '/') // net path
        return false;
    return true;
}

std::string Server::decodeURI(std::string uri)
{
    std::string result;

    for (size_t i = 0; i < uri.size(); i++)
    {
        if (uri[i] == '%' && (i + 2 < uri.size()) && std::isxdigit(uri[i + 1]) && std::isxdigit(uri[i + 2]))
        {
            std::string hex = uri.substr(i + 1, 2);
            char decoded = static_cast<char>(std::strtol(hex.c_str(), NULL, 16));
            result += decoded;
            i += 2;
        }
        else
            result += uri[i];
    }
    return result;
}

bool Server::validURI(std::string uri)
{
    for (size_t i = 0; i < uri.size(); i++)
    {
        if (uri[i] == '%' && (i + 2 < uri.size()) && std::isxdigit(uri[i + 1]) && std::isxdigit(uri[i + 2]))
        {
            std::string hex = uri.substr(i + 1, 2);
            char decoded = static_cast<char>(std::strtol(hex.c_str(), NULL, 16));
            if (decoded == '/' || decoded == '<' || decoded == '>' || decoded == '"' || decoded == '|' || decoded == '\\' || decoded == '^' || decoded == '`')
                return false;
            i += 2;
        }
    }
    return true;
}

bool Server::allUppercase(std::string method)
{
    size_t count = 0;
    for (size_t i = 0; i < method.length(); i++)
    {
        if (isupper(method[i]))
            count++;
    }
    return count == method.length();
}

bool Server::parse_methode(std::string *words, int &error_status, Request &request)
{
    std::string http_versions[] = {"HTTP/0.9", "HTTP/1.0", "HTTP/1.1", "HTTP/2.0", "HTTP/3.0"};
    size_t http_versions_length = sizeof(http_versions) / sizeof(http_versions[0]);
    if (allUppercase(words[0]) && words[0] != "GET" && words[0] != "POST" && words[0] != "DELETE")
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
    if (!validURI(words[1]))
    {
        error_status = 400;
        return false;
    }
    request.uri = decodeURI(words[1]);
    // [ path ] [ ";" params ] [ "?" query ]
    size_t fragment_pos = request.queries.find("#");
    if (fragment_pos != std::string::npos)
        request.uri = request.uri.substr(0, fragment_pos);

    size_t quest_pos = request.uri.find("?");
    if (quest_pos != std::string::npos)
    {
        request.queries = request.uri.substr(quest_pos + 1);
        request.uri = request.uri.substr(0, quest_pos);
    }
    size_t semicolon_pos = request.uri.find(";");
    if (semicolon_pos != std::string::npos)
    {
        request.params = request.uri.substr(semicolon_pos + 1);
        request.uri = request.uri.substr(0, semicolon_pos);
    }
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
    for (size_t i = 0; i < first.size(); i++)
    {
        if (isalnum(first[i]) || first[i] == '-' || first[i] == ':')
            continue;
        else
            return false;
    }
    return true;
}

bool Server::parse_headers(std::string &line, std::map<std::string, std::string> &map, int &error_status, int option, const std::string boundary)
{
    if (line.empty())
    {
        error_status = 400;
        return false;
    }
    if (!option)
    {
        if (line != boundary)
        {
            error_status = 400;
            return false;
        }
        else
            return true;
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
    map[first] = second;
    return true;
}

bool Server::isRequestLineValid(std::string request_line)
{
    size_t i = 0;
    while (i < request_line.length() && isspace(request_line[i]))
        i++;
    if (i > 0)
        return false;
    while (i < request_line.length() && !isspace(request_line[i]))
        i++;
    size_t j = i;
    while (j < request_line.length() && isspace(request_line[j]))
        j++;
    if (j - i != 1)
        return false;
    i = j;
    while (i < request_line.length() && !isspace(request_line[i]))
        i++;
    j = i;
    while (j < request_line.length() && isspace(request_line[j]))
        j++;
    if (j - i != 1)
        return false;
    i = j;
    std::string last = request_line.substr(i);
    if (isspace(last[last.length() - 1]))
        return false;
    return true;
}

bool Server::parse_lines(std::vector<std::string> lines, Request &request, int &error_status)
{
    std::string *words;
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (i == 0)
        {
            if (!isRequestLineValid(lines[i]))
            {
                error_status = 400;
                return false;
            }
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
            if (!parse_headers(lines[i], request.headers, error_status, 1, ""))
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

bool Server::parseRequest(int client_fd, std::string request_string, Request &request, bool inside_read_request, bool &been_here)
{
    bool flag = true;
    int error_status = 0;
    std::string body;
    std::vector<std::string> lines = getheadersLines(request_string, flag, error_status, body);
    if (!flag)
    {
        if (error_pages.count("400") && fileValid(error_pages["400"]))
        {
            if (inside_read_request && write_states.find(client_fd) == write_states.end())
            {
                request.has_error_page = true;
                request.sent_all_error_page = sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, false);
                return request.sent_all_error_page;
            }
            else if (write_states.find(client_fd) != write_states.end())
            {
                request.has_error_page = true;
                request.sent_all_error_page = continueSending(client_fd);
                return request.sent_all_error_page;
            }
        }
        else
        {
            if (!been_here)
            {
                std::string response = buildResponse(buildErrorPage(error_status), ".html", error_status, false, "", false);
                sendResponse(client_fd, response, false);
                return false;
            }
        }
    }
    if (!parse_lines(lines, request, error_status))
    {
        if (error_pages.count("400") && fileValid(error_pages["400"]))
        {
            if (inside_read_request && write_states.find(client_fd) == write_states.end())
            {
                request.has_error_page = true;
                request.sent_all_error_page = sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, false);
                return request.sent_all_error_page;
            }
            else if (write_states.find(client_fd) != write_states.end())
            {
                request.has_error_page = true;
                request.sent_all_error_page = continueSending(client_fd);
                return request.sent_all_error_page;
            }
        }
        else
        {
            if (!been_here)
            {
                been_here = true;
                std::string response = buildResponse(buildErrorPage(error_status), ".html", error_status, false, "", false);
                sendResponse(client_fd, response, false);
                return false;
            }
        }
    }
    if (!isstrdigit(request.headers["content-length"]))
    {
        if (error_pages.count("400") && fileValid(error_pages["400"]))
        {
            if (inside_read_request && write_states.find(client_fd) == write_states.end())
            {
                request.has_error_page = true;
                request.sent_all_error_page = sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, false);
                return request.sent_all_error_page;
            }
            else if (write_states.find(client_fd) != write_states.end())
            {
                request.has_error_page = true;
                request.sent_all_error_page = continueSending(client_fd);
                return request.sent_all_error_page;
            }
        }
        else
        {
            if (!been_here)
            {
                been_here = true;
                std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
                sendResponse(client_fd, response, false);
                return false;
            }
        }
    }
    std::map<std::string, std::string>::iterator found = request.headers.find("connection");
    request.keep_alive = false;
    if (found != request.headers.end() && found->second == "keep-alive")
        request.keep_alive = true;
    if (client_addresses.find(client_fd) != client_addresses.end())
    {
        sockaddr_in addr = client_addresses[client_fd];
        request.remote_addr = getAddress(&addr, NULL, false);
        request.remote_port = ntohs(addr.sin_port);
    }
    else
    {
        request.remote_addr = "127.0.0.1";
        request.remote_port = 0;
    }
    if (request.method == "POST")
        std::cout << YELLOW << "[ " << request.method << " ] " << RESET << request.uri << " " << request.remote_addr << ":" << request.remote_port << std::endl;
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

std::string dirlisntening_gen(std::string request_uri, std::string path, int &code)
{
    DIR *directory = opendir(path.c_str());
    if (!directory)
    {
        code = 403;
        return "<h1 style=\"font-family: sans-serif; color: darkred;\">Forbidden</h1>";
    }

    std::string html_text;

    html_text += "<div style=\"font-family: sans-serif; max-width: 800px; margin: 20px auto;\">";
    html_text += "<h1 style=\"text-align: center; color: #333;\">Index of " + request_uri + "</h1>\n";
    html_text += "<ul style=\"list-style-type: none; padding: 0;\">\n";

    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL)
    {
        std::string name = entry->d_name;
        std::string d_name = path + "/" + name;

        struct stat st;
        stat(d_name.c_str(), &st);
        if (S_ISDIR(st.st_mode))
        {
            name += "/";
            html_text += "<li style=\"margin: 6px 0; display: flex; justify-content: space-between; align-items: center;\">";
            html_text += "<a href=\"" + request_uri + name + "\" style=\"text-decoration: none; color: #0066cc; font-weight: bold;\">" + name + "</a>";
            html_text += "</li>\n";
        }
        else
        {
            html_text += "<li style=\"margin: 6px 0; display: flex; justify-content: space-between; align-items: center;\">";
            html_text += "<a href=\"" + request_uri + name + "\" style=\"text-decoration: none; color: #0066cc; font-weight: bold;\">" + name + "</a>";
            html_text += "<button style=\"color:white; background-color:red; padding:4px 8px; border:none; border-radius:4px; cursor:pointer;\" onclick=\"fetch('" + request_uri + name + "', {method: 'DELETE'}).then(r => location.reload());\">Delete</button>";
            html_text += "</li>\n";
        }
    }
    html_text += "</ul>\n</div>\n";
    closedir(directory);
    code = 200;
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
        "    <title>" +
        tostring(code) + std::string(" ") + status_codes[code] + "</title>\n"
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
                                                                 "<h1>" +
        tostring(code) + "</h1>\n"
                         "<p>" +
        status_codes[code] + "</p>\n"
                             "</body>\n"
                             "</html>\n";
    return html;
}

size_t countPath(const std::string line)
{
    size_t count = 0;
    bool in_exp = false;
    for (size_t i = 0; i < line.size(); i++)
    {
        if (line[i] == '/')
            in_exp = false;
        else
        {
            if (!in_exp)
            {
                count++;
                in_exp = true;
            }
        }
    }
    return count;
}
std::string *split_path(const std::string line, size_t &count)
{
    count = countPath(line);
    std::string *parts = new std::string[count];
    size_t i = 0;
    size_t idx = 0;
    size_t size = line.size();
    while (i < size && idx < count)
    {
        while (i < size && line[i] == '/')
            i++;
        if (i >= size)
            break;
        size_t j = i;
        while (j < size)
        {
            if (line[j] == '/')
                break;
            j++;
        }
        parts[idx++] = line.substr(i, j - i);
        i = j;
    }
    return parts;
}

std::string Server::simplifyPath(std::string path)
{
    size_t count = 0;
    std::string *words = split_path(path, count);
    std::string correct_path;
    std::stack<std::string> s;
    for (size_t i = 0; i < count; i++)
    {
        if (words[i] == "..")
        {
            if (!s.empty())
                s.pop();
        }
        else if (words[i] == ".")
        {
            continue;
        }
        else
        {
            s.push(words[i]);
        }
    }
    if (s.empty())
        return "/";
    else
    {
        while (!s.empty())
        {
            correct_path.insert(0, "/" + s.top());
            s.pop();
        }
    }
    delete[] words;
    return (correct_path);
}

std::vector<std::string> Server::get_bodyheaders_Lines(const std::string req)
{
    std::string sub_req;
    std::vector<std::string> lines;
    size_t pos = req.find("\r\n\r\n");
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
    }
    return lines;
}

std::string Server::buildSearchingFile(std::string root, std::string uri, Location location)
{
    std::string result;

    std::string location_path = location.path;
    result += root;
    if (location_path.length() > 1)
        result += uri.substr(location_path.length());
    else
        result += uri;
    return result;
}

void Server::mergeIndexes(Location &location, std::string toSearch)
{
    for (size_t i = 0; i < location.index.size(); i++)
        location.index[i] = toSearch + location.index[i];
}

bool Server::fileValid(std::string path)
{
    struct stat st;

    if (path.empty())
        return false;
    if (stat(path.c_str(), &st) == -1)
        return false;
    else
    {
        if (S_ISREG(st.st_mode))
        {
            if (access(path.c_str(), R_OK) == 0)
                return true;
        }
        else
            return false;
    }
    return false;
}

bool Server::serveClient(int client_fd, Request request, int epoll_fd)
{
    Webserv::Server server;
    if (request.method == "GET")
    {
        server = *clientfd_to_server[client_fd];
        struct stat st;
        if (!isUriExists(request.uri, server, false))
        {
            if (error_pages.count("404") && fileValid(error_pages["404"]))
                return sendFileResponse(client_fd, error_pages["404"], getExtension(error_pages["404"]), 404, request.keep_alive);
            else
            {
                std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
        }
        else
        {
            Webserv::Location location = getLocation(request.uri, server);
            if (!isMethodAllowed("GET", location))
            {
                if (error_pages.count("400") && fileValid(error_pages["400"]))
                    return sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, request.keep_alive);
                else
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
            }
            if (location.isRedirection)
            {
                if (!location.redirectionIsText)
                {
                    if (location.redirect_relative)
                    {
                        if (!isUriExists(location.redirection.second, server, true)) // hadi drnaha 3la hsab return location wach exists
                        {
                            if (error_pages.count("404") && fileValid(error_pages["404"]))
                                return sendFileResponse(client_fd, error_pages["404"], getExtension(error_pages["404"]), 404, request.keep_alive);
                            else
                            {
                                std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
                                return sendResponse(client_fd, response, request.keep_alive);
                            }
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
            std::string toSearch = buildSearchingFile(location.root, request.uri, location);
            size_t pos = simplifyPath(toSearch).find(location.root);
            if (pos == std::string::npos || pos != 0)
            {
                if (error_pages.count("403") && fileValid(error_pages["403"]))
                    return sendFileResponse(client_fd, error_pages["403"], getExtension(error_pages["403"]), 403, request.keep_alive);
                else
                {
                    std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
            }
            if (stat(toSearch.c_str(), &st) == -1)
            {
                if (error_pages.count("404") && fileValid(error_pages["404"]))
                    return sendFileResponse(client_fd, error_pages["404"], getExtension(error_pages["404"]), 404, request.keep_alive);
                else
                {
                    std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
            }
            else
            {
                if (S_ISDIR(st.st_mode) && toSearch[toSearch.size() - 1] == '/')
                {
                    mergeIndexes(location, toSearch);
                    if (!atleastOneFileExists(location))
                    {
                        if (!location.autoindex)
                        {
                            if (error_pages.count("403") && fileValid(error_pages["403"]))
                                return sendFileResponse(client_fd, error_pages["403"], getExtension(error_pages["403"]), 403, request.keep_alive);
                            else
                            {
                                std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                                return sendResponse(client_fd, response, request.keep_alive);
                            }
                        }
                        else
                        {
                            int code;
                            std::string body = dirlisntening_gen(request.uri, toSearch, code);
                            std::string response = buildResponse(body, ".html", code, false, "", request.keep_alive);
                            return sendResponse(client_fd, response, request.keep_alive);
                        }
                    }
                    else
                    {
                        std::string index_file = getFilethatExists(location);
                        if (!index_file.empty())
                        {
                            std::string ext = getExtension(index_file);
                            if (location.hasCgi && (ext == ".py" || ext == ".php"))
                            {
                                if (!setupCGI(request, index_file, ext, client_fd, epoll_fd))
                                {
                                    if (error_pages.count("500") && fileValid(error_pages["500"]))
                                        return sendFileResponse(client_fd, error_pages["500"], getExtension(error_pages["500"]), 500, request.keep_alive);
                                    else
                                    {
                                        std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", request.keep_alive);
                                        return sendResponse(client_fd, response, request.keep_alive);
                                    }
                                }
                                return true;
                            }
                            else
                                return sendFileResponse(client_fd, index_file, ext, 200, request.keep_alive);
                        }
                        else
                        {
                            if (error_pages.count("403") && fileValid(error_pages["403"]))
                                return sendFileResponse(client_fd, error_pages["403"], getExtension(error_pages["403"]), 403, request.keep_alive);
                            else
                            {
                                std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                                return sendResponse(client_fd, response, request.keep_alive);
                            }
                        }
                    }
                }
                else if (S_ISREG(st.st_mode))
                {
                    if (access(toSearch.c_str(), R_OK) == 0)
                    {
                        std::string ext = getExtension(toSearch);
                        std::string response;
                        if (location.hasCgi && (ext == ".py" || ext == ".php"))
                        {
                            if (!setupCGI(request, toSearch, ext, client_fd, epoll_fd))
                            {
                                if (error_pages.count("500") && fileValid(error_pages["500"]))
                                    return sendFileResponse(client_fd, error_pages["500"], getExtension(error_pages["500"]), 500, request.keep_alive);
                                else
                                {
                                    response = buildResponse(buildErrorPage(500), ".html", 500, false, "", request.keep_alive);
                                    return sendResponse(client_fd, response, request.keep_alive);
                                }
                            }
                            return true;
                        }
                        else
                        {
                            return sendFileResponse(client_fd, toSearch, ext, 200, request.keep_alive);
                        }
                    }
                    else
                    {
                        if (error_pages.count("403") && fileValid(error_pages["403"]))
                            return sendFileResponse(client_fd, error_pages["403"], getExtension(error_pages["403"]), 403, request.keep_alive);
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                            return sendResponse(client_fd, response, request.keep_alive);
                        }
                    }
                }
                else
                {
                    if (error_pages.count("404") && fileValid(error_pages["404"]))
                        return sendFileResponse(client_fd, error_pages["404"], getExtension(error_pages["404"]), 404, request.keep_alive);
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
                        return sendResponse(client_fd, response, request.keep_alive);
                    }
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
            if (error_pages.count("404") && fileValid(error_pages["404"]))
                return sendFileResponse(client_fd, error_pages["404"], getExtension(error_pages["404"]), 404, request.keep_alive);
            else
            {
                std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
        }
        else
        {
            Webserv::Location location = getLocation(request.uri, server);
            if (!isMethodAllowed("DELETE", location))
            {
                if (error_pages.count("400") && fileValid(error_pages["400"]))
                    return sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, request.keep_alive);
                else
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
            }
            std::string filepath = buildSearchingFile(location.root, request.uri, location);
            size_t pos = simplifyPath(filepath).find(location.root);
            if (pos == std::string::npos || pos != 0)
            {
                if (error_pages.count("403") && fileValid(error_pages["403"]))
                    return sendFileResponse(client_fd, error_pages["403"], getExtension(error_pages["403"]), 403, request.keep_alive);
                else
                {
                    std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
            }
            if (stat(filepath.c_str(), &st) == -1)
            {
                if (error_pages.count("404") && fileValid(error_pages["404"]))
                    return sendFileResponse(client_fd, error_pages["404"], getExtension(error_pages["404"]), 404, request.keep_alive);
                else
                {
                    std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", request.keep_alive);
                    return sendResponse(client_fd, response, request.keep_alive);
                }
            }
            else
            {
                if (S_ISDIR(st.st_mode))
                {
                    if (error_pages.count("403") && fileValid(error_pages["403"]))
                        return sendFileResponse(client_fd, error_pages["403"], getExtension(error_pages["403"]), 403, request.keep_alive);
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                        return sendResponse(client_fd, response, request.keep_alive);
                    }
                }
                std::string file_dire = filepath.substr(0, filepath.find_last_of('/'));
                if (access(file_dire.c_str(), W_OK | X_OK) != 0)
                {
                    if (error_pages.count("403") && fileValid(error_pages["403"]))
                        return sendFileResponse(client_fd, error_pages["403"], getExtension(error_pages["403"]), 403, request.keep_alive);
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                        return sendResponse(client_fd, response, request.keep_alive);
                    }
                }
                std::string ext = getExtension(filepath);
                if (location.hasCgi && (ext == ".py" || ext == ".php"))
                {
                    if (!setupCGI(request, filepath, ext, client_fd, epoll_fd))
                    {
                        if (error_pages.count("500") && fileValid(error_pages["500"]))
                            return sendFileResponse(client_fd, error_pages["500"], getExtension(error_pages["500"]), 500, request.keep_alive);
                        else
                        {
                            std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", request.keep_alive);
                            return sendResponse(client_fd, response, request.keep_alive);
                        }
                    }
                    return true;
                }
                if (std::remove(filepath.c_str()) != 0)
                {
                    if (error_pages.count("403") && fileValid(error_pages["403"]))
                        return sendFileResponse(client_fd, error_pages["403"], getExtension(error_pages["403"]), 403, request.keep_alive);
                    else
                    {
                        std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                        return sendResponse(client_fd, response, request.keep_alive);
                    }
                }
                std::string response = buildResponse("", "", 204, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
        }
    }
    return request.keep_alive;
}

void Server::closeClient(int epoll_fd, int client_fd, bool inside_loop)
{
    std::vector<int> cgi_to_clean;
    for (std::map<int, CgiState>::iterator it = cgi_states.begin(); it != cgi_states.end(); it++)
        if (it->second.state.client_fd == client_fd)
            cgi_to_clean.push_back(it->first);
    for (size_t i = 0; i < cgi_to_clean.size(); i++)
        cleanupCGI(epoll_fd, cgi_to_clean[i], true);

    client_addresses.erase(client_fd);
    if (inside_loop)
        this->client_fds.erase(std::remove(client_fds.begin(), client_fds.end(), client_fd), client_fds.end());
    read_states.erase(client_fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
}

void Server::checkTimeoutClients(int epoll_fd)
{
    time_t currentTime = std::time(NULL);
    std::map<int, client_read>::iterator it = read_states.begin();
    std::vector<int> fdsToClose;

    for (; it != read_states.end(); it++)
    {
        bool not_done = true;
        int client_fd = it->first;
        client_read &client_ref = it->second;
        if (client_ref.has_start_time)
        {
            double passed_time = difftime(currentTime, client_ref.start_time);
            if (passed_time > 3)
            {
                if (error_pages.count("400") && fileValid(error_pages["400"]))
                    not_done = sendFileResponse(client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, false);
                else
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
                    sendResponse(client_fd, response, false);
                }
                fdsToClose.push_back(client_fd);
            }
        }
        if (!not_done)
            continueSending(client_fd);
    }
    for (size_t i = 0; i < fdsToClose.size(); i++)
        closeClient(epoll_fd, fdsToClose[i], true);
}

void Server::modifyEpollEvents(int epoll_fd, int client_fd, unsigned int events)
{
    epoll_event ev;
    ev.events = events;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
}

bool Server::sendFileResponse(int client_fd, const std::string file_path, const std::string extension, int status, bool keep_alive)
{
    struct stat st;

    if (stat(file_path.c_str(), &st) == -1)
        return false;
    int file_fd = open(file_path.c_str(), O_RDONLY);
    if (file_fd == -1)
        return false;
    this->fileFdstoClose.push_back(file_fd);

    std::string CRLF = "\r\n";
    std::string response_headers;
    response_headers += "HTTP/1.0 " + tostring(status) + " " + status_codes[status] + CRLF;
    std::string type = "text/plain";
    if (content_type.count(extension))
        type = content_type[extension];
    response_headers += "Date: " + currentDate() + CRLF;
    response_headers += "Content-Type: " + type + CRLF;
    response_headers += "Content-Length: " + tostring(st.st_size) + CRLF;
    response_headers += "Connection: " + std::string(keep_alive ? "keep-alive" : "close") + CRLF;
    response_headers += CRLF;

    client_write &state = write_states[client_fd];
    state.is_streaming = true;
    state.file_fd = file_fd;
    state.file_offset = 0;
    state.file_size = st.st_size;
    state.keep_alive = keep_alive;
    state.buffer_offset = 0;
    state.buffer_len = 0;
    state.response_headers = response_headers;
    state.headers_sent = 0;
    state.headers_complete = false;
    state.pending_response.clear();
    state.bytes_sent = 0;
    return continueSending(client_fd);
}

bool Server::sendResponse(int client_fd, const std::string response, bool keep_alive)
{
    client_write &state = write_states[client_fd];
    state.pending_response = response;
    state.bytes_sent = 0;
    state.keep_alive = keep_alive;

    return continueSending(client_fd);
}

bool Server::continueSending(int client_fd)
{
    client_write &state = write_states[client_fd];
    if (state.is_streaming)
    {
        if (!state.headers_complete)
        {
            ssize_t sent = send(client_fd, state.response_headers.c_str() + state.headers_sent, state.response_headers.length() - state.headers_sent, 0);
            if (sent == -1)
                return false;
            if (sent == 0)
            {
                close(state.file_fd);
                write_states.erase(client_fd);
                return true;
            }
            state.headers_sent += sent;
            if (state.headers_sent >= state.response_headers.length())
            {
                state.headers_complete = true;
                state.response_headers.clear();
            }
            return false;
        }
        if (state.buffer_offset >= state.buffer_len)
        {
            if (state.file_offset >= state.file_size)
            {
                close(state.file_fd);
                write_states.erase(client_fd);
                return true;
            }
            ssize_t bytes_read = read(state.file_fd, state.buffer, sizeof(state.buffer));
            if (bytes_read <= 0)
            {
                close(state.file_fd);
                write_states.erase(client_fd);
                return true;
            }
            state.buffer_offset = 0;
            state.buffer_len = bytes_read;
        }
        ssize_t sent = send(client_fd, state.buffer + state.buffer_offset, state.buffer_len - state.buffer_offset, 0);
        if (sent == -1)
            return false;
        if (sent == 0)
        {
            close(state.file_fd);
            write_states.erase(client_fd);
            return true;
        }
        state.buffer_offset += sent;
        state.file_offset += sent;
        return false;
    }
    while (state.bytes_sent < state.pending_response.length())
    {
        ssize_t sent = send(client_fd, state.pending_response.c_str() + state.bytes_sent, state.pending_response.length() - state.bytes_sent, 0);
        if (sent == -1)
            return false;
        if (sent == 0)
            return true;
        state.bytes_sent += sent;
    }
    write_states.erase(client_fd);
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
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
        {
            std::cerr << "Error: setsockopt failed in socket " << i + 1 << "." << std::endl;
            countingFailedSockets++;
            close(sock_fd);
            continue;
        }
        sockaddr_in address = infos(servers[i]);
        if (bind(sock_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == -1)
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
            std::cerr << "Error: epoll_ctl failed in socket " << i + 1 << "." << std::endl;
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
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        checkTimeoutClients(epoll_fd);
        checkTimeoutCGI(epoll_fd);
        for (int i = 0; i < n; i++)
        {
            int fd = events[i].data.fd;
            bool been_here = false;
            bool is_listening = false;
            bool is_post = true;
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
                int client_fd = accept(fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
                if (client_fd != -1)
                {
                    client_addresses[client_fd] = client_addr;
                    std::cout << "New client " << client_fd << " connected from " << getAddress(&client_addr, NULL, false) << ":" << ntohs(client_addr.sin_port) << std::endl;
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
                bool sending_done = continueSending(fd);
                if (write_states.find(fd) == write_states.end())
                {
                    modifyEpollEvents(epoll_fd, fd, EPOLLIN);
                    bool keep_alive = clientfd_to_request[fd].keep_alive;
                    if (!keep_alive && sending_done)
                    {
                        closeClient(epoll_fd, fd, true);
                        continue;
                    }
                }
            }
            else if ((cgi_states.find(fd) != cgi_states.end()) && (events[i].events & (EPOLLIN | EPOLLHUP)))
            {
                handleCGIOutput(epoll_fd, fd);
                continue;
            }
            else if (events[i].events & EPOLLIN)
            {
                if (find(client_fds.begin(), client_fds.end(), fd) == client_fds.end())
                    continue;
                std::string request_string = readRequest(epoll_fd, fd, been_here);
                Request request;
                if (read_states.find(fd) != read_states.end())
                {
                    client_read &client_read_state = read_states[fd];
                    if (!client_read_state.is_request_full)
                        continue;
                    request_string = client_read_state.headers;
                    is_post = client_read_state.is_post;
                    request = client_read_state.request;
                    if (client_read_state.client_disconnected)
                        closeClient(epoll_fd, fd, true);
                    read_states.erase(fd);
                }
                if (!is_post && !parseRequest(fd, request_string, request, false, been_here) && !request.has_error_page)
                {
                    closeClient(epoll_fd, fd, true);
                    continue;
                }
                if (!is_post)
                {
                    if (request.method == "DELETE")
                        std::cout << RED << "[ " << request.method << " ] " << RESET << request.uri << " " << request.remote_addr << ":" << request.remote_port << std::endl;
                    else
                        std::cout << GREEN << "[ " << request.method << " ] " << RESET << request.uri << " " << request.remote_addr << ":" << request.remote_port << std::endl;
                }
                clientfd_to_request[fd] = request;
                bool sending_done = false;
                if (!is_post)
                    sending_done = serveClient(fd, request, epoll_fd);
                for (std::map<int, CgiState>::iterator it = cgi_states.begin(); it != cgi_states.end(); it++)
                {
                    if (it->second.state.client_fd == fd && !it->second.added_to_epoll)
                    {
                        epoll_event ev;
                        ev.events = EPOLLIN;
                        ev.data.fd = it->first;
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, it->first, &ev);
                        it->second.added_to_epoll = true;
                    }
                }
                if (!sending_done || (request.has_error_page && !request.sent_all_error_page))
                    modifyEpollEvents(epoll_fd, fd, EPOLLIN | EPOLLOUT);
                else if (!request.keep_alive && (sending_done || request.sent_all_error_page))
                    closeClient(epoll_fd, fd, true);
            }
        }
    }
    this->closeSockets();
    for (size_t i = 0; i < this->client_fds.size(); i++)
        closeClient(epoll_fd, client_fds[i], false);
    for (size_t i = 0; i < this->fileFdstoClose.size(); i++)
        close(this->fileFdstoClose[i]);
    close(epoll_fd);
}

bool Server::setupCGI(Request &request, std::string &script_path, std::string &extension, int client_fd, int epoll_fd)
{
    CGI cgi(this, request, script_path, extension);

    CgiState cgi_state;
    cgi_state.state.client_fd = client_fd;
    cgi_state.start_time = time(NULL);
    if (!cgi.start(cgi_state.state))
        return false;
    if (cgi_state.state.pipe_out[0] != -1)
    {
        if (!setNonBlockingFD(cgi_state.state.pipe_out[0]) || !setNonBlockingFD(cgi_state.state.pipe_err[0]))
        {
            cleanupCGI(epoll_fd, cgi_state.state.pipe_out[0], true);
            return false;
        }
        cgi_states[cgi_state.state.pipe_out[0]] = cgi_state;
        return true;
    }
    return false;
}

void Server::handleCGIOutput(int epoll_fd, int pipe_fd)
{
    if (cgi_states.find(pipe_fd) == cgi_states.end())
        return;

    CgiState &cgi_state = cgi_states[pipe_fd];
    CGI cgi(this, cgi_state.state.request, cgi_state.state.script_path, cgi_state.state.extension);
    cgi.handleOutput(cgi_state.state);
    if (cgi_state.state.process_complete)
    {
        if (!cgi_state.state.response_sent_to_client)
        {
            std::string response;
            bool error_status_in_cgi = false; // in case header fcgi status kan error
            int error_status_code_cgi = 0;
            if (!cgi_state.state.stdout_output.empty())
                response = CGI::buildResponseFromState(this, cgi_state.state, cgi_state.state.request.keep_alive, error_status_in_cgi, error_status_code_cgi);
            else if (cgi_state.state.syntax_error)
                response = CGI::buildErrorResponse(this, cgi_state.state);
            else
            {
                if (error_pages.count("500") && fileValid(error_pages["500"]))
                    sendFileResponse(cgi_state.state.client_fd, error_pages["500"], getExtension(error_pages["500"]), 500, cgi_state.state.request.keep_alive);
                else
                    response = buildResponse(buildErrorPage(500), ".html", 500, false, "", false);
            }
            // in case kan error, check first if error page msetiya flconfig else serve default ones
            if (error_status_in_cgi && error_pages.count(tostring(error_status_code_cgi)) && fileValid(error_pages[tostring(error_status_code_cgi)]))
            {
                response = "";
                sendFileResponse(cgi_state.state.client_fd, error_pages[tostring(error_status_code_cgi)], getExtension(error_pages[tostring(error_status_code_cgi)]), error_status_code_cgi, cgi_state.state.request.keep_alive);
            }
            cgi_state.state.response_sent_to_client = true;
            bool fully_sent = sendResponse(cgi_state.state.client_fd, response, cgi_state.state.request.keep_alive);
            if (write_states.find(cgi_state.state.client_fd) != write_states.end())
                modifyEpollEvents(epoll_fd, cgi_state.state.client_fd, EPOLLIN | EPOLLOUT);
            else if (fully_sent && !cgi_state.state.request.keep_alive)
                closeClient(epoll_fd, cgi_state.state.client_fd, true);
        }
        cleanupCGI(epoll_fd, pipe_fd, false);
    }
}

void Server::cleanupCGI(int epoll_fd, int pipe_fd, bool kill_process)
{
    if (cgi_states.find(pipe_fd) == cgi_states.end())
        return;

    CgiState &cgi_state = cgi_states[pipe_fd];
    CGI cgi(this, cgi_state.state.request, cgi_state.state.script_path, cgi_state.state.extension);
    if (cgi_state.added_to_epoll)
    {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, pipe_fd, NULL);
        cgi_state.added_to_epoll = false;
    }
    cgi.cleanup(cgi_state.state, kill_process);
    cgi_states.erase(pipe_fd);
}

void Server::checkTimeoutCGI(int epoll_fd)
{
    time_t now = time(NULL);
    std::vector<int> timed_out;

    for (std::map<int, CgiState>::iterator it = cgi_states.begin(); it != cgi_states.end(); it++)
    {
        bool not_done = true;
        if (difftime(now, it->second.start_time) > 3)
        {
            // std::cerr << "CGI timeout for PID " << it->second.state.pid << std::endl;
            it->second.state.process_complete = true;
            it->second.state.response_sent_to_client = true;
            std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", it->second.state.request.keep_alive);
            if (error_pages.count("400") && fileValid(error_pages["400"]))
                not_done = sendFileResponse(it->second.state.client_fd, error_pages["400"], getExtension(error_pages["400"]), 400, it->second.state.request.keep_alive);
            else
                sendResponse(it->second.state.client_fd, response, it->second.state.request.keep_alive);
            timed_out.push_back(it->first);
        }
        if (!not_done)
            continueSending(it->second.state.client_fd);
    }
    for (size_t i = 0; i < timed_out.size(); i++)
        cleanupCGI(epoll_fd, timed_out[i], true);
}