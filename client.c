#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "sikv.h"

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        perror("hostname and port are required");
        exit(EXIT_FAILURE);
    }

    char buf[BUFFSZ];
    int client_fd;
    char *server_hostname = argv[1];
    ssize_t nr_read;
    size_t input_read;
    char *input_ptr = NULL;
    struct protoent *proto = {NULL};
    in_addr_t in_addr;
    unsigned short server_port = strtol(argv[2], NULL, 10);
    struct sockaddr_in addr_in;
    struct hostent *hostent = {NULL};

    proto = getprotobyname("tcp");
    if (proto == NULL)
    {
        perror("getprotobyname");
        exit(EXIT_FAILURE);
    }

    client_fd = socket(AF_INET, SOCK_STREAM, proto->p_proto);
    if (client_fd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    hostent = gethostbyname(server_hostname);
    if (hostent == NULL)
    {
        fprintf(stderr, "gethostbyname: %s: %s\n", server_hostname, strerror(errno));
        exit(EXIT_FAILURE);
    }

    in_addr = inet_addr(inet_ntoa(*(struct in_addr *)*(hostent->h_addr_list)));
    if (in_addr == (in_addr_t)-1)
    {
        fprintf(stderr, "inet_addr: %s: %s\n", *(hostent->h_addr_list), strerror(errno));
        exit(EXIT_FAILURE);
    }

    addr_in.sin_addr.s_addr = in_addr;
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(server_port);

    if (connect(client_fd, (struct sockaddr *)&addr_in, sizeof(addr_in)) == -1)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("SiKV InMemory Database Client\nReady to accept input\n");

    while (1)
    {
        printf(">> ");
        input_read = getline(&input_ptr, &input_read, stdin);
        if (input_read == -1)
        {
            perror("getline");
            break;
        }

        if (input_read == 1)
        {
            continue;
        }

        char quit[] = "quit";
        if (input_read == sizeof(quit) && memcmp(input_ptr, quit, input_read - 1) == 0)
        {
            fprintf(stderr, "quitting...");
            break;
        }

        char exit[] = "exit";
        if (input_read == sizeof(exit) && memcmp(input_ptr, exit, input_read - 1) == 0)
        {
            fprintf(stderr, "exiting...");
            break;
        }

        if (write(client_fd, input_ptr, input_read) == -1)
        {
            fprintf(stderr, "write error");
            break;
        }

        while ((nr_read = read(client_fd, buf, BUFFSZ)) > 0)
        {
            if (buf[nr_read - 1] == '\n')
            {
                buf[nr_read - 1] = '\0';
                printf("%s\n", buf);
                break;
            }
        }
    }
    close(client_fd);
    free(input_ptr);

    exit(EXIT_SUCCESS);
}