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
#define NAME_SIZE 10
#define MESSAGE_SIZE 50
#define PORT 8080
#define MAX_CLIENTS 10

// NOTE: client list is global so a mutex will be used
struct clientData {
  char client_name[NAME_SIZE];
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
      free(client);
      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

struct clientData *findClientByName(const char *userName) {
  pthread_mutex_lock(&clients_mutex);
  struct clientData *client = NULL;
  size_t userNameLength = strlen(userName);

  for (int i = 0; i < MAX_CLIENTS; i++) {
    printf("looking: %s|\n", clients[i]->client_name);
    if (clients[i] != NULL &&
        strncmp(clients[i]->client_name, userName, userNameLength) == 0) {
      client = clients[i];
      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
  return client;
}

char *findClientNameByConn(const int connfd) {
  pthread_mutex_lock(&clients_mutex);
  char *clientName = NULL;

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] != NULL && clients[i]->connfd == connfd) {
      clientName = clients[i]->client_name;
      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
  return clientName;
}

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

    if (strncmp(buffer, "REQUEST_NAME: \0", 14) == 0) {
      printf("Processing client name... \n");
      // extract name and add it to the struct
      char name[NAME_SIZE];
      strncpy(name, buffer + 14, NAME_SIZE);
      name[NAME_SIZE - 1] = '\0';

      // create client
      struct clientData *client = malloc(sizeof(struct clientData));
      if (client == NULL) {
        perror("Failed to allocate memory for clientData");
        break;
      }

      strcpy(client->client_name, name);
      client->connfd = connfd;

      // add name to the clientlist
      addClient(client);

      // Tell the client they got their name
      bzero(buffer, BUFFER_SIZE); // reset buffer
      const char *response = "You are the name you wanted: ";
      snprintf(buffer, BUFFER_SIZE, "%s %s", response, client->client_name);
      write(connfd, buffer, BUFFER_SIZE);

      printf("Client has been given a name: %s\n\n", client->client_name);
    }
    // process message
    // INFO: -u userName -m message
    else if (strncmp(buffer, "-u ", 3) == 0 && strstr(buffer, "-m ") != NULL) {
      printf("Processing message to another user...\n");
      // get sender name by connfd
      char *senderName = findClientNameByConn(connfd);
      if (senderName == NULL) {
        printf("How did you get here?");
        continue;
      }
      int senderNameLength = strlen(senderName);

      char *userNameStart = strstr(buffer, "-u ") + 3; // Start after "-u "
      while (*userNameStart == ' ')
        userNameStart++; // Skip leading spaces

      char *mFlag = strstr(buffer, "-m "); // Find the start of "-m"
      if (!mFlag || mFlag <= userNameStart) {
        printf("Invalid input format. Missing or misplaced '-m'.\n");
        const char *response = "Error: Invalid input format.\n";
        write(connfd, response, strlen(response));
        continue;
      }

      // Extract the username and trim trailing spaces
      size_t nameLength = mFlag - userNameStart;
      while (nameLength > 0 && userNameStart[nameLength - 1] == ' ') {
        nameLength--;
      }
      printf("nameLength: %zu\n", nameLength);

      char receiverName[nameLength];
      strncpy(receiverName, userNameStart, nameLength);
      receiverName[nameLength] = '\0'; // Null-terminate

      // extract message
      const char *messageStart = mFlag + 3; // Move past "-m "
      while (*messageStart == ' ')
        messageStart++; // Skip leading spaces

      size_t messageLength = strlen(messageStart);
      char message[messageLength];
      strncpy(message, messageStart, messageLength);
      message[messageLength] = '\0';

      // check if username exist, otherwise return
      struct clientData *receiverData = findClientByName(receiverName);
      if (receiverData == NULL) {
        printf("Could not find: %s\n", receiverName);
        const char *response = "The client you want does not exist\n";
        write(connfd, response, strlen(response));
        continue;
      }

      // send message to receiver
      int formattedMessageLength =
          senderNameLength + messageLength + 3; // +3 for space and :
      char *formattedMessage =
          (char *)malloc(formattedMessageLength * sizeof(char));
      sprintf(formattedMessage, "%s: %s", senderName, message);

      int receiverSuc =
          write(receiverData->connfd, formattedMessage, formattedMessageLength);
      if (receiverSuc == -1) {
        printf("Could not message: %s\n", receiverName);
        const char *response = "Receiver did NOT get your message\n";
        write(connfd, response, strlen(response));
        continue;
      }

      printf("Sent message to: %s\n", receiverName);
      const char *response = "Receiver got your message\n";
      int authorResponse = write(connfd, response, strlen(response));
      if (authorResponse == -1) {
        printf("Sender was not able to get the ACK");
      }
      continue;

    } else {
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
    printf("listening... \n\n");
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
      printf("Created thread for client...\n");
    }

    // to avoid memory leaks
    pthread_detach(thread_id);
  }
  return 0;
}
