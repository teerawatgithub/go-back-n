#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <ctype.h>
#include "idle-rq.h"
#include <time.h>

#define PORT 5104
#define BUFSIZE 10000
#define OUTFILENAME "result_server.txt"

// invert case of every characters inside the string
void invertcase(char *str) {
  size_t len = strlen(str);
  size_t i;
  for (i = 0; i < len; i++) {
    if (isalpha(str[i])) {
      str[i] ^= 1 << 5;
    }
  }
}

int main(void) {
  int bufsize = BUFSIZE;
  char msgbuffer[bufsize];
  int pipefd[2];
  pid_t cpid;
  pipe(pipefd); // create the pipe
  cpid = fork(); // duplicate the current process
  if (cpid == 0) // if I am the child then
  {
    close(pipefd[0]); // close the read-end of the pipe, I'm not going to use it
  }
  else // if I am the parent then
  {
    close(pipefd[1]); // close the write-end of the pipe, thus sending EOF to the reader
    char buf;
    int msgsize = 0;
    while (read(pipefd[0], &buf, 1) > 0) // read until EOF
    {
      msgbuffer[msgsize] = buf;
      msgsize++;
    }
    msgbuffer[msgsize] = 0;
    close(pipefd[0]); // close the read-end of the pipe, I'm not going to use it
    wait(NULL); // wait for the child process to exit before I do the same

    printf("Message received, bytes: %d\n", msgsize);
    printf("The message is \n\"%s\"\n", msgbuffer);

    // invert case the entire buffer
    invertcase(msgbuffer);
    printf("The message after case inversion: \n\"%s\"\n", msgbuffer);

    // writing the result to a file
    FILE *file = fopen(OUTFILENAME, "w");
    if (fputs(msgbuffer, file) == EOF) {
      fprintf(stderr, "Error: Cannot write to a file\n");
      return EXIT_FAILURE;
    }
    printf("File recently wrote: %s\n", OUTFILENAME);
    if (fclose(file) == EOF) {
      fprintf(stderr, "Error: Cannot close the file after writing\n");
      return EXIT_FAILURE;
    }
    exit(EXIT_SUCCESS);
  }

  srand(time(NULL));
  // socket - create an endpoint for communication
  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_desc == -1) {
    fprintf(stderr, "Error: Cannot create a socket\n");
    return EXIT_FAILURE;
  }
  // try reusing the socket with the same port when it says that address
  // is already in use when bind
  int yes = 1;
  if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
    perror("setsockopt");
    exit(1);
  }

  // bind - bind a name to a socket
  struct sockaddr_in addrport;
  addrport.sin_family = AF_INET;
  // htons - hostshort to network (little endian to big endian)
  addrport.sin_port = htons(PORT);
  addrport.sin_addr.s_addr = htonl(INADDR_ANY); // htonl = hostlong to network
  // assigning a name to a socket
  int bind_status = bind(
    socket_desc,
    (struct sockaddr *) &addrport,
    sizeof(addrport)
  );
  if (bind_status == -1) {
    fprintf(stderr, "Error: Cannot bind a socket\n");
    return EXIT_FAILURE;
  }

  // listen - listen for connections on a socket
  int qLimit = 10;
  int listen_status = listen(socket_desc, qLimit);
  if (listen_status == -1) {
    fprintf(stderr, "Error: Cannot listen for a socket\n");
    return EXIT_FAILURE;
  }

  // accept - accept a connection on a socket
  struct sockaddr_in clientAddr;
  int clientLen;
  // for (;;) {
  clientLen = sizeof(clientAddr);
  printf("Accepting a connection on port %d ...\n", PORT);
  int client_socket = accept(
    socket_desc, // input
    (struct sockaddr *) &clientAddr, // output
    &clientLen // input & output
  );
  if (client_socket == -1) {
    fprintf(stderr, "Error: Cannot accept a socket\n");
    return EXIT_FAILURE;
  }
  printf("A client from port %d is connected.\n", clientAddr.sin_port);
  // }

  // communicate
  int msgsize = myrecv(client_socket, msgbuffer, bufsize, 0);
  if (msgsize == -1 ) {
    fprintf(stderr, "Error: Cannot receive using recv()\n");
    return EXIT_FAILURE;
  }
  // ensure null-terminated string, otherwise it will print garbage below
  msgbuffer[msgsize] = 0;
  /* printf("Msg buffer: \"%s\" (%d)\n", msgbuffer, msgsize); */
  write(pipefd[1], msgbuffer, msgsize); // send the msg to the parent
  close(pipefd[1]); // close the write-end of the pipe

  /* // send back to client */
  /* int sent_bytes = send(client_socket, msgbuffer, msgsize, 0); */
  /* if (sent_bytes != msgsize) { */
  /*   fprintf(stderr, "Error: send() sent a different number of bytes than\ */
  /*       expected\n"); */
  /*   return EXIT_FAILURE; */
  /* } */
  /* printf("The message is sent back to the client.\n"); */

  // close the socket and free the port
  int close_status = close(socket_desc);
  if (close_status == -1) {
    fprintf(stderr, "Error: Cannot close a socket\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
