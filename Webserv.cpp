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
    this->Data = theOtherObject.Data;
}

Webserv& Webserv::operator=(const Webserv &theOtherObject)
{
    if (this != &theOtherObject)
    {
        this->Data = theOtherObject.Data;
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
    // for (size_t i = 0; i < this->tokens.size(); i++)
    // {
    //     std::cout << this->tokens[i] << " ";
    //     if (i == this->tokens.size() - 1)
    //         std::cout << std::endl;
    // }

    bool found_server = false;
    int brackets_flag = 0;
    for (size_t i = 0; i < tokens.size(); i++)
    {
        if (tokens[i] == "server")
        {
            found_server = true;
            i++;
            if (tokens[i] != "{")
                throw std::runtime_error("Error: Expected '{' after server.");
            brackets_flag = 1;
            i++;
            Server s = parseServer(i, brackets_flag);
            this->servers.push_back(s);
        }
        else
        {
            if (!found_server)
                throw std::runtime_error("Error: 'server' keyword not found.");
        }
    }
}

Webserv::Server Webserv::parseServer(size_t i, int &brackets_flag)
{
    Server server;
    static_cast<void>(brackets_flag);
    while (i < this->tokens.size())
    {
        if (this->tokens[i] == "listen")
        {
            i++;
            if (this->tokens[i].find(';') == std::string::npos)
                throw std::runtime_error("Error: Semicolon is missing or it's not in its place.");
            server.port = atoi((this->tokens[i].substr(0, this->tokens[i].length() - 1)).c_str());
        }
        else if (this->tokens[i] == "server_name")
        {
            i++;
            for (; this->tokens[i].find(';') == std::string::npos; i++)
                server.names.push_back(this->tokens[i]);
            server.names.push_back(this->tokens[i].substr(0, this->tokens[i].length() - 1));
            for (size_t k = 0; k < server.names.size(); k++)
                std::cout << server.names[k] << std::endl;
        }
        else
            throw std::runtime_error("Error: Unknown token " + this->tokens[i]);
        i++;
    }

    return server;
}