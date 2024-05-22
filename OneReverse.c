#ifdef _WIN32
    // Define the Windows version and include necessary headers for Windows
    #define _WIN32_WINNT _WIN32_WINNT_WIN7
    #include <winsock2.h> //for all socket programming
    #include <ws2tcpip.h> //for getaddrinfo, inet_pton, inet_ntop
    #include <stdio.h> //for fprintf, perror
    #include <unistd.h> //for close
    #include <stdlib.h> //for exit
    #include <string.h> //for memset
    #include <pthread.h>
    void OSInit( void )
    {
        WSADATA wsaData;
        int WSAError = WSAStartup( MAKEWORD( 2, 0 ), &wsaData ); 
        if( WSAError != 0 )
        {
            fprintf( stderr, "WSAStartup errno = %d\n", WSAError );
            exit( -1 );
        }
    }
    void OSCleanup( void )
    {
        WSACleanup();
    }
    // Redefine perror for Windows to use WSAGetLastError()
    #define perror(string) fprintf( stderr, string ": WSA errno = %d\n", WSAGetLastError() )
#else
    // Include necessary headers for Unix-based systems
    #include <sys/socket.h> //for sockaddr, socket, socket
    #include <sys/types.h> //for size_t
    #include <netdb.h> //for getaddrinfo
    #include <netinet/in.h> //for sockaddr_in
    #include <arpa/inet.h> //for htons, htonl, inet_pton, inet_ntop
    #include <errno.h> //for errno
    #include <stdio.h> //for fprintf, perror
    #include <unistd.h> //for close
    #include <stdlib.h> //for exit
    #include <string.h> //for memset
    void OSInit( void ) {}
    void OSCleanup( void ) {}
#endif

#define PORT "22"

int initialization();
int connection( int internet_socket);
void execution( int internet_socket );
void cleanup( int internet_socket, int client_internet_socket );
void http();
 
int total_bytes_sent = 0;
char ip_address[INET6_ADDRSTRLEN];

int main( int argc, char * argv[] )
{
    // Initialize the OS-specific network library
    OSInit();

    // Initialize the server socket
    int internet_socket = initialization();
    
    // Main loop to handle incoming connections
    while(1){
        int client_internet_socket = connection( internet_socket );
        execution( client_internet_socket );
    };

    // Cleanup (unreachable due to infinite loop)
    OSCleanup();

    return 0;
}

int initialization()
{
    // Setup address info for the server to listen on
    struct addrinfo internet_address_setup;
    struct addrinfo * internet_address_result;
    memset( &internet_address_setup, 0, sizeof internet_address_setup );
    internet_address_setup.ai_family = AF_UNSPEC;
    internet_address_setup.ai_socktype = SOCK_STREAM;
    internet_address_setup.ai_flags = AI_PASSIVE;
    int getaddrinfo_return = getaddrinfo( NULL, PORT , &internet_address_setup, &internet_address_result );
    if( getaddrinfo_return != 0 )
    {
        fprintf( stderr, "getaddrinfo: %s\n", gai_strerror( getaddrinfo_return ) );
        exit( 1 );
    }

    int internet_socket = -1;
    struct addrinfo * internet_address_result_iterator = internet_address_result;
    while( internet_address_result_iterator != NULL )
    {
        // Create a socket
        internet_socket = socket( internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol );
        if( internet_socket == -1 )
        {
            perror( "socket" );
        }
        else
        {
            // Bind the socket to the address
            int bind_return = bind( internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen );
            if( bind_return == -1 )
            {
                perror( "bind" );
                close( internet_socket );
            }
            else
            {
                // Listen for incoming connections
                int listen_return = listen( internet_socket, 1 );
                if( listen_return == -1 )
                {
                    close( internet_socket );
                    perror( "listen" );
                }
                else
                {
                    break;
                }
            }
        }
        internet_address_result_iterator = internet_address_result_iterator->ai_next;
    }

    freeaddrinfo( internet_address_result );

    if( internet_socket == -1 )
    {
        fprintf( stderr, "socket: no valid socket address found\n" );
        exit( 2 );
    }

    return internet_socket;
}

int connection( int internet_socket)
{
    printf("----Waiting for connection-------\n");
    // Accept an incoming connection
    struct sockaddr_storage client_internet_address;
    socklen_t client_internet_address_length = sizeof client_internet_address;
    int client_socket = accept( internet_socket, (struct sockaddr *) &client_internet_address, &client_internet_address_length );
    if( client_socket == -1 )
    {
        perror( "accept" );
        close( internet_socket );
        exit( 3 );
    }

    // Get the IP address of the client
    void* addr;
    if (client_internet_address.ss_family == AF_INET) {
        struct sockaddr_in* s = (struct sockaddr_in*)&client_internet_address;
        addr = &(s->sin_addr);
    } else {
        struct sockaddr_in6* s = (struct sockaddr_in6*)&client_internet_address;
        addr = &(s->sin6_addr);
    }

    inet_ntop(client_internet_address.ss_family, addr, ip_address, sizeof(ip_address));

    // Log the IP address of the client
    FILE* log_file = fopen("log.txt", "a");
    if (log_file == NULL) {
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

void http() {
    int sockfd;
    struct sockaddr_in server_addr;
    char request[256];
    char response[1024];

    // Making a new connection
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return;
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    server_addr.sin_addr.s_addr = inet_addr("ip-api.com");

    // Connect to the server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        return;
    }

    // Prepare HTTP request
    snprintf(request, sizeof(request), "GET /json/%s HTTP/1.0\r\nHost: ip-api.com\r\n\r\n", ip_address);

    // Send the HTTP request
    if (send(sockfd, request, strlen(request), 0) == -1) {
        perror("send");
        return;
    }

    // Putting  his location in file.
    FILE* file = fopen("log.txt", "a");
    if (file == NULL) {
        perror("fopen");
        return;
    }

    while (1) {
        ssize_t bytes_received = recv(sockfd, response, 1024 - 1, 0);
        if (bytes_received == -1) {
            perror("recv");
            break;
        } else if (bytes_received == 0) {
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

    // Close the connection
    close(sockfd);
}

void* send_emote(void* arg) {
    int client_internet_socket = *(int*)arg;
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
                         "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠀⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                         "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⠀⠸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
                         "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n";

    printf("\nStarted Attack\n");
    // Continuously send the emote to the client
    while (1) {
        int bytes_sent = send(client_internet_socket, emote, strlen(emote), 0);
        if (bytes_sent == -1) {
            perror("send");
            break;
        }
        usleep(100000);  // Sleep for 100 milliseconds to prevent system overload
        total_bytes_sent += bytes_sent;
    }
    printf("\nFinished Attack\n");

    return NULL;
}

void execution(int client_internet_socket) {
    printf("\nExecution Start!\n");
    // Make an HTTP request to log additional information
    http();
    char buffer[1000];

    // Create a new thread to send the emote continuously
    pthread_t send_thread;
    pthread_create(&send_thread, NULL, send_emote, &client_internet_socket);

    // Receive data from the client while sending the emote
    while (1) {
        int number_of_bytes_received = recv(client_internet_socket, buffer, sizeof(buffer) - 1, 0);
        if (number_of_bytes_received == -1) {
            perror("recv");
            break;
        } else if (number_of_bytes_received == 0) {
            // Client closed the connection
            printf("Client closed the connection.\n");
            break;
        }

        buffer[number_of_bytes_received] = '\0';
        printf("Received: %s\n", buffer);

        // Log the received message
        FILE* log_file = fopen("log.txt", "a");
        if (log_file == NULL) {
            perror("fopen");
            break;
        }
        fprintf(log_file, "Message from client: %s\n", buffer);
        fclose(log_file);
    }

    // Wait for the sending thread to finish
    pthread_join(send_thread, NULL);

    // Log the total number of bytes sent
    FILE* log_file = fopen("log.txt", "a");
    if (log_file == NULL) {
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

void cleanup( int internet_socket, int client_internet_socket )
{
    // Shutdown the client connection
    int shutdown_return = shutdown( client_internet_socket, SD_RECEIVE );
    if( shutdown_return == -1 )
    {
        perror( "shutdown" );
    }

    // Close both the client and server sockets
    close( client_internet_socket );
    close( internet_socket );
}
