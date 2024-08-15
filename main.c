#include <stdio.h> // Standard I/O library for input/output functions
#include <unistd.h> // POSIX API for Unix standard functions
#include <arpa/inet.h> // Definitions for internet operations
#include <pthread.h> // POSIX thread library for multi-threading
#include <string.h> // String handling functions, like strcmp
#include <stdlib.h>  // Standard library functions

#define BUFFER_SIZE 4096 // Set the buffer size to the maximum message size
#define PORT 8080 // Define the port number for the server/client

// Function for the peers to receive messages from each other
// Void pointer is used as argument and return to be able to use function in pthread_create
void* receive_messages(void* arg) {
    printf("Running receive messages!");

    // Cast the argument to an integer socket file descriptor
    // Dereference to get the value from the integer pointer
    int sockfd = *((int*)arg);
    char buffer[BUFFER_SIZE]; // Buffer to store received messages
    int bytes_received; // Number of bytes received from recv function

    printf("sockfd in receive_messages function: %d", sockfd);

    // Continually receive bytes from the peer until there is an error or the p2p connection is interrupted
    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        printf("Number of bytes received: %d", bytes_received);
        // Null-terminate the received data since recv does not null terminate the buffer by default and C expects null terminated strings
        buffer[bytes_received] = '\0'; 
        printf("Peer: %s\n", buffer); // Print received bytes as a string to the standard output
    }

    // If no more bytes are received, causing the while loop to stop, print that the peer has disconnected
    if (bytes_received == 0) {
        printf("Peer disconnected\n");
    } else if (bytes_received < 0) {
        // If bytes received are less than 0, this indicates an error in the recv function
        perror("Receive failed");
    }

    // No data needs to be returned in the thread function, which expects void*, so just return NULL
    return NULL;
}

// Function for the peers to send messages to each other
void send_messages(int sockfd) {
    // Buffer to store outgoing message
    char buffer[BUFFER_SIZE]; 

    while (1) {
        // Get the input from standard input and store in the outgoing message buffer
        // Stores BUFFER_SIZE - 1 bytes and appends an null-termination character to the buffer
        // Null termination character will not be transferred to socket communication and therefore it needs to be manually added in the receive messages function
        fgets(buffer, BUFFER_SIZE, stdin);

        // Send the data from the outgoing message buffer to the peer
        // The data is sent through the network socket, specified by the socket file descriptor (sockfd)
        // The outgoing message buffer and length of the message being sent is then specified
        // The last argument specifies how the data should be sent, 0 specifying default mode
        // In default send mode, if send cannot send all the specified bytes because of a full send buffer, etc.
        // Send will block further incoming bytes until the number of specified bytes has been sent
        send(sockfd, buffer, strlen(buffer), 0);
    }
}


// Specify the argument count (argc) and the argument vector (argv)
// to be able to specify if the application should act as a server or a client
int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Invalid number of arguments!\n");
        return 1;
    }

    int sockfd, client_fd; // Socket file descriptors
    struct sockaddr_in address; // Structure to hold socket address information
    pthread_t recv_thread_server, recv_thread_client; // Receive thread identifiers for concurrently receiving and sending capabilities
    socklen_t addr_len = sizeof(address); // Length of the address structure

    // Create a TCP (AF_INET), stream socket (SOCK_STREAM), with the default protocol (0 = TCP for stream sockets)
    // Returns a non-negative value representing the socket file descriptor
    // Negative value indicates error during socket creation
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed!"); // Print error message
        // Exit program, perform necessary cleanup and return EXIT_FAILURE status code, indicating termination due to error
        exit(EXIT_FAILURE); 
    }

    address.sin_family = AF_INET; // Set the address family to IPv4
    // Convert the port number to 16-bit number using Host TO Network Short (htons)
    address.sin_port = htons(PORT);

    // Parse the second argument in the argument vector (program is first argument)
    // Compare it to "server" to verify that this version of the application should run as a server
    // strcmp compares each character in the strings and returns the difference in ASCII-value
    // between the first non-equal character in the compared strings, otherwise it returns 0
    // if the strings are the same
    if (strcmp(argv[1], "server") == 0) {
        // Set the socket in address of the server socket to accept any address
        address.sin_addr.s_addr = INADDR_ANY;

        // Bind the socket to the port by specifying the socket file descriptor of the previously created socket
        // Cast the socket address of type sockaddr_in, which is used for specifying IPV4 addresses, to the sockaddr type
        // Size of address struct is needed for bind to know how to read the address data, since bind can handle multiple address types of multiple struct sizes
        if (bind(sockfd, (struct sockaddr*) &address, sizeof(address)) < 0) {
            // If the return value from bind is negative, socket binding failed
            perror("Bind failed!"); // Print error message
            close(sockfd); // Close the socket
            exit(EXIT_FAILURE); // Exit the program with an error code
        }

        // Since this peer acts as the server,
        // the socket with the socket file descriptor sockfd, listens for incoming connections
        // A maximum of 1 pending connection is allowed and only 1 allowed is needed since the idea is that only one peer will connect
        if (listen(sockfd, 1) < 0) {
            // If return value from listen is negative, socket listening failed
            perror("Listen failed!"); // Print error message
            close(sockfd); // Close the socket
            exit(EXIT_FAILURE); // Exit the program with an error code
        }

        // Communicate that the socket is listening and on what port
        printf("Server listening on port %d\n", PORT);

        // The socket with the socket file descriptor sockfd will accept the next pending connection
        // The address information of the socket which is requesting a connection (the client socket), 
        // will be stored in in the address structure when the accept function returns
        // the addr_len will initially store the size of the address structure with its current information (the size of the servers address)
        // Since it is a pointer to a socklen_t variable, the address of the addr_len variable will be used as an argument
        // When the accept function returns, addr_len will be updated with the actual size of the clients address
        // The function returns the socket file descriptor of the connecting socket (the client socket)
        client_fd = accept(sockfd, (struct sockaddr*) &address, &addr_len);
        if (client_fd < 0) {
            // If return value from accept is negative, accepting the connection failed
            perror("Accept failed"); // Print error message
            close(sockfd); // Close the socket
            exit(EXIT_FAILURE); // Exit the program with an error code
        }

        // Inform the user that a peer is connected
        printf("Connected to peer (client) with sockfd: %d, as server.\n", client_fd);

        // Create a POSIX-thread to handle receiving messages
        // The first argument is a pointer to a variable of type phtread_t, which stores the id of the newly created thread
        // The second argument specifies attributes for the new thread such as stack size, scheduling policy, etc.
        // Since we don't want any special policies, pass in a NULL value
        // The third argument is a pointer to the function the thread will execute
        // The function passed must match the signature void *function_name(void *arg), which our receive_messages function does
        // The fourth argument is the argument that will be passed as an argument to the start routine (the function executed in the thread)
        // The argument is sent in as a void pointer, therefore we will provide the address of the sockfd variable which is the socket file descriptor for our server socket
        // We want our server to receive messages, and therefore its socket file descriptor is passed into the thread function
        pthread_create(&recv_thread_server, NULL, receive_messages, &sockfd);

        // Handle message sending
        // The socket file descriptor of the server socket is passed in to the send messages function
        // This is because the send_messages function takes in the socket file descriptor of the sender of a message
        send_messages(sockfd);
    } else if (strcmp(argv[1], "client") == 0) {
        printf("Enter server IP address: "); // Prompt the user to enter the server IP address
        // Buffer to store the IP address in its human readable form
        // INET_ADDRSTRLEN is the length of a human readable IPV4 address, i.e "192.168.1.1"
        char ip[INET_ADDRSTRLEN];
        // Read a sequence of characters from the standard output until a whitespace character is encountered (space, tab, newline, etc.)
        scanf("%s", ip);
        // After scanf, the newline character is left in the input buffer, call getchar to clear the input buffer of the newline character
        getchar();
        // Convert the ip address given to binary format and store it in the address structure
        address.sin_addr.s_addr = inet_addr(ip);

        // Connect the client socket, specified by the socket file descriptor, to the server socket
        // The ip address of the server socket is stored in the address structure
        // Port and address family is set before the if-statement
        // The server socket accepts any ip address, defined by INADDR_ANY
        // Size of the current address is passed in to define how the address structure should be read
        if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            // If return value from connect is negative, connecting to the server socket has failed
            perror("Connection to server socket failed!"); // Print an error message if connection failed
            close(sockfd); // Close the socket
            exit(EXIT_FAILURE); // Exit the program with an error code
        }

        // Inform user of successful connection
        printf("Connection successful!\nConnected to peer as client, client sockfd: %d", sockfd);

        // Create a thread to handle receiving messages, similar to the server socket
        // Use a different identifier to have a handle to the clients receive thread as well
        pthread_create(&recv_thread_client, NULL, receive_messages, &sockfd);

        // Handle sending messages for the client socket
        send_messages(sockfd); 
    } else {
        // Print error message if invalid arguments are used
        printf("Invalid argument. Use 'server' or 'client'.\n");
        close(sockfd); // Close the socket
        return 1; // Exit the program with a non-zero status, indicating faulty program termination
    }

    close(sockfd); // Close the main socket when done
    return 0; // Return 0 to indicate successful program termination
}