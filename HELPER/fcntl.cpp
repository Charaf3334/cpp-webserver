#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

// int main() {
//     std::cout << "O_RDONLY     = " << O_RDONLY << std::endl;
//     std::cout << "O_NONBLOCK   = " << O_NONBLOCK << std::endl;

//     int fd = open("text.txt", O_RDONLY);
//     int current_fd_flags = fcntl(fd, F_GETFL, 0);
//     std::cout << "Before current_fd_flags: "<< current_fd_flags << std::hex << " 0x" << current_fd_flags << std::endl; // by default taykon 8000 | It also includes kernel-internal flag bits

//     if (current_fd_flags == -1)
//     {
//         std::cerr << "Error: getting current fd flags." << std::endl;
//         return false;
//     }
//     if (fcntl(fd, F_SETFL, current_fd_flags | O_NONBLOCK) == -1)
//     {
//         std::cerr << "Error: setting flags non-blocking mode." << std::endl;
//         return false;
//     }

//     current_fd_flags = fcntl(fd, F_GETFL, 0);
//     std::cout << "After current_fd_flags : " << std::dec << current_fd_flags << std::hex << " 0x" << current_fd_flags << std::endl;
//     return true;
// }


int main() {
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    // Case 1: nonblocking mode
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
    // Case 2: just comment the bit above and read gonna hang waiting for data

    char buffer[100];

    // No data
    ssize_t bytes = read(pipefd[0], buffer, sizeof(buffer)); //recv

    if (bytes == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { // eAgain Ewouldblock are the same in linux
            std::cout << "Nonblocking read: no data available\n";
        } else {
            std::cerr << "Read error: " << strerror(errno) << "\n";
        }
    } else {
        std::cout << "Read " << bytes << " bytes\n";
    }

    // Data available in pipe
    const char* msg = "Hello!";
    write(pipefd[1], msg, strlen(msg)); //send

    // Read data
    bytes = read(pipefd[0], buffer, sizeof(buffer));

    if (bytes >= 0) {
        buffer[bytes] = '\0';
        std::cout << "Read after write: \"" << buffer << "\"\n";
    }

    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}

// Setting a file descriptor to O_NONBLOCK makes all I/O syscalls on it nonblocking (read, write, accept, connect, send, recv, etc.). If an operation cannot complete immediately, it returns EAGAIN/EWOULDBLOCK instead of blocking the calling thread.
