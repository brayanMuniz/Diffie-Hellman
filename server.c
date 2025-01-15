#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> // read(), write(), close()

#define BUFFER_SIZE 128
#define PORT 8080

// handle client messages
void handleMessage(int connfd) {
  char buffer[BUFFER_SIZE];
  int bytesRead;

  while (1) {

    // read the message and print it out
    bytesRead = read(connfd, buffer, sizeof(buffer));
    if (bytesRead <= 0) {
      printf("Client disconnected or error occurred\n");
      break;
    } else {
      printf("Received %d bytes\n", bytesRead);
    }

    buffer[bytesRead] = '\0'; // ensure the string is null-terminated
    printf("Client said: %s\n", buffer);
    bzero(buffer, BUFFER_SIZE);

    // reply to the client
    const char *response = "I got your message\n";
    strncpy(buffer, response, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0'; // ensure null termination

    // send the response back to the client
    write(connfd, buffer, strlen(buffer));

    // exit if client wishes to
    if (strncmp("exit", buffer, 4) == 0) {
      printf("Server Exit...\n");
      break;
    }
  }
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
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(PORT);

  // bind to the socket
  int err = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
  if (err == -1) {
    printf("socket binding failed...\n");
    exit(1);
  } else {
    printf("socket was binded \n");
  }

  // listen to request
  int listenErr = listen(sockfd, 100); // second parameter defines the maximum
                                       // length for the queue of pending
  if (listenErr == -1) {
    printf("failed to start listening... \n");
    exit(1);
  } else {
    printf("listening... \n");
  }
  socklen_t clientAddrSize = sizeof(clientaddr);

  // accept
  int connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientAddrSize);
  if (connfd == -1) {
    printf("Failed to accept request... \n ");
    exit(1);
  } else {
    printf("Accepted request \n");
  }

  // handle clients messages
  handleMessage(connfd);

  return 0;
}
