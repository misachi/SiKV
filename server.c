#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "sikv.h"

#define PORT 8007

size_t strlen0(char *buf)
{
    size_t len = 0;
    while (*buf != '\0')
    {
        buf++;
        len++;
    }
    return len;
}

void free_input_buffer(char **input_buf)
{
    for (size_t i = 0; i < 3; i++)
    {
        free(input_buf[i]);
    }

    free(input_buf);
}

static void sigint_handler(int sig)
{
    if (write(STDERR_FILENO, "SIGINT\nCleaning up....\n", 23) == -1)
    {
        fprintf(stderr, "write error");
    }
    KV_destroy();
    exit(EXIT_FAILURE);
}

char **parse_input(char *str, size_t len)
{
    char **buf = malloc(sizeof(char *) * 3);
    if (buf == NULL)
    {
        perror("failed malloc");
        exit(EXIT_FAILURE);
    }
    memset(buf, 0, sizeof(char *) * 3);
    size_t i = 0;
    size_t off = 0;
    size_t j = 0;
    char *start = str;
    while (*str == ' ')
    {
        i++;
        str++;
    }

    while (i < len)
    {
        if (*str == ' ')
        {
            buf[j] = realloc(buf[j], (i - off) + 1);
            if (buf[j] == NULL)
            {
                free(buf);
                perror("failed realloc");
                exit(EXIT_FAILURE);
            }
            memcpy(buf[j], (char *)&start[off], i - off);
            buf[j][i - off] = '\0';
            off = i;
            j++;
        }
        else if (*str == '\n')
        {
            buf[j] = realloc(buf[j], (i - off) + 1);
            if (buf[j] == NULL)
            {
                free(buf);
                perror("failed realloc");
                exit(EXIT_FAILURE);
            }

            memcpy(buf[j], (char *)&start[off], i - off);
            buf[j][i - off] = '\0';
            off = i;
            j++;
        }

        i++;
        str++;
    }
    return buf;
}

void serve(int argc, char *argv[])
{
    if (argc < 3)
    {
        perror("hostname and port are required");
        exit(EXIT_FAILURE);
    }

    if (MIN_ENTRY_NUM && CHECK_POWER_OF_2(MIN_ENTRY_NUM) != 0)
    {
        fprintf(stderr, "ERROR: Hmap size must be a power of two\n");
        exit(EXIT_FAILURE);
    }

    char buf[BUFFSZ];
    int server_fd, client_fd;
    size_t nr_read;
    int enable = 1;
    socklen_t client_len;
    struct protoent *proto;
    struct sockaddr_in server_sock, client_sock;
    unsigned short server_port = strtol(argv[2], NULL, 10);

    proto = getprotobyname("tcp");
    if (proto == NULL)
    {
        perror("getprotobyname");
        exit(EXIT_FAILURE);
    }

    server_fd = socket(AF_INET, SOCK_STREAM, proto->p_proto);
    if (server_fd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    server_sock.sin_family = AF_INET;
    server_sock.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sock.sin_port = htons(server_port);

    if (bind(server_fd, (struct sockaddr *)&server_sock, sizeof(struct sockaddr_in)) == -1)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) == -1)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGINT, sigint_handler) == SIG_ERR)
    {
        exit(EXIT_FAILURE);
    }

    printf("SiKV InMemory Database Server\nListening for connections on port %d\n", PORT);
    struct hash_map *hmap = KV_init(MIN_ENTRY_NUM, KV_hash_function, KV_STRING);

    while (1)
    {
        client_len = sizeof(struct sockaddr_in);
        client_fd = accept(server_fd, (struct sockaddr *)&client_sock, &client_len);

        while ((nr_read = read(client_fd, buf, BUFFSZ)) > 0)
        {
            if (buf[nr_read - 1] == '\n')
            {
                char **input_buf = parse_input(buf, nr_read);
                char *ret = process_cmd(hmap, sizeof(input_buf), input_buf);

                if (ret == NULL)
                {
                    ret = "GET Not found\n";
                    printf("%s\n", ret);
                    if (write(client_fd, ret, strlen(ret)) == -1)
                    {
                        fprintf(stderr, "write error");
                    }
                }
                else if (ret == SUCCESS)
                {
                    ret = "Ok\n";
                    printf("%s\n", ret);
                    if (write(client_fd, ret, strlen(ret)) == -1)
                    {
                        fprintf(stderr, "write error");
                    }
                }
                else
                {
                    size_t n = strlen(ret);
                    char *temp = malloc(n + 1);
                    if (temp == NULL)
                    {
                        perror("malloc");
                    }

                    memcpy(temp, ret, n);
                    temp[n] = '\n';
                    if (write(client_fd, temp, n + 1) == -1)
                    {
                        fprintf(stderr, "write error");
                    }
                    free(temp);
                }

                free_input_buffer(input_buf);
            }
        }
        close(client_fd);
    }
    KV_destroy();
}
