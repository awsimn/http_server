#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>

void *thread_function(void *arg)
{
    int listen_socket = *(int *)arg;
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int p = accept(listen_socket, (struct sockaddr *)&client_addr, &client_addr_size);
        if (p < 0)
        {
            perror("accept");
            pthread_exit(NULL);
        }

        char *request_buffer = malloc(sizeof(char) * 4096);
        if (request_buffer == NULL)
        {
            perror("malloc");
            close(p);
            pthread_exit(NULL);
        }

        ssize_t bytes = recv(p, request_buffer, 4095, 0);
        // printf("Received %zd bytes\n", bytes);

        if (bytes > 0)
        {
            request_buffer[bytes] = '\0';
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
            char *fail_response = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Length: 0\r\n"
                                  "\r\n";
            send(p, fail_response, strlen(fail_response), 0);
            free(request_buffer);
            close(p);
            continue;
        }

        if (request_path != NULL)
        {
            request_path++;
        }

        char *file_text;
        int fs;

        struct dirent *dir_entry;
        DIR *dir = opendir(".");

        if (dir == NULL)
        {
            perror("opendir");
            close(p);
            pthread_exit(NULL);
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
            char *fail_response = "HTTP/1.1 404 File not found\r\n"
                                  "Content-Length: 0\r\n"
                                  "\r\n";
            send(p, fail_response, strlen(fail_response), 0);
            free(request_buffer);
            close(p);
            continue;
        }

        FILE *f = fopen(request_path, "r");
        if (f == NULL)
        {
            char *fail_response = "HTTP/1.1 404 File not found\r\n"
                                  "Content-Length: 0\r\n"
                                  "\r\n";
            send(p, fail_response, strlen(fail_response), 0);
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
            free(file_text);
            close(p);
            pthread_exit(NULL);
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
            free(file_text);
            free(request_buffer);
            close(p);
            pthread_exit(NULL);
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
    return NULL;
}

int main()
{
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in listen_socket_add;
    socklen_t sockaddr_len = sizeof(listen_socket_add);
    listen_socket_add.sin_family = AF_INET;
    listen_socket_add.sin_port = htons(8080);
    listen_socket_add.sin_addr.s_addr = INADDR_ANY;

    int option = 1;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
    {
        perror("setsockopt");
        close(listen_socket);
        return -1;
    }

    if (bind(listen_socket, (struct sockaddr *)&listen_socket_add, sockaddr_len) < 0)
    {
        perror("bind");
        close(listen_socket);
        return 1;
    }

    int st = listen(listen_socket, 128);
    if (st < 0)
    {
        perror("listen");
        close(listen_socket);
        return -1;
    }

    int max_threads = sysconf(_SC_NPROCESSORS_ONLN);

    pthread_t threads[max_threads];
    for (size_t i = 0; i < max_threads; i++)
    {
        int result = pthread_create(
            &threads[i],
            NULL,
            thread_function,
            &listen_socket);

        if (result != 0)
        {
            fprintf(stderr, "pthread_create failed: %s\n",
                    strerror(result));
        }
    }

    for (size_t i = 0; i < max_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    return 0;
}