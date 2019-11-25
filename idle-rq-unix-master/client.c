#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include "idle-rq.h"
#include <sys/time.h>
#include <sys/timeb.h>



#define PORT 5104
#define ADDRESS "127.0.0.1" // IP cil.informatics = 10.16.64.39
#define INFILENAME "input.txt"
#define OUTFILENAME "result_client.txt"
#define MAX_LINE_WIDTH 500
#define MAX_FILE_SIZE 10000

struct timeb start,end;
struct timeval start2, end2;
int diff;

int main(void) {
  char content[MAX_FILE_SIZE];
  content[0] = 0;

  int pipefd[2];
  pid_t cpid;
  pipe(pipefd); // create the pipe
  cpid = fork(); // duplicate the current process
  if (cpid == 0) // if I am the child then
  {
    char buf;
    close(pipefd[1]); // close the write-end of the pipe, I'm not going to use it
    while (read(pipefd[0], &buf, 1) > 0) // read until EOF
      strcat(content, &buf);
    close(pipefd[0]); // close the read-end of the pipe
  }
  else // if I am the parent then
  {
    // read file
    FILE *file = fopen(INFILENAME, "r");
    if (file == NULL) {
      fprintf(stderr, "Error: Cannot open file \"%s\"\n", INFILENAME);
      return EXIT_FAILURE;
    }
    char line[MAX_LINE_WIDTH];
    while (fgets(line, MAX_LINE_WIDTH, file) != NULL) {
      strcat(content, line);
    }
    //printf("File read: %s\n", INFILENAME);
    if (fclose(file) == EOF) {
      fprintf(stderr, "Error: Cannot close the file\n");
      return EXIT_FAILURE;
    }
    close(pipefd[0]); // close the read-end of the pipe, I'm not going to use it
    write(pipefd[1], content, strlen(content)); // send the content to the reader
    close(pipefd[1]); // close the write-end of the pipe, thus sending EOF to the reader
    wait(NULL); // wait for the child process to exit before I do the same
    exit(EXIT_SUCCESS);
  }

  srand(time(NULL));
  // socket - create an endpoint for communication
  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_desc == -1) {
    fprintf(stderr, "Error: Cannot create a socket\n");
    return EXIT_FAILURE;
  }

  // connect - initiate a connection on a socket
  struct sockaddr_in servaddr;
  servaddr.sin_family = AF_INET;
  // htons - hostshort to network (little endian to big endian)
  servaddr.sin_port = htons(PORT);
  servaddr.sin_addr.s_addr = inet_addr(ADDRESS);
  //printf("Connecting to %s on port %d ...\n", ADDRESS, PORT);
  int connect_status = connect(
    socket_desc,
    (struct sockaddr *) &servaddr,
    sizeof(servaddr)
  );
  if (connect_status == -1) {
    fprintf(stderr, "Error: Cannot connect to the server\n");
    return EXIT_FAILURE;
  }
  //printf("Connection established.\n");

  // communicate
  ftime(&start);
  gettimeofday(&start2, NULL);
  //printf("Sending Message: \n\"%s\"\n", content);
  int len = strlen(content);
  int sent_bytes = send(socket_desc, content, len, 0);
  if (sent_bytes != len) {
    fprintf(stderr, "Error: send() sent a different number of bytes than\
        expected\n");
    return EXIT_FAILURE;
  }

  /* // receiving from the server */
  /* int msgsize = recv(socket_desc, content, MAX_FILE_SIZE, 0); */
  /* if (msgsize == -1) { */
  /*   fprintf(stderr, "Error: Cannot receive using recv()\n"); */
  /*   return EXIT_FAILURE; */
  /* } */
  /* content[msgsize] = 0; */
  /* printf("Message received, bytes: %d\n", msgsize); */
  /* printf("The message is \n\"%s\"\n", content); */

  /* // writing the result to a file */
  /* file = fopen(OUTFILENAME, "w"); */
  /* if (fputs(content, file) == EOF) { */
  /*   fprintf(stderr, "Error: Cannot write to a file\n"); */
  /*   return EXIT_FAILURE; */
  /* } */
  /* printf("File recently wrote: %s\n", OUTFILENAME); */
  /* if (fclose(file) == EOF) { */
  /*   fprintf(stderr, "Error: Cannot close the file after writing\n"); */
  /*   return EXIT_FAILURE; */
  /* } */

  // close the socket and free the port
  int close_status = close(socket_desc);
  if (close_status == -1) {
    fprintf(stderr, "Error: Cannot close a socket\n");
    return EXIT_FAILURE;
  }
  ftime(&end);
  gettimeofday(&end2, NULL);
  diff = (int) (1000.0 * (end.time - start.time) + (end.millitm - start.millitm));
  unsigned long time_in_microsStart = 1000000 * start2.tv_sec + start2.tv_usec;
  unsigned long time_in_microsStop = 1000000 * end2.tv_sec + end2.tv_usec;
  // printf("%u milliseconds \n", diff);
  printf("\nTime: %ld microsecons\n", time_in_microsStop - time_in_microsStart);
  return EXIT_SUCCESS;
}
