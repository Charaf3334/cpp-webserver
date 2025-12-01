#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    unsigned char buf[sizeof(struct in6_addr)];
    int domain, s;
    char str[INET6_ADDRSTRLEN];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s {i4|i6|<num>} string\n", argv[0]);
        return (1);
    }

    domain = (strcmp(argv[1], "i4") == 0) ? AF_INET :
            (strcmp(argv[1], "i6") == 0) ? AF_INET6 : atoi(argv[1]);

    s = inet_pton(domain, argv[2], buf); // 1 success
    if (s <= 0) { // 0 invalid address matalan passiti i4 moraha ipv6 address
        if (s == 0)
            fprintf(stderr, "Not in presentation format\n");
        else // -1 error if domain ghalat
            perror("inet_pton");
        return (1);
    }

    int len = (domain == AF_INET) ? 4 : 16;
    printf("Presentation to network: ");
    for (int i = 0; i < len; i++)
        printf("%02X ", buf[i]);
    printf("\n");


    if (inet_ntop(domain, buf, str, INET6_ADDRSTRLEN) == NULL) {
        perror("inet_ntop");
        return (1);
    }

    printf("Network to presentation: %s\n", str);

    return (0);
}