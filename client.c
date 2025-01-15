#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 128
#define PORT 8080

void handleMessage(int sockfd) {
  char buffer[BUFFER_SIZE];

  printf("Type your messages. Type 'exit' to disconnect.\n");

  while (1) {
    // Get user input
    printf("You: ");
    if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
      perror("Error reading input");
      break;
    }

    // Remove trailing newline, if present
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
    }

    // Send the message to the server
    ssize_t bytesWritten = write(sockfd, buffer, strlen(buffer));
    if (bytesWritten == -1) {
      perror("Error sending message to server");
      break;
    }

    // Exit if the user types "exit"
    if (strcmp(buffer, "exit") == 0) {
      printf("Disconnecting from server...\n");
      break;
    }

    // Receive the server's response
    bzero(buffer, BUFFER_SIZE);
    ssize_t bytesRead = read(sockfd, buffer, BUFFER_SIZE - 1);
    if (bytesRead <= 0) {
      if (bytesRead == 0) {
        printf("Server closed the connection.\n");
      } else {
        perror("Error reading response from server");
      }
      break;
    }

    buffer[bytesRead] = '\0'; // Ensure null-termination
    printf("Server: %s\n", buffer);
  }

  close(sockfd); // Close the socket after exiting the loop
}

int main(int argc, char *argv[]) {
  // create the socket
  int sockfd;
  sockfd = socket(AF_INET, SOCK_STREAM, 0); // domain, type, protocol
  if (sockfd == -1) {
    printf("socket creation failed...\n");
    exit(1);
  } else {
    printf("socket was created \n");
  }

  struct sockaddr_in servaddr, clientaddr;
  bzero(&servaddr, sizeof(servaddr)); // make the values 0, just in case

  // assign IP, PORT
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  servaddr.sin_port = htons(PORT);

  // connect client socket to the server socket
  int connErr = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
  if (connErr == -1) {
    printf("connection failed...\n");
    exit(EXIT_FAILURE);
  } else {
    printf("connected to the server\n");
  }

  // talk to the server
  handleMessage(sockfd); // NOTE: have to use sockfd

  return EXIT_SUCCESS;
}
