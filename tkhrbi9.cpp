#include <iostream>
#include <vector>
#include "Webserv.hpp"

std::string str_tolower(std::string str)
{
    std::string result = str;
	for (size_t i = 0; i < str.size(); i++)
		result[i] = std::tolower((unsigned char)str[i]);
	return result;
}

bool check_allowedfirst(std::string &first)
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

bool one_string_case(std::string &line)
{
	std::map<std::string, std::string>map;

    if (line.empty())
    {
        std::cout << "ERROR\n";
		return false;
    }
	size_t pos = line.find(':');
	if (pos == std::string::npos)
    {
        std::cout << "ERROR\n";
		return false;
    }
	if (pos == line.size() - 1)
    {
        std::cout << "ERROR\n";
		return false;
    }
	if (line[pos + 1] == ':')
    {
        std::cout << "ERROR\n";
		return false;
    }
	std::string first = line.substr(0, pos + 1);
	first = str_tolower(first);
	if (first.size() == 1 || !check_allowedfirst(first))
    {
        std::cout << "ERROR\n";
		return false;
    }
    first = first.substr(0, pos);
    pos++;
    while (line[pos] && line[pos] == ' ')
        pos++;
	if (line[pos] == ':'){
        std::cout << "ERROR\n";
		return false;
	}
	std::string second = line.substr(pos);
	std::cout << first << std::endl;
	std::cout << second << std::endl;
	map[first] = second;
	return true;
}


int main(void){
	try
	{
		std::string line = "User-Agent:l";
		one_string_case(line);
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}
}
