#include "Webserv.hpp"

Webserv::Webserv()
{
    throw std::runtime_error("Error: Empty server object.");
}

Webserv::Webserv(const std::string config_file_path) : config_file(config_file_path.c_str())
{
    if (!config_file.is_open())
        throw std::runtime_error("Error: Problem occured while opening the file.");
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
        if (tokens[i] == "http")
        {
            i++;
            if (tokens[i] != "{")
                throw std::runtime_error("Error: Expected '{' after http.");
            i++;
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

    // print_data
    for (size_t i = 0; i < servers.size(); i++)
    {
        std::cout << "Listen: " <<  servers[i].port << std::endl;
        std::cout << "Name: " <<  servers[i].name << std::endl;
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
        }
        std::cout << "-------------------------------------------------" << std::endl;
    }
}

bool Webserv::checkDuplicatePorts(void) const
{
    std::set<int> Set;
    for (size_t i = 0; i < servers.size(); i++) 
        Set.insert(servers[i].port);
    return Set.size() != servers.size();
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

bool Webserv::checkValidPort(const std::string s) const
{
    size_t len = s.length();

    if (!len)
        return false;
    for (size_t i = 0; i < len; i++)
        if (!isdigit(s[i]))
            return false;
    int p = atoi(s.c_str());
    if (p < 0 || p > 65535)
        return false;
    return true;
}

bool Webserv::checkServerName(const std::string s) const
{
    size_t len = s.length();

    if (!len)
        return false;
    if (s != "localhost")
        return false;
    return true;
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
    bool sawServerName = false;
    bool sawLocation = false;
    int depth = 1;

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
                throw std::runtime_error("Error: Expected ';' after port.");
            std::string s = tokens[i];
            if (!checkValidPort(s))
                throw std::runtime_error("Error: Port is invalid.");
            server.port = atoi(s.c_str());
            i++;
            continue;
        }
        else
        {
            if (!sawListen)
                throw std::runtime_error("Error: Expected listen directive at top of server block.");
        }
        if (tokens[i] == "server_name")
        {
            if (depth != 1)
                throw std::runtime_error("Error: Server_name is not inside server block.");
            if (sawServerName)
                throw std::runtime_error("Error: Duplicate server_name directive.");
            sawServerName = true;
            i++;
            if (tokens[i + 1] != ";")
            throw std::runtime_error("Error: Expected ';' after server name.");
            std::string s = tokens[i];
            if (!checkServerName(s))
                throw std::runtime_error("Error: Server_name should be localhost.");
            server.name = s;
            i++;
            continue;
        }
        else
        {
            if (!sawServerName)
                throw std::runtime_error("Error: Expected server_name directive in server block.");
        }
        if (tokens[i] != "listen" && tokens[i] != "server_name" && tokens[i] != "location" && tokens[i] != "}")
            throw std::runtime_error("Error: Unknown directive '" + tokens[i] + "'.");
        if (tokens[i] == "location")
            parseLocation(i, server, depth, sawLocation);
        else
        {
            if (!sawLocation)
                throw std::runtime_error("Error: Expected location block in server block.");
        }
        if (i + 1 < tokens.size() && tokens[i + 1] == "server")
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
                if (tokens[i] != "GET" && tokens[i] != "POST" && tokens[i] != "DELETE" && tokens[i] != "PUT")
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
        else if (tokens[i] != "root" && tokens[i] != "index" && tokens[i] != "allow_methods" && tokens[i] != "autoindex" && tokens[i] != ";")
            throw std::runtime_error("Error: Invalid directive '" + tokens[i] + "' inside location block.");
    }
    if (tokens[i] == "}")
        depth--;
    if (tokens[i + 1] == "}")
        depth--;
    server.locations.push_back(location);
}


// handle missing of required fields and errors to throw (ie. root and index and so on)
// also duplicates in location ex i have both location paths point to /, or both servers listen at same port.

// khsni mzl nchof subject lakant chi haja tzad f config file w ha7na salina hh