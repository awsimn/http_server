#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

int main()
{
    int my_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (my_socket < 0)
    {
        printf("Connected failed. Failed to create socket.");
        return 1;
    }

    struct sockaddr_in my_socket_add;
    socklen_t sockaddr_len = sizeof(my_socket_add);
    my_socket_add.sin_family = AF_INET;
    my_socket_add.sin_port = htons(8080);
    my_socket_add.sin_addr.s_addr = INADDR_ANY;

    int option = 1;
    if (setsockopt(my_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
    {
        perror("setsockopt");
        return -1;
    }

    if (bind(my_socket, (struct sockaddr *)&my_socket_add, sockaddr_len) < 0)
    {
        perror("bind");
        return 1;
    }

    int st = listen(my_socket, 2);
    if (st < 0)
    {
        perror("listen");
        return 1;
    }

    struct sockaddr_in client_addr;

    while (1)
    {
        socklen_t client_addr_size = sizeof(client_addr);
        int p = accept(my_socket, (struct sockaddr *)&client_addr, &client_addr_size);
        if (p < 0)
        {
            perror("accept");
            return 1;
        }
        printf("Connected established.\n");

        char *request_buffer = malloc(sizeof(char) * 4096);
        if (request_buffer == NULL)
        {
            printf("Request buffer memory allocation failed.\n");
            return 0;
        }

        ssize_t bytes = recv(p, request_buffer, 4095, 0);
        printf("Received %zd bytes\n", bytes);

        if (bytes > 0)
        {
            request_buffer[bytes] = '\0';
            printf("%s\n", request_buffer);
        }
        if (bytes < 0)
        {
            perror("recv");
            free(request_buffer);
            close(p);
            continue;
        }
        if (bytes == 0)
        {
            free(request_buffer);
            close(p);
            continue;
        }

        char *request_method = strtok(request_buffer, " ");
        char *request_path = strtok(NULL, " ");

        if (request_method == NULL || request_path == NULL)
        {
            printf("Bad request\n");
            free(request_buffer);
            close(p);
            continue;
        }

        if (request_path != NULL)
        {
            request_path++;
            printf("Method type: %s\n", request_method);
            printf("Path: %s\n", request_path);
        }

        char *file_text;
        int fs;

        struct dirent *dir_entry;
        DIR *dir = opendir(".");

        if (dir == NULL)
        {
            perror("opendir");
            return -1;
        }

        int found = 0;

        while ((dir_entry = readdir(dir)) != NULL)
        {
            if ((strcmp(dir_entry->d_name, request_path) == 0))
            {
                found = 1;
                break;
            }
        }

        closedir(dir);

        if (found == 0)
        {
            printf("File not found.\n");
            free(request_buffer);
            close(p);
            continue;
        }

        FILE *f = fopen(request_path, "r");
        if (f == NULL)
        {
            printf("File not found.\n");
            free(request_buffer);
            close(p);
            continue;
        }

        fseek(f, 0L, SEEK_END);
        fs = ftell(f);
        rewind(f);

        file_text = malloc(fs + 1);
        if (file_text == NULL)
        {
            perror("malloc");
            return -1;
        }

        if (file_text)
        {
            size_t bytes_read = fread(file_text, 1, fs, f);
            file_text[bytes_read] = '\0';
            fclose(f);

            if (bytes_read != fs)
            {
                perror("fread");
                free(file_text);
                free(request_buffer);
                close(p);
                continue;
            }
        }

        char *full_response = malloc(4096);
        if (full_response == NULL)
        {
            perror("malloc");
            return -1;
        }

        snprintf(full_response, 4096, "HTTP/1.1 200 OK\r\n"
                                      "Content-Length: %d\r\n"
                                      "\r\n",
                 fs);

        send(p, full_response, strlen(full_response), 0);
        send(p, file_text, fs, 0);
        free(file_text);
        free(request_buffer);
        free(full_response);
        close(p);
    }

    return 0;
}