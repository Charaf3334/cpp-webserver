#include "Webserv.hpp"
#include "Server.hpp"

Server* Server::instance = NULL;

int main(int ac, char **av)
{
    try
    {
        if (ac <= 2)
        {
            Webserv webserv(ac == 2 ? av[1] : "./default/default.conf");
            webserv.read_file();
            Server server(webserv);
            server.initialize();
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