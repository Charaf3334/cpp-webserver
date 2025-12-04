#include <iostream>
#include <vector>

// std::vector<std::string> getLines(const std::string& req)
// {
//     std::vector<std::string> lines;
//     std::string curr;

//     for (size_t i = 0; i < req.size();)
//     {
//         if (req[i] == '\r')
//         {
//             if (i + 1 >= req.size() || req[i + 1] != '\n')
//                 throw std::runtime_error("400 Bad Request: malformed CRLF");
//             lines.push_back(curr);
//             curr.clear();
//             i += 2;

//             if (i < req.size() && req[i] == '\r')
//             {
//                 if (i + 1 < req.size() && req[i + 1] == '\n')
//                 {
//                     lines.push_back("");
//                     i += 2;
//                 }
//                 else
//                     throw std::runtime_error("400 Bad Request: malformed CRLF");
//             }
//         }
//         else if (req[i] == '\n')
//             throw std::runtime_error("400 Bad Request: LF without CR");
//         else
//         {
//             curr += req[i];
//             i++;
//         }
//     }
//     return lines;
// }

std::vector<std::string> getheadersLines(const std::string& req)
{
	std::string sub_req;
	int pos = req.find("\r\n\r\n");
	if (pos == std::string::npos)
		throw std::runtime_error("400 Bad Request: headers should end with CRLF");
	sub_req = req.substr(0, pos);
	// std::cout << "sub_req: |" << sub_req  << "|"<< std::endl;

	// std::string error_comb[] = {"\r\r", "\n\n", "\n\r"};
	// for (int i = 0; i < 3; i++)
	// {
	// 	if (sub_req.find(error_comb[i]) != std::string::npos)
	// 		throw std::runtime_error("400 Bad Request: incorrect CRLF");
	// }


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
	return lines;
}

// bool parse_lines

int main(void)
{
	try
	{
		std::string request = "GET /php_cgi/cat.png HTTP/1.1\r\nHOST: localhost\r\nlanguage:eng\r\nzbi\r\nzbi2\r\nzbi3\r\nzbi4\r\nzbi4\r\n\r\n";
		std::vector<std::string> lines = getheadersLines(request);
		for (int i = 0; i < lines.size(); i++)
		{
			std::cout << "|" << lines[i] << "|" << std::endl;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}
}
