#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h> // read(), write(), close()

#define BUFFER_SIZE 128
#define PORT 8080
#define MAX_CLIENTS 100

// NOTE: client list is global so a mutex will be used
struct clientData {
  char client_name[BUFFER_SIZE];
  int connfd;
};

struct clientData *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// client handling
void addClient(struct clientData *client) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] == NULL) {
      clients[i] = client;
      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

void removeClient(struct clientData *client) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] == client) {
      clients[i] = NULL;
      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

// NOTE: you have to use *args because of the way that you call arguments when
// making a new thread
void *handleClient(void *arg) {
  int connfd = *(int *)arg;
  free(arg); // Free the dynamically allocated memory for connfd
  char buffer[BUFFER_SIZE];

  while (1) {
    // Read the message from the client
    ssize_t bytesRead = read(connfd, buffer, sizeof(buffer) - 1);
    if (bytesRead <= 0) {
      if (bytesRead == 0) {
        printf("Client disconnected...\n");
      } else {
        perror("Error reading from client");
      }
      break;
    }

    buffer[bytesRead] = '\0'; // Ensure null-termination
    printf("Client said: %s\n", buffer);

    // check if the client wants a name
    const char *REQUEST = "REQUEST_NAME: ";
    int wantsName = strncmp(REQUEST, buffer, strlen(REQUEST));
    if (wantsName == 0) {
      // create client struct
      struct clientData client = {};
      strcpy(client.client_name, buffer);
      client.connfd = connfd;

      // add name to the clientlist
      addClient(&client);

      // Tell the client they got their name
      const char *response = "You are the name you wanted: ";
      char result[BUFFER_SIZE + 50] = {0};
      snprintf(result, BUFFER_SIZE + 50, "%s %s", response, client.client_name);

      write(connfd, result, strlen(result));

    } else {
      // Reply to the client
      const char *response = "I got your message\n";
      write(connfd, response, strlen(response));

      // Exit if the client sends "exit"
      if (strncmp(buffer, "exit", 4) == 0) {
        printf("Client requested disconnection.\n");
        break;
      }
    }
  }

  close(connfd);
  pthread_exit(NULL);
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

  // create a new thread for each client
  while (1) {
    // accept
    socklen_t clientAddrSize = sizeof(clientaddr);
    int *connfd = malloc(sizeof(int));
    *connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientAddrSize);

    if (*connfd == -1) {
      printf("Failed to accept request... \n ");
      free(connfd);
      continue;
    }
    printf("Accepted connection \n");

    // create thread
    pthread_t thread_id;
    int threadErr = pthread_create(&thread_id, NULL, *handleClient, connfd);

    if (threadErr != 0) {
      perror("failed to create thread...");
      free(connfd);
      continue;
    } else {
      printf("Created thread for client...\n ");
    }

    // to avoid memory leaks
    pthread_detach(thread_id);
  }
  return 0;
}
