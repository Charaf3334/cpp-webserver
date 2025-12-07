#include <iostream>
#include <vector>
#include "Webserv.hpp"

size_t countParts(const std::string line)
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

std::string* split(const std::string &line)
{
    size_t count = countParts(line);
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

std::vector<std::string> getheadersLines(const std::string req)
{
	std::string sub_req;
	int pos = req.find("\r\n\r\n");
	if (pos == std::string::npos)
		throw std::runtime_error("400 Bad Request: headers should end with CRLF");
	if (pos == 0)
		throw std::runtime_error("400 Bad Request: request empty");
	sub_req = req.substr(0, pos);
	std::vector<std::string> lines;
	int start = 0;
	for (int j = 0; j < sub_req.size(); j++)
	{
		if ((sub_req[j] == '\r' && sub_req[j + 1] == '\n') || j + 1 == sub_req.size())
		{
			lines.push_back(sub_req.substr(start, (j + 1 == sub_req.size()) ? j - start + 1 : j - start));
			j++;
			start = j + 1;
		}
		else if (!std::isprint(sub_req[j]))
			throw std::runtime_error("400 Bad Request: invalid character");
	}
	if (lines.size() == 1)
		throw std::runtime_error("400 bad Request: Headers missing");
	return lines;
}

bool parse_path(std::string &path){
	
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

bool check_allowedfirst(std::string &first)
{
	// std::string allowed_first = "!#$%&'*+-.^_`|~:";
	for(int i = 0; i < first.size(); i++)
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

std::string str_tolower(std::string str)
{
	std::string result = str;
	for (size_t i = 0; i < str.size(); i++)
		result[i] = std::tolower((unsigned char)str[i]);
	return result;
}

bool one_string_case(std::string &str, std::map<std::string, std::string> &map)
{
	if (str.empty()){
		std::cerr << "400 Bad Request: header empty" << std::endl;
		return false;
	}
	int pos = str.find(':');
	if (pos == std::string::npos){
		std::cerr << "400 Bad Request: character ':' missing in header" << std::endl;
		return false;
	}
	if (pos == str.size() - 1){
		std::cerr << "400 Bad Request: header value missing" << std::endl;
		return false;
	}
	if (str[pos + 1] == ':'){
		std::cerr << "400 Bad Request: character ':' invalid" << std::endl;
		return false;
	}
	std::string first = str.substr(0, pos + 1);
	first = str_tolower(first);
	if (first.size() == 1 || !check_allowedfirst(first)){
		std::cerr << "400 Bad Request: invalid header" << std::endl;
		return false;
	}
	std::string second = str.substr(pos + 1);
	map[first] = second;
	return true;
}

bool two_string_case(std::string *words, std::map<std::string, std::string>&map)
{
	size_t size = words[0].size();
	if (size == 1){
		std::cerr << "400 Bad Request: invalid header" << std::endl;
		return false;
	}
	int pos = words[0].find(':');
	if (pos == std::string::npos){
		std::cerr << "400 Bad Request: character ':' missing in header" << std::endl;
		return false;
	}
	if (pos != size - 1){
		std::cerr << "400 Bad Request: character ':' is not in the end of the header" << std::endl;
		return false;
	}
	if (!check_allowedfirst(words[0])){
		std::cerr << "400 Bad Request: invalid header" << std::endl;
		return false;
	}
	words[0] = str_tolower(words[0]);
	int tmp = words[1].find(':');
	if (tmp != std::string::npos && tmp == 0){
		std::cerr << "400 Bad Request: character ':' is invalid" << std::endl;
		return false;
	}
	map[words[0]] = words[1];
	return true;
}

bool parse_methode(std::string *words){
	std::string http_versions[] = {"HTTP/0.9", "HTTP/1.0", "HTTP/1.1", "HTTP/2.0", "HTTP/3.0"};
	std::string http_methodes[] = {"GET", "POST", "DELETE", "PATCH", "PUT", "HEAD", "OPTIONS"};
	for (int j = 0; j < 7; j++){
		if (words[0] == http_methodes[j])
			break;
		if (j == 6){
			std::cerr << "400 Bad Request: HTTP methode not correct" << std::endl;
			return false;
		}
	}
	if (words[0] != "GET" && words[0] != "POST" && words[0] != "DELETE"){
		std::cerr << "400 Bad Request: HTTP methode not allowed" << std::endl;
		return false;
	}
	if (!parse_path(words[1])){
		std::cerr << "400 Bad Request: bad HTTP methode path" << std::endl;
		return false;
	}
	for (int i = 0; i < 5; i++){
		if (words[2] == http_versions[i])
			break;
		if (i == 4){
			std::cerr << "400 Bad Request: HTTP protocole not correct" << std::endl;
			return false;
		}
	}
	if (words[2] != "HTTP/1.0" && words[2] != "HTTP/1.1"){
		std::cerr << "505 Bad Request: HTTP protocole version not supported" << std::endl;
		return false;
	}
	return true;

}

bool parse_lines(std::vector<std::string> &lines){
	std::string *words;

	for (int i = 0; i < lines.size(); i++)
	{
		int size = countParts(lines[i]);
		words = split(lines[i]);
//--------------------------------------METHOD--------------------------------------
		if (i == 0 && size != 3){
			std::cerr << "400 Bad Request: HTTP methode line incorrect" << std::endl;
			return false;
		}
		else if (i == 0){
			if (!parse_methode(words))
				return false;
		}
//--------------------------------------HEADERS--------------------------------------
		std::map<std::string, std::string>map;
		if (i > 0 && (size != 2 && size != 1)){
			std::cerr << "400 Bad Request: header line incorrect" << std::endl;
			return false;
		}
		else if (i > 0)
		{
			if (size == 1){
				if (!one_string_case(words[0], map))
					return false;
			}
			else{
				if (!two_string_case(words, map))
					return false;
			}
		}
	}
	return true;
}

int main(void){
	try
	{
		std::string request = "GET / HTTP/1.0\r\nheywhate:ver\r\n\r\n";
		std::vector<std::string> lines = getheadersLines(request);
		for (size_t i = 0; i < lines.size(); i++)
			std::cout << "lines[" << i << "]: " << lines[i] << std::endl;
		parse_lines(lines);
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}
}
