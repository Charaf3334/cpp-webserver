#include "Webserv.hpp"

int main(int ac, char **av)
{
    try
    {
        if (ac != 2)
        {
            std::cerr << "Error: Number of args is not valid." << std::endl;
            return 1;
        }
        Webserv server(av[1]);
        server.read_file();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}