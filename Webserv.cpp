#include "Webserv.hpp"

Webserv::Webserv()
{
    throw std::runtime_error("Error: Empty server object.");
}

Webserv::Webserv(const std::string config_file_path) : config_file(config_file_path.c_str())
{
    if (!config_file.is_open())
        throw std::runtime_error("Error: Problem occured while opening the file.");
    if (!checkFileExtension(config_file_path))
        throw std::runtime_error("Error: Unknown file extension, use '.conf' files.");
    if (this->isFileEmpty())
        throw std::runtime_error("Error: Config file is empty.");
}

Webserv::Webserv(const Webserv &theOtherObject)
{
    static_cast<void>(theOtherObject);
}

Webserv& Webserv::operator=(const Webserv &theOtherObject)
{
    if (this != &theOtherObject)
    {
        static_cast<void>(theOtherObject);
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

bool Webserv::checkFileExtension(const std::string path) const
{
    size_t dot = path.rfind('.');
    const std::string extension = ".conf";

    if (dot == std::string::npos || dot == 0)
        return false;
    std::string ext = path.substr(dot);
    return ext == extension;
}

size_t Webserv::countParts(const std::string line) const
{
    size_t _count = 0;
    bool in_exp = false;
    for (size_t i = 0; i < line.size(); i++)
    {
        if (isspace(line[i]))
            in_exp = false;
        else
        {
            if (!in_exp)
            {
                _count++;
                in_exp = true;
            }
        }
    }
    return _count;
}

std::string* Webserv::split(const std::string line)
{
    size_t count = this->countParts(line);
    std::string* parts = new std::string[count];
    size_t size = line.size();

    size_t idx = 0;
    size_t i = 0;
    while (i < size && idx < count)
    {
        while (i < size && isspace(line[i]))
            i++;
        if (i >= size)
            break;

        size_t j = i;
        while (j < size && !isspace(line[j])) 
            j++;
        parts[idx++] = line.substr(i, j - i);
        i = j;
    }
    return parts;
}

std::vector<std::string> Webserv::semicolonBracketsFix(const std::vector<std::string> input)
{
    std::vector<std::string> result;
    for (std::vector<std::string>::const_iterator it = input.begin(); it != input.end(); ++it) 
    {
        const std::string s = *it;
        std::string temp;

        for (std::string::size_type i = 0; i < s.size(); ++i) 
        {
            char c = s[i];

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
    for (size_t i = 0; i < tokens.size(); i++)
    {
        bool bodySizeFlag = false;
        if (tokens[i] == "http")
        {
            i++;
            if (tokens[i] != "{")
                throw std::runtime_error("Error: Expected '{' after http.");
            i++;
            if (tokens[i] != "error_pages")
                throw std::runtime_error("Error: Expected error_pages after http.");
            i++;
            if (tokens[i] == ";")
                throw std::runtime_error("Error: No error pages are provided.");
            while (i < tokens.size() && (tokens[i] != ";" && tokens[i] != "client_max_body_size" && tokens[i] != "server"))
            {
                if (!checkRoot(tokens[i]))
                    throw std::runtime_error("Error: " + tokens[i] + " not a valid path for error_pages.");
                error_pages.push_back(tokens[i++]);
            }
            i++;
            if (tokens[i] != "server" && tokens[i] != "client_max_body_size")
                throw std::runtime_error("Error: Expected ';' after error_pages.");
            if (tokens[i] == "client_max_body_size")
            {
                i++;
                if (tokens[i] == ";")
                    throw std::runtime_error("Error: Empty client_max_body_size.");
                if (tokens[i + 1] != ";")
                    throw std::runtime_error("Error: Expected ';' after client_max_body_size or many values provided.");
                if (!checkMaxBodySize(tokens[i]))
                    throw std::runtime_error("Error: Invalid value for client_max_body_size.");
                bodySizeFlag = true;
                client_max_body_size = tokens[i];
            }
            if (bodySizeFlag)
            {
                i++;
                if (tokens[i] != ";")
                    throw std::runtime_error("Error: Expected ';' after client_max_body_size.");
                i++;
            }
            while (i < tokens.size() && tokens[i] != "}")
            {
                if (tokens[i] == "server")
                {
                    i++;
                    if (tokens[i] != "{")
                        throw std::runtime_error("Error: Expected '{' after server.");
                    i++;
                    Server s = parseServer(i);
                    this->servers.push_back(s);
                }
                else
                    throw std::runtime_error("Error: Server block not found.");
            }
        }
        else
            throw std::runtime_error("Error: Http context not found.");
    }
    if (checkDuplicatePorts())
        throw std::runtime_error("Error: Multiple servers listen to same ports.");
    if (checkDuplicatePaths())
        throw std::runtime_error("Error: Server has the same location path multiple times.");

    // print_data
    for (size_t i = 0; i < error_pages.size(); i++)
        std::cout << "Error_pages: " << error_pages[i] << std::endl;
    std::cout << "MAX_CLIENT_BODY_SIZE: " << (client_max_body_size.empty() ? "Empty" : client_max_body_size) << std::endl;
    for (size_t i = 0; i < servers.size(); i++)
    {
        std::cout << "Listen: " << servers[i].ip_address <<  ":" << servers[i].port << std::endl;
        // std::cout << "Name: " <<  servers[i].name << std::endl;
        for (size_t j = 0; j < servers[i].locations.size(); j++)
        {
            std::cout << "Path: " << j << " " << servers[i].locations[j].path << std::endl;
            std::cout << "Root: " << j << " " << servers[i].locations[j].root << std::endl;
            for (size_t k = 0; k < servers[i].locations[j].index.size(); k++)
            {
                std::cout << "Index: " << j << " " << servers[i].locations[j].index[k] << std::endl;
            }
            for (size_t k = 0; k < servers[i].locations[j].methods.size(); k++)
            {
                std::cout << "Methods: " << j << " " << servers[i].locations[j].methods[k] << std::endl;
            }
            std::cout << "Autoindex: " << j << " " << (servers[i].locations[j].autoindex ? "True" : "False") << std::endl;
            std::cout << "Redirection: " << j << " " << (servers[i].locations[j].isRedirection ? "True" : "False") << std::endl;
            if (servers[i].locations[j].isRedirection)
            {
                std::map<int, std::string>::iterator it = servers[i].locations[j].redirection.begin();
                std::cout << "is Text: " << (servers[i].locations[j].redirectionIsText ? "True" : "False") << std::endl;
                std::cout << "Code & URL/TEXT: " << it->first << " -> " << it->second << std::endl;
            }
            std::cout << "Upload: " << servers[i].locations[j].upload_dir << std::endl;
        }
        std::cout << "-------------------------------------------------" << std::endl;
    }
}

bool Webserv::checkMaxBodySize(const std::string value)
{
    if (value.empty())
        return false;
    if (value[0] == 'M')
        return false;
    size_t idx = value.find('M');
    if (idx == std::string::npos || idx != value.size() - 1)
        return false;
    int count = std::count(value.begin(), value.end(), 'M');
    if (count != 1)
        return false;
    for (size_t i = 0; i < value.size() - 1; i++)
    {
        if (!isdigit(value[i]))
            return false;
    }
    return true;
}

bool Webserv::checkDuplicatePorts(void) const
{
    std::set< std::pair<std::string, int> > seen;
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

std::string Webserv::convertHostToIp(const std::string host)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host.c_str(), NULL, &hints, &res);
    if (status != 0 || res == NULL)
        throw std::runtime_error("Error: Listening address is invalid.");

    char ip_string[INET_ADDRSTRLEN];
    struct sockaddr_in *ipv4 = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
    inet_ntop(AF_INET, &(ipv4->sin_addr), ip_string, sizeof(ip_string));

    freeaddrinfo(res);
    return std::string(ip_string);
}

bool Webserv::checkHost(const std::string host, bool &isHost) const
{
    isHost = true;
    if (host.empty() || host.length() > 253)
        return false;
    int labelLength = 0;

    for (size_t i = 0; i < host.length(); i++)
    {
        char c = host[i];
        if (c == '.')
        {
            if (labelLength == 0 || labelLength > 63)
                return false;
            labelLength = 0;
            continue;
        }
        if (!(std::isalnum(c) || c == '-'))
            return false;
        if ((labelLength == 0 && c == '-') || (i + 1 < host.length() && host[i + 1] == '.' && c == '-'))
            return false;
        labelLength++;
    }
    if (labelLength == 0 || labelLength > 63)
        return false;
    return true;
}

bool Webserv::isValidIp(std::string ip) const
{
    struct in_addr ipv4;
    if (inet_pton(AF_INET, ip.c_str(), &ipv4) == 1)
        return true;
    return false;
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
    if (!isValidIp(ip) && !checkHost(ip, isHost))
        return false;
    if (port < 0 || port > 65535)
        return false;
    return true;
}

// bool Webserv::checkServerName(const std::string s) const
// {
//     size_t len = s.length();

//     if (!len)
//         return false;
//     if (s != "localhost")
//         return false;
//     return true;
// }

bool Webserv::checkStatusCode(const std::string code) const
{
    for (size_t i = 0; i < code.length(); i++)
        if (!isdigit(code[i]))
            return false;
    long status_code = atol(code.c_str());
    if (status_code < 100 || status_code > 599)
        return false;
    return true;
}

bool Webserv::checkUrlText(size_t i, Location &location) const
{
    if (tokens[i].find('"') != std::string::npos)
    {
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
    // this block where i will handle URL
    return true;
}

std::string Webserv::retrieveText(size_t &i)
{
    std::string result;
    bool started = false;

    while (i < tokens.size() && tokens[i] != ";")
    {
        std::string token = tokens[i];
        if (!started)
        {
            size_t start_pos = token.find('"');
            if (start_pos != std::string::npos)
            {
                started = true;
                token.erase(start_pos, 1);
                size_t end_pos = token.find('"');
                if (end_pos != std::string::npos)
                {
                    token.erase(end_pos, 1);
                    result += token;
                    i++;
                    break;
                }
                result += token;
            }
        }
        else
        {
            size_t end_pos = token.find('"');
            if (end_pos != std::string::npos)
            {
                result += " " + token.substr(0, end_pos);
                i++;
                break;
            }
            else
                result += " " + token;
        }
        i++;
    }
    return result;
}

void Webserv::serverDefaultInit(Webserv::Server &server)
{
    server.name = "";
    server.port = -1;
}

void Webserv::locationDefaultInit(Location &location)
{
    location.autoindex = false;
    location.path = "";
    location.root = "";
    location.isRedirection = false;
    location.redirectionIsText = false;
    location.upload_dir = "./uploads"; // default path
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
    if (path.size() > 1 && path[path.size() - 1] == '/' && path[path.size() - 2] == '/')
        return false;
    return true;
}

bool Webserv::checkRoot(const std::string path) const
{
    if (path.empty())
        return false;
    if (path[0] != '/' && !(path.size() > 1 && path[0] == '.' && path[1] == '/'))
        return false;
    return true;
}

Webserv::Server Webserv::parseServer(size_t &i)
{
    Server server;
    serverDefaultInit(server);
    bool sawListen = false;
    // bool sawServerName = false;
    bool sawLocation = false;
    int depth = 1;
    bool isHost = false;

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
                server.ip_address = convertHostToIp(s.substr(0, s.find(':')));
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
        // if (tokens[i] == "server_name")
        // {
        //     if (depth != 1)
        //         throw std::runtime_error("Error: Server_name is not inside server block.");
        //     if (sawServerName)
        //         throw std::runtime_error("Error: Duplicate server_name directive.");
        //     sawServerName = true;
        //     i++;
        //     if (tokens[i + 1] != ";")
        //         throw std::runtime_error("Error: Expected ';' after server name.");
        //     std::string s = tokens[i];
        //     if (!checkServerName(s))
        //         throw std::runtime_error("Error: Server_name should be localhost.");
        //     server.name = s;
        //     i++;
        //     continue;
        // }
        // else
        // {
        //     if (!sawServerName)
        //         throw std::runtime_error("Error: Expected server_name directive in server block.");
        // }
        if (tokens[i] != "listen" && tokens[i] != "server_name" && tokens[i] != "location" && tokens[i] != "}")
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
    return server;
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
    if (tokens[i] == "}" || tokens[i] == ";")
        throw std::runtime_error("Error: Location block cannot be empty.");

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
            if (!checkRoot(tokens[i])) // khsni mzl nchecki wach dak path 3ndi, hada ghy check syntax
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
                if (tokens[i] == "root" || tokens[i] == "allow_methods" || tokens[i] == "index" || tokens[i] == "autoindex" || tokens[i] == "}")
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
                location.methods.push_back(tokens[i]);
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
            if (sawRedirection)
                throw std::runtime_error("Error: Duplicate return directive.");
            sawRedirection = true;
            location.isRedirection = true;
            i++;
            if (tokens[i] == ";")
                throw std::runtime_error("Error: Expected CODE and URL/TEXT after return.");
            if (!checkStatusCode(tokens[i]))
                throw std::runtime_error("Error: Invalid status code.");
            int status_code = atoi(tokens[i].c_str());
            i++;
            if (tokens[i] == ";")
                throw std::runtime_error("Error: Expected URL/TEXT after status code.");
            if (!checkUrlText(i, location))
                throw std::runtime_error("Error: Invalid TEXT/URL after status code.");
            if (location.redirectionIsText)
                location.redirection[status_code] = retrieveText(i);
            else
            {
                location.redirection[status_code] = tokens[i]; // still need to parse URL
                i++;
            }
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
        else if (tokens[i] != "root" && tokens[i] != "index" && tokens[i] != "allow_methods" && tokens[i] != "autoindex" && tokens[i] != ";")
            throw std::runtime_error("Error: Invalid directive '" + tokens[i] + "' inside location block.");
    }
    if (!sawRoot && !sawRedirection)
        throw std::runtime_error("Error: Missing root inside location.");
    if (tokens[i] == "}")
        depth--;
    if (tokens[i + 1] == "}")
        depth--;
    server.locations.push_back(location);
}