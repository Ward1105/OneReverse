#ifdef _WIN32
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

int total_bytes_sent = 0;

// Initialize Winsock library on Windows
void OSInit(void)
{
    WSADATA wsaData;
    int WSAError = WSAStartup(MAKEWORD(2, 0), &wsaData);
    if (WSAError != 0)
    {
        fprintf(stderr, "WSAStartup errno = %d\n", WSAError);
        exit(-1);
    }
}
#define perror(string) fprintf(stderr, string ": WSA errno = %d\n", WSAGetLastError())
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
// No initialization required for non-Windows systems
void OSInit(void) {}
void OSCleanup(void) {}
#endif

int initialization();
int connection(int internet_socket);
void execution(int internet_socket);
void cleanup(int internet_socket, int client_internet_socket);

int main(int argc, char *argv[])
{
    printf("Program Start\n");
    OSInit();
    int internet_socket = initialization();

    while (1) {
        int client_internet_socket = connection(internet_socket);
        execution(client_internet_socket);
    }

    return 0;
}

int initialization()
{
    // Step 1.1: Setup the address information
    struct addrinfo internet_address_setup;
    struct addrinfo *internet_address_result;
    memset(&internet_address_setup, 0, sizeof internet_address_setup);
    internet_address_setup.ai_family = AF_UNSPEC;
    internet_address_setup.ai_socktype = SOCK_STREAM;
    internet_address_setup.ai_flags = AI_PASSIVE;
    int getaddrinfo_return = getaddrinfo(NULL, "22", &internet_address_setup, &internet_address_result);
    if (getaddrinfo_return != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_return));
        exit(1);
    }

    int internet_socket = -1;
    struct addrinfo *internet_address_result_iterator = internet_address_result;
    while (internet_address_result_iterator != NULL)
    {
        // Step 1.2: Create a socket
        internet_socket = socket(internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol);
        if (internet_socket == -1)
        {
            perror("socket");
        }
        else
        {
            // Step 1.3: Bind the socket to the address
            int bind_return = bind(internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen);
            if (bind_return == -1)
            {
                perror("bind");
                close(internet_socket);
            }
            else
            {
                // Step 1.4: Listen for incoming connections
                int listen_return = listen(internet_socket, SOMAXCONN); // Use SOMAXCONN for a system-defined maximum backlog value
                if (listen_return == -1)
                {
                    close(internet_socket);
                    perror("listen");
                }
                else
                {
                    break;
                }
            }
        }
        internet_address_result_iterator = internet_address_result_iterator->ai_next;
    }

    freeaddrinfo(internet_address_result);

    if (internet_socket == -1)
    {
        fprintf(stderr, "socket: no valid socket address found\n");
        exit(2);
    }

    return internet_socket;
}

char ip_address[INET6_ADDRSTRLEN];

int connection(int internet_socket)
{
    printf("----Waiting for connection-------\n");
    struct sockaddr_storage client_internet_address;
    socklen_t client_internet_address_length = sizeof(client_internet_address);
    int client_socket = accept(internet_socket, (struct sockaddr *)&client_internet_address, &client_internet_address_length);
    if (client_socket == -1)
    {
        perror("accept");
        close(internet_socket);
        exit(3);
    }

    void *addr;
    if (client_internet_address.ss_family == AF_INET)
    {
        struct sockaddr_in *s = (struct sockaddr_in *)&client_internet_address;
        addr = &(s->sin_addr);
    }
    else
    {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_internet_address;
        addr = &(s->sin6_addr);
    }

    inet_ntop(client_internet_address.ss_family, addr, ip_address, sizeof(ip_address));

    // Log the IP address of the client
    FILE *log_file = fopen("log.txt", "a");
    if (log_file == NULL)
    {
        perror("fopen");
        close(client_socket);
        exit(4);
    }
    fprintf(log_file, "------------------------\n");
    fprintf(log_file, "Connection from %s\n", ip_address);
    fprintf(log_file, "------------------------\n");
    fclose(log_file);

    return client_socket;
}

void http_get()
{
    int sockfd;
    struct sockaddr_in server_addr;
    char request[256];
    char response[1024];

    // Create a new socket for the HTTP request
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        return;
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    server_addr.sin_addr.s_addr = inet_addr("208.95.112.1");

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("connect");
        return;
    }

    // Prepare the HTTP GET request
    snprintf(request, sizeof(request), "GET /json/%s HTTP/1.0\r\nHost: ip-api.com\r\n\r\n", ip_address);

    // Send the HTTP request
    if (send(sockfd, request, strlen(request), 0) == -1)
    {
        perror("send");
        return;
    }

    // Log the response from the server
    FILE *file = fopen("log.txt", "a");
    if (file == NULL)
    {
        perror("fopen");
        return;
    }

    while (1)
    {
        ssize_t bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
        if (bytes_received == -1)
        {
            perror("recv");
            break;
        }
        else if (bytes_received == 0)
        {
            break;
        }

        response[bytes_received] = '\0';

        fprintf(file, "------------------------\n");
        fprintf(file, "Response from GET request:\n%s\n", response);
        fprintf(file, "------------------------\n");
        printf("------------------------\n");
        printf("Response from GET request:\n%s\n", response);
        printf("------------------------\n");
    }

    fclose(file);

    // Close the socket
    close(sockfd);
}

void *send_emote(void *arg)
{
    int client_internet_socket = *(int *)arg;
    // Define the ASCII art emote to be sent
    const char *emote = "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡏⠁⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠟⠛⠁⠀⢺⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣟⣧⠀⠐⢯⠀⠸⠻⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⠀⠀⠀⠀⠀⠀⠘⢿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⡀⠀⠀⠀⢠⠄⠘⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⢿⠿⠛⠛⠋⠀⠀⠀⠀⣠⣴⣾⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⠃⠀⠀⣀⢀⠀⠀⠀⠀⢰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣿⣿⣿⣿⡏⠀⢀⢠⣿⣿⣿⣷⡀⠀⠀⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⠿⠻⠏⠛⠀⣴⣾⣿⣿⣿⣿⣿⠃⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣷⣤⣤⣀⣰⣿⣿⣿⣿⣿⣿⡛⠀⠐⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡆⠀⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⠀⠸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                        "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n";

    printf("\nStarted Attack\n");
    // Continuously send the emote to the client
    while (1)
    {
        int bytes_sent = send(client_internet_socket, emote, strlen(emote), 0);
        if (bytes_sent == -1)
        {
            perror("send");
            break;
        }
        usleep(100000); // Sleep for 100 milliseconds to prevent system overload
        total_bytes_sent += bytes_sent;
    }
    printf("\nFinished Attack\n");

    return NULL;
}

void execution(int client_internet_socket)
{
    // Step 1: Receive initial data
    printf("\nExecution Start!\n");
    http_get();
    char buffer[1000];

    // Create a new thread to send emote
    pthread_t send_thread;
    pthread_create(&send_thread, NULL, send_emote, &client_internet_socket);

    while (1)
    {
        int number_of_bytes_received = recv(client_internet_socket, buffer, sizeof(buffer) - 1, 0);
        if (number_of_bytes_received == -1)
        {
            perror("recv");
            break;
        }
        else if (number_of_bytes_received == 0)
        {
            printf("Client closed the connection.\n");
            break;
        }

        buffer[number_of_bytes_received] = '\0';
        printf("Received: %s\n", buffer);

        // Log the received message
        FILE *log_file = fopen("log.txt", "a");
        if (log_file == NULL)
        {
            perror("fopen");
            break;
        }
        fprintf(log_file, "Message from client: %s\n", buffer);
        fclose(log_file);
    }

    // Wait for the send thread to finish
    pthread_join(send_thread, NULL);

    // Log and print the total number of bytes delivered successfully
    FILE *log_file = fopen("log.txt", "a");
    if (log_file == NULL)
    {
        perror("fopen");
        close(client_internet_socket);
        exit(4);
    }
    fprintf(log_file, "------------------------\n");
    fprintf(log_file, "Total bytes delivered: %d\n", total_bytes_sent);
    fprintf(log_file, "------------------------\n");
    fclose(log_file);
    printf("------------------------\n");
    printf("Total bytes delivered: %d\n", total_bytes_sent);
    printf("------------------------\n");

    // Close the client connection
    close(client_internet_socket);
}
