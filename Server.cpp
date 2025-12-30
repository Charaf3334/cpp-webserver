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
    response += "Content-Type: " + type + CRLF;
    if (inRedirection)
        response += "Location: " + newPath + CRLF;

    // set cookies
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

std::string Server::readRequest(int epoll_fd, int client_fd)
{
    client_read &client = read_states[client_fd];
    Webserv::Server server = *clientfd_to_server[client_fd];
    std::string CRLF = "\r\n\r\n";
    client.just_parsed = false;

    if (!client.has_start_time)
    {
        client.total_bytes_written = 0;
        client.loop_bytes_written = 0;
        client.buffer_size = 0;
        client.content_len = 0;
        client.is_request_full = false;
        client.headers = "";
        client.temporary_body = "";
        client.isParsed = false;
        client.content_lenght_present = false;
        client.first_time = 0;
        client.should_ignore = false;
        client.is_post = false;
        client.request.is_uri_dir = false;
        client.request.is_uri_reg = false;
        client.request.body_headers_done = false;
        client.temporary_body = "";
        client.boundary_found = false;
        client.just_took_headers = false;
        client.keep_alive = false;
    }
    if (!client.has_start_time || client.first_time > 0)
    {
        gettimeofday(&client.start_time, NULL);
        client.has_start_time = true;
    }
    memset(client.buffer, 0 ,sizeof(client.buffer));
    ssize_t bytes = read(client_fd, client.buffer, sizeof(client.buffer));
    if (bytes < 0)
    {
        client.is_request_full = false;
        return client.headers;
    }
    if (bytes == 0)
    {
        client.is_request_full = true;
        std::cout << "Client disconnected" << std::endl;
        return client.headers;
    }
    if (!client.isParsed)
    {
        client.headers.append(client.buffer, bytes);
        size_t CRLF_pos = client.headers.find(CRLF);
        if (CRLF_pos == std::string::npos)
        {
            std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
            sendResponse(client_fd, response, false);
            client.is_request_full = true;
            closeClient(epoll_fd, client_fd, true);
            return "";
        }
        client.isParsed = true;
        client.just_parsed = true;
        client.temporary_body = client.headers.substr(CRLF_pos + 4);
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
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
                    sendResponse(client_fd, response, false);
                    client.is_request_full = true;
                    closeClient(epoll_fd, client_fd, true);
                    return "";
                }
                client.content_lenght_present = true;
                client.content_len = std::atoll(content_length_value.c_str());
            }
        }
        size_t method_pos = client.headers.find("POST");
        client.is_post = (method_pos != std::string::npos && method_pos == 0) ? true : false;
        if (client.is_post)
        {
            if (!parseRequest(client_fd, client.headers, client.request))
            {
                std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
                sendResponse(client_fd, response, false);
                client.is_request_full = true;
                return "";
            }

            struct stat st;
            if (!isUriExists(client.request.uri, server, false))
            {
                std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", client.request.keep_alive);
                sendResponse(client_fd, response, client.request.keep_alive);
                client.is_request_full = true;
                client.keep_alive = client.request.keep_alive;
                return "";
            }
            else
            {
                client.request.location = getLocation(client.request.uri, server);
                if (!isMethodAllowed("POST", client.request.location))
                {
                    std::string response = buildResponse(buildErrorPage(405), ".html", 405, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }

                std::map<std::string, std::string>::iterator cl_it = client.request.headers.find("content-length");
                if (cl_it != client.request.headers.end())
                {
                    size_t content_length = atol(cl_it->second.c_str());
                    if (content_length > client_max_body_size)
                    {
                        std::string response = buildResponse(buildErrorPage(413), ".html", 413, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }

                std::string toSearch = client.request.location.root + client.request.uri;
                size_t pos = simplifyPath(toSearch).find(client.request.location.root);
                if (pos == std::string::npos || pos != 0)
                {
                    std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                if (stat(toSearch.c_str(), &st) == -1)
                {
                    std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                if (S_ISREG(st.st_mode))
                    client.request.is_uri_reg = true;
                else if (S_ISDIR(st.st_mode))
                    client.request.is_uri_dir = true;
                else
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                std::string upload_dir = client.request.location.upload_dir;
                if (stat(upload_dir.c_str(), &st) == -1)
                {
                    std::string response = buildResponse(buildErrorPage(404), ".html", 404, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                if (!S_ISDIR(st.st_mode))
                {
                    std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                if (access(upload_dir.c_str(), W_OK) == -1)
                {
                    std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                if (client.request.headers.find("content-type") == client.request.headers.end())
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                size_t multipart_pos = client.request.headers["content-type"].find("multipart/form-data;");
                if (multipart_pos == std::string::npos && !client.request.is_uri_reg)
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                if (client.request.is_uri_reg) {
                    goto cgi;
                }
                size_t boundary_pos = client.request.headers["content-type"].find("boundary=");
                if (boundary_pos == std::string::npos)
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                client.request.body_boundary = "--" + client.request.headers["content-type"].substr(boundary_pos + 9);
            }
        }
    }
    cgi:
        if (client.content_lenght_present && client.is_post && client.request.is_uri_reg)
        {
            if (client.content_len > 60000)
            {
                std::string response = buildResponse(buildErrorPage(413), ".html", 413, false, "", client.request.keep_alive);
                sendResponse(client_fd, response, client.request.keep_alive);
                client.is_request_full = true;
                client.keep_alive = client.request.keep_alive;
                return "";
            }
            else
                client.request.body = client.temporary_body;     
            std::string toSearch = client.request.location.root + client.request.uri;
            std::string ext = getExtension(toSearch);
            if (getLocation(client.request.uri, server).hasCgi && (ext == ".py" || ext == ".php"))
            {
                if (!setupCGI(client.request, toSearch, ext, client_fd))
                {
                    std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                client.keep_alive = client.request.keep_alive;
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
    if (client.content_lenght_present && client.is_post && client.request.is_uri_dir)
    {
        if (client.just_parsed)
            client.body_buffer = client.temporary_body;
        else
            client.body_buffer = client.temporary_body + std::string(client.buffer, bytes);
        client.buffer_size = client.body_buffer.size();
        client.temporary_body = "";
        while (client.loop_bytes_written < client.buffer_size)
        {
            if (!client.request.body_headers_done)
            {
                size_t body_crlf = client.body_buffer.find("\r\n\r\n");
                if (body_crlf == std::string::npos)
                {
                    client.temporary_body = client.body_buffer;
                    return "";
                }
                std::string body_headers_string = client.body_buffer.substr(0, body_crlf);
                std::vector<std::string> body_headers_array = get_bodyheaders_Lines(body_headers_string);
                int no_use;
                for (size_t i = 0; i < body_headers_array.size(); i++)
                {
                    if (!parse_headers(body_headers_array[i], client.request.body_headers, no_use, i, client.request.body_boundary))
                    {
                        std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
                if (client.request.body_headers.find("content-disposition") == client.request.body_headers.end())
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                size_t filename_pos = client.request.body_headers["content-disposition"].find("filename=");
                if (filename_pos == std::string::npos)
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                client.request.bodyfile_name = client.request.body_headers["content-disposition"].substr(filename_pos + 9);
                client.request.bodyfile_name.erase(0, 1);
                client.request.bodyfile_name.erase(client.request.bodyfile_name.size() - 1, 1);
                std::string upload_dir = client.request.location.upload_dir + "/" + "Client_" + tostring(client_fd);
                mkdir(upload_dir.c_str(), 0777);
                std::string upload_file = upload_dir + "/" + client.request.bodyfile_name;
                client.file_fd = open(upload_file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (client.file_fd == -1)
                {
                    std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                client.total_bytes_written += body_headers_string.size() + 4;
                client.loop_bytes_written += body_headers_string.size() + 4;
                if (client.body_buffer.size() > body_crlf + 4)
                    client.body_buffer = client.body_buffer.substr(body_crlf + 4);
                client.request.body_headers_done = true;
                continue;
            }
            //==================================================================BODY WRITE=======================================================================
            size_t boundry_pos = client.body_buffer.find("\r\n" + client.request.body_boundary + "\r\n");
            size_t end_boundry_pos = client.body_buffer.find("\r\n" + client.request.body_boundary + "--\r\n");
            if (boundry_pos != std::string::npos)
            {
                client.request.body_headers_done = false;
                client.to_write = client.body_buffer.substr(0, boundry_pos);
                client.body_buffer = client.body_buffer.substr(boundry_pos + 2);
                client.total_bytes_written += 2;
                client.loop_bytes_written += 2;
            }
            else if (end_boundry_pos != std::string::npos)
            {
                client.temporary_body = client.body_buffer.substr(end_boundry_pos + client.request.body_boundary.size() + 6);
                if (!client.temporary_body.empty())
                {
                    std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                client.to_write = client.body_buffer.substr(0, end_boundry_pos);
                client.total_bytes_written += client.request.body_boundary.size() + 6;
                client.loop_bytes_written += client.request.body_boundary.size() + 6;
                client.boundary_found = true;
            }
            else
            {
                size_t r_pos = client.body_buffer.find("\r");
                if (r_pos == std::string::npos)
                    client.to_write = client.body_buffer;
                else
                {
                    client.temporary_body = client.body_buffer.substr(r_pos);
                    if (client.temporary_body.size() > client.request.body_boundary.size() + 6)
                    {
                        client.to_write = client.body_buffer;
                        client.temporary_body = "";
                    }
                    else
                    {
                        client.to_write = client.body_buffer.substr(0, r_pos);
                        client.body_buffer = client.temporary_body;
                    }
                }
            }
            int written = write(client.file_fd, client.to_write.c_str(), client.to_write.size());
            if (written < (int)client.to_write.size())
            {
                if (written == -1)
                {
                    std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", client.request.keep_alive);
                    sendResponse(client_fd, response, client.request.keep_alive);
                    close(client.file_fd);
                    client.is_request_full = true;
                    client.keep_alive = client.request.keep_alive;
                    return "";
                }
                else
                {
                    if (write(client.file_fd, client.to_write.substr(written).c_str(), client.to_write.size() - written) == -1)
                    {
                        std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", client.request.keep_alive);
                        sendResponse(client_fd, response, client.request.keep_alive);
                        close(client.file_fd);
                        client.is_request_full = true;
                        client.keep_alive = client.request.keep_alive;
                        return "";
                    }
                }
            }
            client.total_bytes_written += client.to_write.size();
            client.loop_bytes_written += client.to_write.size();
        }
        client.loop_bytes_written = 0;
        if (client.boundary_found)
        {
            if (!client.request.body_headers_done)
            {
                close(client.file_fd);
                client.boundary_found = false;
                return "";
            }
            close(client.file_fd);
            std::string response = buildResponse(buildErrorPage(201), ".html", 201, false, "", client.request.keep_alive);
            sendResponse(client_fd, response, client.request.keep_alive);
            client.keep_alive = client.request.keep_alive;
        }
    }
    //===================================================POST END===============================================================
    if (client.is_post && !client.content_lenght_present)
    {
        std::string response = buildResponse(buildErrorPage(411), ".html", 411, false, "", false);
        sendResponse(client_fd, response, false);
        client.is_request_full = true;
        return "";
    }
    if (!client.is_post && client.content_lenght_present)
    {
        client.temporary_body = "";
        client.temporary_file_fd = open("/home/tibarike/goinfre/trash", O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (client.temporary_file_fd < 0)
        {
            std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", false);
            sendResponse(client_fd, response, false);
            client.is_request_full = true;
            return "";
        }
        ssize_t written = write(client.temporary_file_fd, client.buffer, bytes);
        if (written == -1)
        {
            close(client.temporary_file_fd);
            std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", false);
            sendResponse(client_fd, response, false);
            client.is_request_full = true;
            return "";
        }
        close(client.temporary_file_fd);
        client.total_bytes_written += written;
    }
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
                std::string response = buildResponse(buildErrorPage(400), ".html", 400, false, "", false);
                sendResponse(client_fd, response, false);
                client.is_request_full = true;
                closeClient(epoll_fd, client_fd, true);
                return client.headers;
            }
            client.is_request_full = true;
            return client.headers;
        }
        else if (client.total_bytes_written < client.content_len)
        {
            client.first_time++;
            client.is_request_full = false;
            return client.headers;
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
        if (!isalnum(path[i]) && path[i] != '/' && path[i] != '_' && path[i] != '-' && path[i] != '.' && path[i] != '&' && path[i] != '=' && path[i] != '?')
            return false;
    }
    return true;
}

bool Server::parse_methode(std::string *words, int &error_status, Request &request)
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
            continue;
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

bool Server::parse_headers(std::string &line, std::map<std::string, std::string> &map, int &error_status, int option, const std::string boundary)
{
    // std::cout << "line: " << line << std::endl;
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
    // std::cout << "first: " << first << std::endl;
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

bool Server::parse_lines(std::vector<std::string> lines, Request &request, int &error_status)
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

bool Server::parseRequest(int client_fd, std::string request_string, Request &request) // this is the core function that processes the http requests our webserver receives
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
    std::map<std::string, std::string>::iterator found = request.headers.find("connection");
    request.keep_alive = false;
    if (found != request.headers.end() && found->second == "keep-alive")
        request.keep_alive = true;

    if (client_addresses.find(client_fd) != client_addresses.end())
    { // zakaria
        sockaddr_in addr = client_addresses[client_fd];
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
        request.remote_addr = ip_str;
        request.remote_port = ntohs(addr.sin_port);
    }
    else
    {
        request.remote_addr = "127.0.0.1";
        request.remote_port = 0;
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
        return "<html><body><h1>Forbidden</h1></body></html>";
    }
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

bool Server::serveClient(int client_fd, Request request)
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
            size_t pos = simplifyPath(toSearch).find(location.root);
            if (pos == std::string::npos || pos != 0)
            {
                std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
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
                            int code;
                            std::string body = dirlisntening_gen(request.uri, location.root + request.uri, code);
                            std::string response = buildResponse(body, ".html", code, false, "", request.keep_alive);
                            return sendResponse(client_fd, response, request.keep_alive);
                        }
                    }
                    else
                    {
                        std::string index_file = getFilethatExists(location);
                        if (!index_file.empty())
                        {
                            return sendFileResponse(client_fd, index_file, getExtension(index_file), 200, request.keep_alive);
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
                        if (getLocation(request.uri, server).hasCgi && (ext == ".py" || ext == ".php"))
                        {
                            // non blocking cgi
                            if (!setupCGI(request, toSearch, ext, client_fd))
                            {
                                std::string response = buildResponse(buildErrorPage(500), ".html", 500, false, "", request.keep_alive);
                                return sendResponse(client_fd, response, request.keep_alive);
                            }
                            return true;
                        }
                        else
                        {
                            return sendFileResponse(client_fd, toSearch, ext, 200, request.keep_alive);
                        }
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
            size_t pos = simplifyPath(filepath).find(location.root);
            if (pos == std::string::npos || pos != 0)
            {
                std::string response = buildResponse(buildErrorPage(403), ".html", 403, false, "", request.keep_alive);
                return sendResponse(client_fd, response, request.keep_alive);
            }
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
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    std::map<int, client_read>::iterator it = read_states.begin();
    std::vector<int> fdsToClose;

    for (; it != read_states.end(); it++)
    {
        int client_fd = it->first;
        client_read &client_ref = it->second;
        if (client_ref.has_start_time && !client_ref.is_request_full && (!client_ref.isParsed || client_ref.first_time > 0))
        {
            long passed_time = ((currentTime.tv_sec - client_ref.start_time.tv_sec) * 1000) + ((currentTime.tv_usec - client_ref.start_time.tv_usec) / 1000);
            if (passed_time > 3000)
            {
                std::string response = buildResponse(buildErrorPage(408), ".html", 408, false, "", false);
                sendResponse(client_fd, response, false);
                fdsToClose.push_back(client_fd);
            }
        }
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

bool Server::sendFileResponse(int client_fd, const std::string file_path, const std::string extension, int status, bool keep_alive, const std::vector<std::pair<std::string, std::string> > &extra_headers)
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
    for (size_t i = 0; i < extra_headers.size(); i++)
    {
        const std::string key = extra_headers[i].first;
        if (key == "content-type" || key == "content-length")
            continue;
        response_headers += key + ": " + extra_headers[i].second + CRLF;
    }
    response_headers += "Content-Length: " + tostring(st.st_size) + CRLF;
    response_headers += "Connection: " + std::string(keep_alive ? "keep-alive" : "close") + CRLF;
    response_headers += CRLF;

    ClientState &state = client_states[client_fd];
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
    ClientState &state = client_states[client_fd];
    state.pending_response = response;
    state.bytes_sent = 0;
    state.keep_alive = keep_alive;

    return continueSending(client_fd);
}

bool Server::continueSending(int client_fd)
{
    ClientState &state = client_states[client_fd];

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
                client_states.erase(client_fd);
                return false;
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
                bool keep_alive = state.keep_alive;
                client_states.erase(client_fd);
                return keep_alive;
            }
            ssize_t bytes_read = read(state.file_fd, state.buffer, sizeof(state.buffer));
            if (bytes_read <= 0)
            {
                close(state.file_fd);
                client_states.erase(client_fd);
                return false;
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
            client_states.erase(client_fd);
            return false;
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
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        checkTimeoutClients(epoll_fd);
        checkTimeoutCGI(epoll_fd);
        for (int i = 0; i < n; i++)
        {
            int fd = events[i].data.fd;
            if (cgi_states.find(fd) != cgi_states.end())
            {
                handleCGIOutput(epoll_fd, fd);
                continue;
            }

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
                int client_fd = accept(fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
                if (client_fd != -1)
                {
                    client_addresses[client_fd] = client_addr; // zakaria
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
                    int client_port = ntohs(client_addr.sin_port);
                    std::cout << "New client " << client_fd << " connected from: " << client_ip << ":" << client_port << std::endl;
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
                bool keep_alive = continueSending(fd);
                if (client_states.find(fd) == client_states.end())
                {
                    modifyEpollEvents(epoll_fd, fd, EPOLLIN);
                    if (!keep_alive)
                    {
                        closeClient(epoll_fd, fd, true);
                        continue;
                    }
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                std::string request_string = readRequest(epoll_fd, fd);
                if (read_states.find(fd) != read_states.end())
                {
                    client_read &client_read_state = read_states[fd];
                    if (!client_read_state.is_request_full)
                        continue;
                    request_string = client_read_state.headers;
                }
                if (request_string.empty())
                {
                    closeClient(epoll_fd, fd, true);
                    continue;
                }
                Request request;
                if (!parseRequest(fd, request_string, request))
                {
                    closeClient(epoll_fd, fd, true);
                    continue;
                }
                bool keep_alive = serveClient(fd, request);
                for (std::map<int, CgiState>::iterator it = cgi_states.begin(); it != cgi_states.end(); it++)
                {
                    if (it->second.state.client_fd == fd && !it->second.added_to_epoll)
                    {
                        // add CGI pipe_out to epoll
                        epoll_event ev;
                        ev.events = EPOLLIN;
                        ev.data.fd = it->first;
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, it->first, &ev);
                        it->second.added_to_epoll = true;
                    }
                }
                read_states.erase(fd);
                if (client_states.find(fd) != client_states.end())
                {
                    modifyEpollEvents(epoll_fd, fd, EPOLLIN | EPOLLOUT);
                }
                else if (!keep_alive)
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

// cgi block server

bool Server::setupCGI(Request &request, std::string &script_path, std::string &extension, int client_fd)
{
    CGI cgi(this, request, script_path, extension);

    CgiState cgi_state;
    cgi_state.state.client_fd = client_fd;
    cgi_state.start_time = time(NULL);

    if (!cgi.start(cgi_state.state))
        return false;

    if (cgi_state.state.pipe_out[0] != -1)
    {
        setNonBlockingFD(cgi_state.state.pipe_out[0]);
        setNonBlockingFD(cgi_state.state.pipe_err[0]);

        cgi_states[cgi_state.state.pipe_out[0]] = cgi_state;
        return true;
    }
    return false;
}


void Server::handleCGIOutput(int epoll_fd, int pipe_fd)
{
    CgiState &cgi_state = cgi_states[pipe_fd];
    CGI cgi(this, cgi_state.state.request, cgi_state.state.script_path, cgi_state.state.extension);

    bool more_data = cgi.handleOutput(cgi_state.state);

    if (!more_data || cgi_state.state.process_complete)
    {
        // std::cout << "[DEBUG] handleCGIOutput: pipe_fd=" << pipe_fd
        //           << ", client_fd=" << cgi_state.state.client_fd
        //           << ", output_size=" << cgi_state.state.output.size()
        //           << ", headers_complete=" << cgi_state.state.headers_complete
        //           << ", more_data=" << more_data
        //           << ", process_complete=" << cgi_state.state.process_complete
        //           << std::endl;

        // Only send response once
        if (!cgi_state.state.response_sent_to_client && !cgi_state.state.output.empty())
        {
            std::string response;
            if (cgi_state.state.syntax_error)
            {
                response = CGI::buildErrorResponse(this, cgi_state.state);
            }
            else
            {
                response = CGI::buildResponseFromState(this, cgi_state.state, cgi_state.state.request.keep_alive);
            }

            std::cout << "response length (handleCGIOutput): " << response.size() << std::endl;

            cgi_state.state.response_sent_to_client = true; // Mark as sent BEFORE sending

            bool fully_sent = sendResponse(cgi_state.state.client_fd, response, cgi_state.state.request.keep_alive);

            if (client_states.find(cgi_state.state.client_fd) != client_states.end())
            {
                // if data is pending, register for EPOLLOUT to continue sending
                modifyEpollEvents(epoll_fd, cgi_state.state.client_fd, EPOLLIN | EPOLLOUT);
            }
            else if (!fully_sent || !cgi_state.state.request.keep_alive)
            {
                closeClient(epoll_fd, cgi_state.state.client_fd, true);
            }
            // If fully_sent && keep_alive, client stays open for next request
        }
        else if (cgi_state.state.output.empty())
        {
            sendResponse(cgi_state.state.client_fd, buildResponse(buildErrorPage(500), ".html", 500, false, "", false), false);
            closeClient(epoll_fd, cgi_state.state.client_fd, true);
        }

        if (cgi_state.state.process_complete)
        {
            cleanupCGI(epoll_fd, pipe_fd, true);
        }
    }
}

void Server::cleanupCGI(int epoll_fd, int pipe_fd, bool kill_process)
{
    if (cgi_states.find(pipe_fd) == cgi_states.end())
        return;

    CgiState &cgi_state = cgi_states[pipe_fd];

    CGI cgi(this, cgi_state.state.request, cgi_state.state.script_path, cgi_state.state.extension);

    if (cgi_state.added_to_epoll) // keep alive from request zakaria
    {
        (void) epoll_fd;
        // std::cout << "removed from epoll\n";
        // epoll_ctl(epoll_fd, EPOLL_CTL_DEL, pipe_fd, NULL);
        // cgi_state.added_to_epoll = false;
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
        if (difftime(now, it->second.start_time) > CGI_TIMEOUT)
        {
            std::cerr << "CGI timeout for PID " << it->second.state.pid << std::endl;
            std::string response = buildResponse(buildErrorPage(504), ".html", 504, false, "", it->second.state.request.keep_alive);
            sendResponse(it->second.state.client_fd, response, it->second.state.request.keep_alive);

            timed_out.push_back(it->first);
        }
    }

    for (size_t i = 0; i < timed_out.size(); i++)
        cleanupCGI(epoll_fd, timed_out[i], true);
}