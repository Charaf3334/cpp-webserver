#include "Webserv.hpp"

Webserv::Webserv()
{
    throw std::runtime_error("Error: Empty server object.");
}

Webserv::Webserv(const std::string config_file_path) : config_file(config_file_path.c_str())
{
    if (!config_file.is_open())
        throw std::runtime_error("Error: Problem occured while opening the file.");
    if (!checkFileExtension(config_file_path, ".conf"))
        throw std::runtime_error("Error: Unknown file extension, use '.conf' files.");
    if (this->isFileEmpty())
        throw std::runtime_error("Error: Config file is empty.");
    assignStatusCodes();
    assignContentType();
}

Webserv::Webserv(const Webserv &theOtherObject)
{
    this->tokens = theOtherObject.tokens;
    this->servers = theOtherObject.servers;
    this->error_pages = theOtherObject.error_pages;
    this->client_max_body_size = theOtherObject.client_max_body_size;
    this->brackets = theOtherObject.brackets;
    this->status_codes = theOtherObject.status_codes;
    this->content_type = theOtherObject.content_type;
}

Webserv& Webserv::operator=(const Webserv &theOtherObject)
{
    if (this != &theOtherObject)
    {
        this->tokens = theOtherObject.tokens;
        this->servers = theOtherObject.servers;
        this->error_pages = theOtherObject.error_pages;
        this->client_max_body_size = theOtherObject.client_max_body_size;
        this->brackets = theOtherObject.brackets;
        this->status_codes = theOtherObject.status_codes;
        this->content_type = theOtherObject.content_type;
    }
    return *this;
}

Webserv::~Webserv()
{
    config_file.close();
}

bool Webserv::isFileEmpty(void)
{
    char c;
    
    while (this->config_file.get(c))
    {
        if (!std::isspace(static_cast<unsigned char>(c)))
            return false;
    }
    return true;
}

bool Webserv::checkFileExtension(const std::string path, const std::string extension) const
{
    size_t dot = path.rfind('.');

    if (dot == std::string::npos || dot == 0)
        return false;
    std::string ext = path.substr(dot);
    return ext == extension;
}

size_t Webserv::countParts(const std::string line) const
{
    size_t count = 0;
    bool in_exp = false;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); i++)
    {
        if (line[i] == '"')
            in_quotes = !in_quotes;
        else if (!in_quotes && isspace(line[i]))
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

std::string* Webserv::split(const std::string line)
{
    size_t count = this->countParts(line);
    std::string* parts = new std::string[count];
    size_t i = 0;
    size_t idx = 0;
    bool in_quotes = false;
    size_t size = line.size();

    while (i < size && idx < count)
    {
        while (i < size && isspace(line[i]) && !in_quotes)
            i++;
        if (i >= size)
            break;
        size_t j = i;
        while (j < size)
        {
            if (line[j] == '"')
                in_quotes = !in_quotes;
            else if (!in_quotes && isspace(line[j]))
                break;
            j++;
        }
        parts[idx++] = line.substr(i, j - i);
        i = j;
    }
    return parts;
}

std::vector<std::string> Webserv::semicolonBracketsFix(const std::vector<std::string> input)
{
    std::vector<std::string> result;

    for (size_t i = 0; i < input.size(); i++)
    {
        const std::string s = input[i];
        std::string temp;
        for (size_t j = 0; j < s.size(); j++)
        {
            char c = s[j];

            if (c == ';' || c == '{' || c == '}')
            {
                if (!temp.empty())
                {
                    result.push_back(temp);
                    temp.clear();
                }
                result.push_back(std::string(1, c));
            }
            else
                temp += c;
        }
        if (!temp.empty())
            result.push_back(temp);
    }
    return result;
}

void Webserv::assignStatusCodes(void)
{
    status_codes[200] = "OK";
    status_codes[201] = "Created";
    status_codes[202] = "Accepted";
    status_codes[204] = "No Content";
    status_codes[300] = "Multiple Choices";
    status_codes[301] = "Moved Permanently";
    status_codes[302] = "Moved Temporarily";
    status_codes[304] = "Not Modified";
    status_codes[400] = "Bad Request";
    status_codes[401] = "Unauthorized";
    status_codes[403] = "Forbidden";
    status_codes[404] = "Not Found";
    status_codes[405] = "Method Not Allowed"; // HTTP/1.1
    status_codes[408] = "Request Timeout"; // HTTP/1.1
    status_codes[411] = "length required"; // HTTP/1.1
    status_codes[413] = "Payload Too Large"; // HTTP/1.1
    status_codes[500] = "Internal Server Error";
    status_codes[501] = "Not Implemented";
    status_codes[502] = "Bad Gateway";
    status_codes[503] = "Service Unavailable";
    status_codes[504] = "Gateway Timeout"; // HTTP/1.1
}

void Webserv::assignContentType(void)
{
    content_type[".aac"] = "audio/aac";
    content_type[".abw"] = "application/x-abiword";
    content_type[".apng"] = "image/apng";
    content_type[".arc"] = "application/x-freearc";
    content_type[".avif"] = "image/avif";
    content_type[".azw"] = "application/vnd.amazon.ebook";
    content_type[".avi"] = "video/x-msvideo";
    content_type[".bin"] = "application/octet-stream";
    content_type[".bmp"] = "image/bmp";
    content_type[".bz"] = "application/x-bzip";
    content_type[".bz2"] = "application/x-bzip2";
    content_type[".cda"] = "application/x-cdf";
    content_type[".csh"] = "application/x-csh";
    content_type[".css"] = "text/css";
    content_type[".csv"] = "text/csv";
    content_type[".doc"] = "application/msword";
    content_type[".docx"] = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    content_type[".eot"] = "application/vnd.ms-fontobject";
    content_type[".epub"] = "application/epub+zip";
    content_type[".gz"] = "application/gzip";
    content_type[".gif"] = "image/gif";
    content_type[".htm"] = "text/html";
    content_type[".html"] = "text/html";
    content_type[".ico"] = "image/vnd.microsoft.icon";
    content_type[".ics"] = "text/calendar";
    content_type[".jar"] = "application/java-archive";
    content_type[".jpeg"] = "image/jpeg";
    content_type[".jpg"] = "image/jpeg";
    content_type[".js"] = "text/javascript";
    content_type[".json"] = "application/json";
    content_type[".jsonld"] = "application/ld+json";
    content_type[".md"] = "text/markdown";
    content_type[".mid"] = "audio/midi";
    content_type[".midi"] = "audio/midi";
    content_type[".mjs"] = "text/javascript";
    content_type[".mp3"] = "audio/mpeg";
    content_type[".mp4"] = "video/mp4";
    content_type[".mkv"] = "video/x-matroska";
    content_type[".mpeg"] = "video/mpeg";
    content_type[".mpkg"] = "application/vnd.apple.installer+xml";
    content_type[".odp"] = "application/vnd.oasis.opendocument.presentation";
    content_type[".ods"] = "application/vnd.oasis.opendocument.spreadsheet";
    content_type[".odt"] = "application/vnd.oasis.opendocument.text";
    content_type[".oga"] = "audio/ogg";
    content_type[".ogv"] = "video/ogg";
    content_type[".ogx"] = "application/ogg";
    content_type[".opus"] = "audio/ogg";
    content_type[".otf"] = "font/otf";
    content_type[".png"] = "image/png";
    content_type[".pdf"] = "application/pdf";
    content_type[".php"] = "application/x-httpd-php";
    content_type[".ppt"] = "application/vnd.ms-powerpoint";
    content_type[".pptx"] = "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    content_type[".rar"] = "application/vnd.rar";
    content_type[".rtf"] = "application/rtf";
    content_type[".sh"] = "application/x-sh";
    content_type[".svg"] = "image/svg+xml";
    content_type[".tar"] = "application/x-tar";
    content_type[".tif"] = "image/tiff";
    content_type[".tiff"] = "image/tiff";
    content_type[".ts"] = "video/mp2t";
    content_type[".ttf"] = "font/ttf";
    content_type[".txt"] = "text/plain";
    content_type[".vsd"] = "application/vnd.visio";
    content_type[".wav"] = "audio/wav";
    content_type[".weba"] = "audio/webm";
    content_type[".webm"] = "video/webm";
    content_type[".webmanifest"] = "application/manifest+json";
    content_type[".webp"] = "image/webp";
    content_type[".woff"] = "font/woff";
    content_type[".woff2"] = "font/woff2";
    content_type[".xhtml"] = "application/xhtml+xml";
    content_type[".xls"] = "application/vnd.ms-excel";
    content_type[".xlsx"] = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    content_type[".xml"] = "application/xml";
    content_type[".xul"] = "application/vnd.mozilla.xul+xml";
    content_type[".zip"] = "application/zip";
    content_type[".3gp"] = "video/3gpp";
    content_type[".3g2"] = "video/3gpp2";
    content_type[".7z"] = "application/x-7z-compressed";
}

bool Webserv::isCodeInMap(int code)
{
    std::map<int, std::string>::iterator found = status_codes.find(code);
    if (found == status_codes.end())
        return false;
    return true;
}

bool Webserv::isValidStatusCode(const std::string code)
{
    for (size_t i = 0; i < code.length(); i++)
    {
        if (!isdigit(code[i]))
            return false;
    }
    return true;
}

void Webserv::print_conf(void) {
        std::cout << "ERROR PAGES\n";
    for (std::map<std::string, std::string>::iterator it = error_pages.begin(); it != error_pages.end(); it++)
        std::cout << "Error_page: " << it->first << " -> " << it->second << std::endl;
    
    std::cout << "MAX_CLIENT_BODY_SIZE: " << client_max_body_size << std::endl;
    for (size_t i = 0; i < servers.size(); i++)
    {
        std::cout << "\nSERVER[" << i << "]\n";
        std::cout << "Listen: " << servers[i].ip_address <<  ":" << servers[i].port << std::endl;
        std::cout << "Server Root: " << servers[i].root << std::endl;
        for (size_t j = 0; j < servers[i].locations.size(); j++)
        {
            std::cout << "  \nLOCATION[" << j << "]\n";
            std::cout << "    Path: " << j << " " << servers[i].locations[j].path << std::endl;
            std::cout << "    Root: " << j << " " << servers[i].locations[j].root << std::endl;
            for (size_t k = 0; k < servers[i].locations[j].index.size(); k++)
            {
                std::cout << "    Index: " << j << " " << servers[i].locations[j].index[k] << std::endl;
            }
            for (size_t k = 0; k < servers[i].locations[j].methods.size(); k++)
            {
                std::cout << "    Methods: " << j << " " << servers[i].locations[j].methods[k] << std::endl;
            }
            std::cout << "    Autoindex: " << j << " " << (servers[i].locations[j].autoindex ? "True" : "False") << std::endl;
            std::cout << "    Redirection: " << j << " " << (servers[i].locations[j].isRedirection ? "True" : "False") << std::endl;
            if (servers[i].locations[j].isRedirection)
            {
                std::cout << "    is Text: " << (servers[i].locations[j].redirectionIsText ? "True" : "False") << std::endl;
                std::cout << "    Code & URL/TEXT: " << servers[i].locations[j].redirection.first << " -> " << "|" << servers[i].locations[j].redirection.second << "|" << std::endl;
            }
            std::cout << "    Upload: " << servers[i].locations[j].upload_dir << std::endl;
            std::cout << "    Cgi: " << (servers[i].locations[j].hasCgi ? "on" : "off") << std::endl;
        }
        std::cout << "-------------------------------------------------" << std::endl;
    }
}

bool Webserv::htmlPage(std::string path)
{
    size_t pos = path.find_last_of("/");
    std::string file = path.substr(pos + 1);
    if (!checkFileExtension(file, ".html") && !checkFileExtension(file, ".htm"))
        return false;
    return true;
}

void Webserv::read_file(void)
{
    this->config_file.clear();
    this->config_file.seekg(0, std::ios::beg);
    std::string line;
    while (std::getline(this->config_file, line))
    {
        std::string *parts = this->split(line);
        
        for (size_t i = 0; i < this->countParts(line); i++)
            this->tokens.push_back(parts[i]);
        delete[] parts;
    }
    tokens = semicolonBracketsFix(tokens);
    if (!checkForBrackets())
        throw std::runtime_error("Error: Unclosed brackets are inside the config file.");
    if (!checkSemicolon())
        throw std::runtime_error("Error: Semicolon not in appropriate place.");
    bool sawServer = false;
    for (size_t i = 0; i < tokens.size(); i++)
    {
        bool has_body_size = false;
        while (i < tokens.size() && tokens[i] != "}" && tokens[i] != "server")
        {
            if (tokens[i] == "error_page")
            {
                i++;
                if (tokens[i] == ";")
                    throw std::runtime_error("Error: No error pages provided.");
                std::vector<std::string> error_codes;
                std::string error_path;
                while (i < tokens.size() && tokens[i] != ";" && tokens[i] != "}" && tokens[i] != "server")
                {
                    if (isValidStatusCode(tokens[i]))
                    {
                        size_t code = atoll(tokens[i].c_str());
                        if (code < 400 || code > 599)
                            throw std::runtime_error("Error: Status code " + tokens[i] + " must be between 400 and 599");
                        if (!isCodeInMap(code))
                            throw std::runtime_error("Error: Not a valid http status code.");
                        error_codes.push_back(tokens[i]);
                        i++;
                    }
                    else
                    {
                        error_path = tokens[i];
                        if (!checkRoot(error_path))
                            throw std::runtime_error("Error: " + error_path + " not a valid path for error_page.");
                        if (!htmlPage(error_path))
                            throw std::runtime_error("Error: " + error_path + " not a valid html page for error_page.");
                        i++;
                        if (tokens[i] != ";")
                            throw std::runtime_error("Error: Expected ';' after error_page.");
                        break;
                    }
                }
                if (error_codes.empty())
                    throw std::runtime_error("Error: No error codes provided for error_page.");
                if (error_path.empty())
                    throw std::runtime_error("Error: No path provided for error_page.");
                for (size_t j = 0; j < error_codes.size() && error_pages[error_codes[j]].empty(); j++)
                    error_pages[error_codes[j]] = error_path;
                if (i < tokens.size() && tokens[i] == ";")
                    i++;
                else if (i < tokens.size() && tokens[i] != "}" && tokens[i] != "server")
                    throw std::runtime_error("Error: Expected ';' after error_page.");
            }
            else if (tokens[i] == "client_max_body_size")
            {
                if (has_body_size)
                    throw std::runtime_error("Error: Duplicate client_max_body_size directive.");
                i++;
                if (tokens[i] == ";")
                    throw std::runtime_error("Error: Empty client_max_body_size.");
                std::string body_size_value = tokens[i];
                i++;
                if (tokens[i] != ";")
                    throw std::runtime_error("Error: Expected ';' after client_max_body_size.");
                if (!checkMaxBodySize(body_size_value) || atoll(body_size_value.c_str()) <= 0)
                    throw std::runtime_error("Error: Invalid value for client_max_body_size.");
                client_max_body_size = atoll(body_size_value.c_str());
                i++;
                has_body_size = true;
            }
            else
                throw std::runtime_error("Error: Unexpected token: " + tokens[i]);
        }
        if (!has_body_size)
            client_max_body_size = 1024;
        while (i < tokens.size() && tokens[i] != "}")
        {
            if (tokens[i] == "server")
            {
                sawServer = true;
                i++;
                if (i >= tokens.size() || tokens[i] != "{")
                    throw std::runtime_error("Error: Expected '{' after server.");
                i++;
                Server s = parseServer(i);
                this->servers.push_back(s);
            }
            else if (tokens[i] == ";")
                i++;
            else
                throw std::runtime_error("Error: Server block not found or unexpected token: " + tokens[i]);
        }
        if (i < tokens.size() && tokens[i] == "}")
            i++;
    }
    if (!sawServer)
        throw std::runtime_error("Error: Server not found.");
    if (checkDuplicatePorts())
        throw std::runtime_error("Error: Multiple servers listen to same ports.");
    if (checkDuplicatePaths())
        throw std::runtime_error("Error: Server has the same location path multiple times.");
    // print_conf();
}


bool Webserv::checkMaxBodySize(const std::string value)
{
    if (value.empty())
        return false;
    for (size_t i = 0; i < value.size(); i++)
    {
        if (!isdigit(value[i]))
            return false;
    }
    return true;
}

bool Webserv::checkDuplicatePorts(void) const
{
    std::set<std::pair<std::string, int> > seen;
    for (size_t i = 0; i < servers.size(); i++)
    {
        std::pair<std::string, int> pair(servers[i].ip_address, servers[i].port);
        if (seen.find(pair) != seen.end())
            return true;
        seen.insert(pair);
    }
    return false;
}


bool Webserv::checkDuplicatePaths(void)
{
    std::set<std::string> Set;
    for (size_t i = 0; i < servers.size(); i++) 
    {
        Set.clear();
        for (size_t j = 0; j < servers[i].locations.size(); j++)
            Set.insert(servers[i].locations[j].path);
        if (Set.size() != servers[i].locations.size())
            return true;
    }
    return false;
}

bool Webserv::checkForBrackets(void)
{
    for (size_t i = 0; i < tokens.size(); i++)
    {
        std::string token = tokens[i];
        if (token == "{")
            brackets.push(token);
        else if (token == "}")
        {
            if (brackets.empty())
                return false;
            brackets.pop();
        }
    }
    return brackets.empty();
}

bool Webserv::checkSemicolon(void) const
{
    for (size_t i = 1; i < tokens.size(); i++)
    {
        if (tokens[i] == ";" && tokens[i - 1] == ";")
            return false;
    }
    return true;
}


std::string Webserv::getAddress(sockaddr_in *addr, addrinfo *result, bool should_free)
{
    std::stringstream ip_string;
    unsigned int ip = ntohl(addr->sin_addr.s_addr);

    char *parts = reinterpret_cast<char *>(&ip);
    unsigned char forth = parts[0];
    unsigned char third = parts[1];
    unsigned char second = parts[2];
    unsigned char first = parts[3];
    ip_string << static_cast<int>(first) << "." << static_cast<int>(second) << "." << static_cast<int>(third) << "." << static_cast<int>(forth);
    if (should_free)
        freeaddrinfo(result);
    return ip_string.str();
}

std::string Webserv::convertHostToIp(const std::string host, const std::string message)
{
    addrinfo hints;
    addrinfo *result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host.c_str(), NULL, &hints, &result);
    if (status != 0 || result == NULL)
        throw std::runtime_error(message);
    sockaddr_in *address = reinterpret_cast<sockaddr_in *>(result->ai_addr);
    return getAddress(address, result, true);
}

bool Webserv::checkHost(const std::string host, bool &isHost) const
{
    isHost = true;
    if (host.empty() || host.length() > 253)
        return false;
    int part_length = 0;
    for (size_t i = 0; i < host.length(); i++)
    {
        char c = host[i];
        if (c == '.')
        {
            if (part_length == 0 || part_length > 63)
                return false;
            part_length = 0;
            continue;
        }
        if (!(std::isalnum(c) || c == '-'))
            return false;
        if ((part_length == 0 && c == '-') || (i + 1 < host.length() && host[i + 1] == '.' && c == '-'))
            return false;
        part_length++;
    }
    if (part_length == 0 || part_length > 63)
        return false;
    return true;
}

int Webserv::isValidIp(std::string ip) const
{
    size_t counter = 0;

    for (size_t i = 0; i < ip.length(); i++)
    {
        if (!isdigit(ip[i]) && ip[i] != '.')
            return -1;
    }
    if (std::count(ip.begin(), ip.end(), '.') != 3)
        return -2;
    size_t pos = ip.find('.');
    while (pos != std::string::npos)
    {
        std::string part = ip.substr(0, pos);
        if (part.length() && (std::atoll(part.c_str()) >= 0 && std::atoll(part.c_str()) <= 255))
            counter++;
        ip = ip.substr(pos + 1);
        pos = ip.find('.');
    }
    if (ip.length() && (std::atoll(ip.c_str()) >= 0 && std::atoll(ip.c_str()) <= 255))
        counter++;
    if (counter == 4)
        return 1;
    return -2;
}

bool Webserv::checkValidListen(const std::string s, bool &isHost) const
{
    size_t len = s.length();

    if (!len)
        return false;
    size_t idx = s.find(':');
    if (idx == std::string::npos)
        return false;
    int count = std::count(s.begin(), s.end(), ':');
    if (count != 1)
        return false;
    std::string ip = s.substr(0, idx);
    std::string p = s.substr(idx + 1);
    if (p.empty())
        return false;
    for (size_t i = 0; i < p.length(); i++)
        if (!isdigit(p[i]))
            return false;
    long port = atol(p.c_str());
    int res = isValidIp(ip);
    if (res == -1 && !checkHost(ip, isHost)) // -1 not a valid ip address
        return false;
    if (res == -2)
        return false;
    if (port < 0 || port > 65535)
        return false;
    return true;
}

bool Webserv::checkStatusCode(const std::string code) const
{
    for (size_t i = 0; i < code.length(); i++)
        if (!isdigit(code[i]))
            return false;
    long status_code = atol(code.c_str());
    if (status_code < 200 || status_code > 599)
        return false;
    return true;
}

bool Webserv::isstrdigit(std::string str) const
{
    for (size_t i = 0; i < str.length(); i++)
    {
        if (!isdigit(str[i]))
            return false;
    }
    return true;
}

bool Webserv::checkUrlText(size_t i, Location &location, bool code_present, int status_code)
{
    bool isHttps = false;
    if (tokens[i].find('"') != std::string::npos)
    {
        if (!code_present)
            throw std::runtime_error("Error: Code before return text missing.");
        if (status_code / 100 == 3)
            throw std::runtime_error("Error: Code for return text incorrect.");
        location.redirectionIsText = true;
        if (tokens[i][0] != '"')
            return false;
        if (std::count(tokens[i].begin(), tokens[i].end(), '"') == 2)
        {
            if (tokens[i][0] != '"' || tokens[i][tokens[i].length() - 1] != '"')
                return false;
            return true;
        }
        i++;
        while (i < tokens.size() && tokens[i] != ";")
        {
            if (tokens[i].find('"') != std::string::npos)
                return true;
            i++;
        }
        return false;
    }
    else if (tokens[i][0] == '/')
    {
        size_t pos = tokens[i].find("//");
        if (pos != std::string::npos)
            throw std::runtime_error("Error: Return path not valid.");
        if (status_code != 301 && status_code != 302)
            throw std::runtime_error("Error: Code for return path incorrect.");
        location.redirect_relative = true;
    }
    else if ((tokens[i][0] == 'h' && tokens[i][1] == 't' && tokens[i][2] == 't' && tokens[i][3] == 'p' && tokens[i][4] == ':'
        && tokens[i][5] == '/' && tokens[i][6] == '/') || (tokens[i][0] == 'h' && tokens[i][1] == 't' && tokens[i][2] == 't' && tokens[i][3] == 'p' && tokens[i][4] == 's'
        && tokens[i][5] == ':' && tokens[i][6] == '/' && tokens[i][7] == '/'))
    {
        if ((tokens[i][0] == 'h' && tokens[i][1] == 't' && tokens[i][2] == 't' && tokens[i][3] == 'p' && tokens[i][4] == 's' && tokens[i][5] == ':' && tokens[i][6] == '/' && tokens[i][7] == '/'))
            isHttps = true;
        if (status_code != 301 && status_code != 302)
            throw std::runtime_error("Error: Code for return path incorrect.");
        std::string domain = tokens[i].substr(7);
        if (isHttps)
            domain = tokens[i].substr(8);
        size_t pos = domain.find('/');
        if (pos != std::string::npos)
        {
            if (domain[pos + 1] == '/')
                throw std::runtime_error("Error: Return path incorrect.");
            domain = domain.substr(0, pos);
        }
        convertHostToIp(domain, "Error: Invalid domain name in return directive.");
        location.redirect_absolute = true;
    }
    else
        return false;
    return true;
}

void Webserv::serverDefaultInit(Webserv::Server &server)
{
    server.port = -1;
    server.root = "";
}

void Webserv::locationDefaultInit(Location &location)
{
    location.autoindex = false;
    location.path = "";
    location.root = "";
    location.isRedirection = false;
    location.redirectionIsText = false;
    location.upload_dir = "/uploads";
    location.hasCgi = false;
    location.redirect_absolute = false;
    location.redirect_relative = false;
}

bool Webserv::checkPath(const std::string path) const
{
    if (path.empty() || path[0] != '/')
        return false;
    for (size_t i = 0; i < path.length(); i++)
    {
        if (!isalnum(path[i]) && path[i] != '/' && path[i] != '_' && path[i] != '-' && path[i] != '.')
            return false;
    }
    if (path.find("//") != std::string::npos)
        return false;
    if (path.find("..") != std::string::npos || path.find(".") != std::string::npos)
        return false;
    if (path.size() > 1 && path[path.size() - 1] == '/' && path[path.size() - 2] == '/')
        return false;
    return true;
}

bool Webserv::checkRoot(const std::string path) const
{
    if (path.empty() || path[0] != '/')
        return false;
    if (path.find("..") != std::string::npos)
        return false;
    if (path.find("//") != std::string::npos)
        return false;
    return true;
}

Webserv::Server Webserv::parseServer(size_t &i)
{
    Server server;
    serverDefaultInit(server);
    bool sawListen = false;
    bool sawLocation = false;
    int depth = 1;
    bool isHost = false;
    bool sawRoot = false;
    for (; i < tokens.size(); i++)
    {
        if (tokens[i] == "listen")
        {
            if (depth != 1)
                throw std::runtime_error("Error: Listen is not inside server block.");
            if (sawListen)
                throw std::runtime_error("Error: Duplicate listen directive.");
            sawListen = true;
            i++;
            if (tokens[i + 1] != ";")
                throw std::runtime_error("Error: Expected ';' after interface:port.");
            std::string s = tokens[i];
            if (!checkValidListen(s, isHost))
                    throw std::runtime_error("Error: Listening address is invalid.");
            server.port = atoi(s.substr(s.find(':') + 1).c_str());
            if (isHost)
                server.ip_address = convertHostToIp(s.substr(0, s.find(':')), "Error: Invalid listening host.");
            else
                server.ip_address = s.substr(0, s.find(':'));
            i++;
            continue;
        }
        else
        {
            if (!sawListen)
                throw std::runtime_error("Error: Expected listen directive at top of server block.");
        }
        if (tokens[i] == "root")
        {
            if (depth != 1)
                throw std::runtime_error("Error: root not in appropriate place.");
            if (sawRoot)
                throw std::runtime_error("Error: Duplicate root directive.");
            sawRoot = true;
            i++;
            if (tokens[i] == ";")
                throw std::runtime_error("Error: Expected a path after root directive.");
            if (tokens[i + 1] != ";")
                throw std::runtime_error("Error: Expected ';' after root.");
            if (!checkPath(tokens[i]))
                throw std::runtime_error("Error: Invalid path for root directive.");
            server.root = tokens[i];
            i++;
            continue;
        }
        if (tokens[i] != "listen" && tokens[i] != "server_name" && tokens[i] != "location" && tokens[i] != "root" && tokens[i] != "}")
            throw std::runtime_error("Error: Unknown directive '" + tokens[i] + "'.");
        if (tokens[i] == "location")
            parseLocation(i, server, depth, sawLocation);
        else
        {
            if (!sawLocation)
                throw std::runtime_error("Error: Expected location block in server block.");
        }
        if (i + 1 < tokens.size() && tokens[i + 1] == "server" && depth != 1)
        {
            i++;
            break;
        }
    }
    sortLocationPaths(server);
    return server;
}

bool Webserv::locationCmp(const Webserv::Location &l1, const Webserv::Location &l2)
{
    return l1.path.size() > l2.path.size();
}

void Webserv::sortLocationPaths(Webserv::Server &server) 
{
    std::sort(server.locations.begin(), server.locations.end(), locationCmp);
}

void Webserv::parseLocation(size_t &i, Webserv::Server &server, int &depth, bool &sawLocation)
{
    if (depth != 1)
        throw std::runtime_error("Error: Location is not inside server block.");
    sawLocation = true;
    bool sawRoot = false;
    bool sawIndex = false;
    bool sawMethods = false;
    bool sawAutoIndex = false;
    bool sawRedirection = false;
    bool sawUpload = false;
    bool sawCgi = false;
    i++;
    Location location;
    locationDefaultInit(location);
    if (tokens[i] == "{")
        throw std::runtime_error("Error: Expected path for location block.");
    if (!checkPath(tokens[i]))
        throw std::runtime_error("Error: Invalid path for location block.");
    location.path = tokens[i];
    i++;
    if (tokens[i] != "{")
        throw std::runtime_error("Error: Expected '{' after path.");
    depth++;
    i++;
    for (; i < tokens.size() && tokens[i] != "}"; i++)
    {
        if (tokens[i] == "root")
        {
            if (sawRoot)
                throw std::runtime_error("Error: Duplicate root directive.");
            sawRoot = true;
            i++;
            if (tokens[i] == ";")
                throw std::runtime_error("Error: Expected a path after root directive.");
            if (tokens[i + 1] != ";")
                throw std::runtime_error("Error: Expected ';' after root.");
            if (!checkRoot(tokens[i]))
                throw std::runtime_error("Error: Invalid path for root directive.");
            location.root = tokens[i];
        }
        else if (tokens[i] == "index")
        {
            if (sawIndex)
                throw std::runtime_error("Error: Duplicate index directive.");
            sawIndex = true;
            i++;
            if (tokens[i] == ";")
                throw std::runtime_error("Error: Expected files at index directive.");
            while (i < tokens.size() && tokens[i] != ";")
            {
                if (tokens[i] == "root" || tokens[i] == "allow_methods" || tokens[i] == "index" || tokens[i] == "autoindex" || tokens[i] == "return" || tokens[i] == "cgi" || tokens[i] == "upload_dir" || tokens[i] == "}")
                    throw std::runtime_error("Error: Expected ';' after index.");
                location.index.push_back(tokens[i]);
                i++;
            }
        }
        else if (tokens[i] == "allow_methods")
        {
            if (sawMethods)
                throw std::runtime_error("Error: Duplicate allow_methods directive.");
            sawMethods = true;
            i++;
            if (tokens[i] == ";")
                throw std::runtime_error("Error: Expected http methods after allow methods.");
            while (i < tokens.size() && tokens[i] != ";")
            {
                if (tokens[i] != "GET" && tokens[i] != "POST" && tokens[i] != "DELETE")
                    throw std::runtime_error("Error: Invalid http method or Expected ';'.");
                std::vector<std::string>::iterator found = std::find(location.methods.begin(), location.methods.end(), tokens[i]);
                if (found == location.methods.end())
                    location.methods.push_back(tokens[i]);
                else
                    throw std::runtime_error("Error: " + tokens[i] + " is duplicated.");
                i++;
            }
        }
        else if (tokens[i] == "autoindex")
        {
            if (sawAutoIndex)
                throw std::runtime_error("Error: Duplicate autoindex directive.");
            sawAutoIndex = true;
            i++;
            if (tokens[i] != "on" && tokens[i] != "off")
                throw std::runtime_error("Error: Invalid autoindex.");
            if (tokens[i + 1] != ";")
                throw std::runtime_error("Error: Expected ';' after autoindex.");
            tokens[i] == "on" ? location.autoindex = true : location.autoindex = false;
        }
        else if (tokens[i] == "return")
        {
            int status_code = 302;
            bool code_present = false;
            if (sawRedirection)
                throw std::runtime_error("Error: Duplicate return directive.");
            sawRedirection = true;
            location.isRedirection = true;
            i++;
            if (tokens[i] == ";")
                throw std::runtime_error("Error: Expected CODE and URL/TEXT after return.");
            if (isstrdigit(tokens[i]))
            {
                if (!checkStatusCode(tokens[i]))
                    throw std::runtime_error("Error: Invalid status code.");
                status_code = atoi(tokens[i].c_str());
                if (!isCodeInMap(status_code))
                    throw std::runtime_error("Error: Not a valid status code.");
                code_present = true;
                i++;
                if (tokens[i] == ";")
                    throw std::runtime_error("Error: Expected URL/TEXT after status code.");
            }
            if (!checkUrlText(i, location, code_present, status_code))
                throw std::runtime_error("Error: Invalid TEXT/URL after status code.");
            if (location.redirectionIsText)
            {
                tokens[i].erase(0, 1);
                tokens[i].erase(tokens[i].length() - 1, 1);
            }
            location.redirection.first = status_code;
            location.redirection.second = tokens[i];
            i++;
            if (tokens[i] != ";")
                throw std::runtime_error("Error: Expected ';' at the end of return directive.");
        }
        else if (tokens[i] == "upload_dir")
        {
            if (sawUpload)
                throw std::runtime_error("Error: Duplicate upload directive.");
            sawUpload = true;
            i++;
            if (tokens[i] == ";")
                throw std::runtime_error("Error: Expected path for upload_dir.");
            if (!checkRoot(tokens[i]))
                throw std::runtime_error("Error: Invalid path for upload_dir.");
            location.upload_dir = tokens[i];
            i++;
            if (tokens[i] != ";")
                throw std::runtime_error("Error: Expected ';' after upload_dir.");
        }
        else if (tokens[i] == "cgi")
        {
            if (sawCgi)
                throw std::runtime_error("Error: Duplicate cgi directive.");
            sawCgi = true;
            i++;
            if (tokens[i] == ";")
                throw std::runtime_error("Error: Empty cgi field.");
            
            if (tokens[i] == "on")
                location.hasCgi = true;
            else if (tokens[i] != "off")
                throw std::runtime_error("Error: Invalid value for cgi directive. Expected 'on' or 'off'.");
            i++;
            if (tokens[i] != ";")
                throw std::runtime_error("Error: Expected ';' after cgi directive.");
        }
        else if (tokens[i] != "root" && tokens[i] != "index" && tokens[i] != "allow_methods" && tokens[i] != "autoindex" && tokens[i] != ";")
            throw std::runtime_error("Error: Invalid directive '" + tokens[i] + "' inside location block.");
    }
    if (server.root.empty() && !sawRoot && !sawRedirection)
        throw std::runtime_error("Error: Missing root.");
    else if (!server.root.empty() && !sawRoot)
        location.root = server.root;
    if (!sawIndex)
        location.index.push_back("index.html");
    if (!location.methods.size())
        location.methods.push_back("GET");
    if (tokens[i] == "}")
        depth--;
    if (tokens[i + 1] == "}")
        depth--;
    server.locations.push_back(location);
}