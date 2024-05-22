#ifdef _WIN32
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
	#define perror(string) fprintf( stderr, string ": WSA errno = %d\n", WSAGetLastError() )
#else
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

#define SERVERPORT "22"
#define CLIENTPORT "80"

struct OneReverse {
    int internet_socket;
    FILE* filePointer;
    char client_address_string[INET6_ADDRSTRLEN];
};


int initialization_server();
int initialization_client();
int connection( int internet_socket,const char * client_address_string, int size );
void IPRequest(const char * client_address_string,FILE *filePointer,char thread_id_str[20]);
void execution( int internet_socket,FILE * filePointer, char client_address_string[INET6_ADDRSTRLEN]);
void cleanup(int client_internet_socket);
void* threadExecution(void* arg);

int main( int argc, char * argv[] )
{

    FILE *filePointer =fopen( "log.log", "w" ); //open the log.log file in write mode

    OSInit();// initialize the OS

    int internet_socket = initialization(0);//open socket for botnets

    char client_address_string[INET6_ADDRSTRLEN];//string to store IP address

    while(1) {



        int client_internet_socket = connection(internet_socket, client_address_string, sizeof(client_address_string)); //let a new client connect

        pthread_t thread;//Variable to store identifier for new thread

        //allocate memory for the ThreadArgs struct
        struct OneReverse* args = (struct ThreadArgs*)malloc(sizeof(struct OneReverse));

        args->internet_socket = client_internet_socket;//assign the value of client_internet_socket to the member of the struct
        args->filePointer = filePointer; // assign the value of the filePointer to the struct
        strcpy(args->client_address_string, client_address_string);//copy the ip address in the stuct


        int thread_create_result = pthread_create(&thread, NULL, threadExecution, args); //create a new thread that runs threadExecution
        if (thread_create_result != 0) {
            fprintf(stderr, "Failed to create thread: %d\n", thread_create_result);
        }

    }
    fclose(filePointer); //close file

    cleanup(internet_socket); //cleanup

    OSCleanup();

    return 0;
}

int initialization_server()
{
	//Step 1.1
	struct addrinfo internet_address_setup;
	struct addrinfo * internet_address_result;
	memset( &internet_address_setup, 0, sizeof internet_address_setup );
	internet_address_setup.ai_family = AF_UNSPEC;
	internet_address_setup.ai_socktype = SOCK_STREAM;
	internet_address_setup.ai_flags = AI_PASSIVE;
	int getaddrinfo_return = getaddrinfo( NULL, SERVERPORT, &internet_address_setup, &internet_address_result );
	if( getaddrinfo_return != 0 )
	{
		fprintf( stderr, "getaddrinfo: %s\n", gai_strerror( getaddrinfo_return ) );
		exit( 1 );
	}

	int internet_socket = -1;
	struct addrinfo * internet_address_result_iterator = internet_address_result;
	while( internet_address_result_iterator != NULL )
	{
		//Step 1.2
		internet_socket = socket( internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol );
		if( internet_socket == -1 )
		{
			perror( "socket" );
		}
		else
		{
			//Step 1.3
			int bind_return = bind( internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen );
			if( bind_return == -1 )
			{
				perror( "bind" );
				close( internet_socket );
			}
			else
			{
				//Step 1.4
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

int initialization_client()
{
	//Step 1.1
	struct addrinfo internet_address_setup;
	struct addrinfo * internet_address_result;
	memset( &internet_address_setup, 0, sizeof internet_address_setup );
	internet_address_setup.ai_family = AF_UNSPEC;
	internet_address_setup.ai_socktype = SOCK_STREAM;
	int getaddrinfo_return = getaddrinfo( "ip-api.com", CLIENTPORT, &internet_address_setup, &internet_address_result );
	if( getaddrinfo_return != 0 )
	{
		fprintf( stderr, "getaddrinfo: %s\n", gai_strerror( getaddrinfo_return ) );
		exit( 1 );
	}

	int internet_socket = -1;
	struct addrinfo * internet_address_result_iterator = internet_address_result;
	while( internet_address_result_iterator != NULL )
	{
		//Step 1.2
		internet_socket = socket( internet_address_result_iterator->ai_family, internet_address_result_iterator->ai_socktype, internet_address_result_iterator->ai_protocol );
		if( internet_socket == -1 )
		{
			perror( "socket" );
		}
		else
		{
			//Step 1.3
			int connect_return = connect( internet_socket, internet_address_result_iterator->ai_addr, internet_address_result_iterator->ai_addrlen );
			if( connect_return == -1 )
			{
				perror( "connect" );
				close( internet_socket );
			}
			else
			{
				break;
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

void IPRequest(const char * client_address_string,FILE *filePointer,char thread_id_str[20]){

    int internet_socket_HTTP = initialization(1); //initialize API socket


    //print IP address in log file
    fputs("ID: ", filePointer);
    fputs(thread_id_str, filePointer);
    fputs(" The IP address =",filePointer);
    fputs(client_address_string,filePointer);
    fputs("\n",filePointer);

    char buffer[1000];
    char HTTPrequest[100] ={0};


    //make HTTPrequest
    sprintf(HTTPrequest,"GET /json/%s HTTP/1.0\r\nHost: ip-api.com\r\nConnection: close\r\n\r\n", client_address_string);
    printf("HTTP request = %s",HTTPrequest);


    //SEND HTTPREQUEST TO GET LOCATION OF BOTNET
    int number_of_bytes_send = 0;
    number_of_bytes_send = send( internet_socket_HTTP, HTTPrequest , strlen(HTTPrequest), 0 );
    if( number_of_bytes_send == -1 )
    {
        perror( "send" );
    }

    //receive json data and cut the HTTP header off
    int number_of_bytes_received = 0;
    number_of_bytes_received = recv( internet_socket_HTTP, buffer, ( sizeof buffer ) - 1, 0 );
    if( number_of_bytes_received == -1 )
    {
        perror( "recv" );
    }
    else
    {
        buffer[number_of_bytes_received] = '\0';
        printf( "Received : %s\n", buffer );
    }

    char* jsonFile = strchr(buffer,'{');

    if( jsonFile == NULL){
        number_of_bytes_received = recv( internet_socket_HTTP, buffer, ( sizeof buffer ) - 1, 0 );
        if( number_of_bytes_received == -1 )
        {
            perror( "recv" );
        }
        else
        {
            buffer[number_of_bytes_received] = '\0';
            printf( "Received : %s\n", buffer );
        }

        //put geolocation in file
        fputs("ID: ", filePointer);
        fputs(thread_id_str, filePointer);
        fputs(" Geolocation = ",filePointer);
        fputs( buffer , filePointer );
        fputs("\n",filePointer);
    } else{

        //put geolocation in file
        fputs("ID: ", filePointer);
        fputs(thread_id_str, filePointer);
        fputs(" Geolocation = ",filePointer);
        fputs(jsonFile, filePointer);
        fputs("\n",filePointer);
    }

    //clean HTTP socket
    cleanup(internet_socket_HTTP);
}

int connection( int internet_socket,const char * client_address_string, int size )
{
	//Step 2.1
	struct sockaddr_storage client_internet_address;
	socklen_t client_internet_address_length = sizeof client_internet_address;
	int client_socket = accept( internet_socket, (struct sockaddr *) &client_internet_address, &client_internet_address_length );
	if( client_socket == -1 )
	{
		perror( "accept" );
		close( internet_socket );
		exit( 3 );
	}
	
        //get IP address from client
        if (client_internet_address.ss_family == AF_INET) {
            // IPv4 address
            struct sockaddr_in* s = (struct sockaddr_in*)&client_internet_address;
            inet_ntop(AF_INET, &s->sin_addr, client_address_string, size);
        }
		 else { // AF_INET6
            // IPv6 address
            struct sockaddr_in6* s = (struct sockaddr_in6*)&client_internet_address;
            inet_ntop(AF_INET6, &s->sin6_addr, client_address_string, size);
        }

        printf("Client IP address: %s\n", client_address_string);

    return client_socket;
}
	


void execution( int internet_socket,FILE * filePointer, char client_address_string[INET6_ADDRSTRLEN])
{
	    //get the thread_id and put it in the string
    pthread_t thread_id = pthread_self();
    char thread_id_str[20];
    sprintf(thread_id_str, "%ld", thread_id);

    //receive a message
    int number_of_bytes_received = 0;
    char buffer[100];

    number_of_bytes_received = recv(internet_socket, buffer, (sizeof buffer) - 1, 0);
    if (number_of_bytes_received == -1) {
        perror("recv");
    } else {
        buffer[number_of_bytes_received] = '\0';
        printf("Received : %s\n", buffer);
    }

    // get the geolocation
    IPgetRequest(client_address_string, filePointer,thread_id_str);

    //put the message that the client sent in the log file
    fputs("Thread ID: ", filePointer);
    fputs(thread_id_str, filePointer);
    fputs(" Client sent = ",filePointer);
    fputs(buffer, filePointer);
    fputs("\n",filePointer);
    //Step 3.1
    int number_of_bytes_send = 0;
    int totalBytesSend = 0;

    char totalBytesSendStr[20];


    while(1){

        //UNO REVERSE TIME
        number_of_bytes_send = send( internet_socket, "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n"
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
                                                      "⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿\n", 400, 0 );

        //check if client left and put number of bytes send in the log file
        if( number_of_bytes_send == -1 )
        {
            printf("Client left.  %d bytes\n",totalBytesSend);
            sprintf(totalBytesSendStr, "%d", totalBytesSend);
            fputs("ID: ", filePointer);
            fputs(thread_id_str, filePointer);
            fputs(" Total bytes send = ",filePointer);
            fputs(totalBytesSendStr, filePointer);
            fputs("\n",filePointer);
            break;
        }else{
            totalBytesSend +=number_of_bytes_send;
            usleep(100000);// Just here to test or i wil attack myself to hard
        }
    }
    cleanup(internet_socket);
}

void cleanup(int client_internet_socket)
{
    //Step 4.2
#ifdef _WIN32 //cleanup for windows
    int shutdown_return = shutdown( client_internet_socket, SD_RECEIVE );
    if( shutdown_return == -1 )
    {
        perror( "shutdown" );
    }
#else // cleanup for linux
    int shutdown_return = shutdown( client_internet_socket, SHUT_RD );
    if( shutdown_return == -1 )
    {
        perror( "shutdown" );
    }
#endif


    //Step 4.1
    close( client_internet_socket );
}

//function for thread execution
void* threadExecution(void* arg) {
    struct ThreadArgs* args = (struct ThreadArgs*)arg;

    // Execute the code for the client in this thread
    execution(args->internet_socket, args->filePointer, args->client_address_string); // call the execution for each client 

    // Clean up and exit the thread
    free(args);
    pthread_exit(NULL);
}