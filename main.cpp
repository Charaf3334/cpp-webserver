#include "Webserv.hpp"

int main(int ac, char **av)
{
    try
    {
        if (ac <= 2)
        {
            Webserv server(ac == 2 ? av[1] : "./default/default.conf");
            server.read_file();
        }
        else
        {
            std::cerr << "Error: Number of args is not valid." << std::endl;
            return 1;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}