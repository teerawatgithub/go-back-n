#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "trouble-maker.h"
#include "stdio.h"
#include "stdbool.h"

/* const int MAX_DELAY_MS = 3000; */
const int SEND_CHANCE = 100; // chance to actually send (and not get lost)
const int CORRUPT_CHANCE =0; // chance to corrupt the frame given sending event

struct send_data { // used for passing into thread function
  int sockfile;
  short frame;
  int delay;
};

/* void *timed_send(void *arg); */

/* void *timed_send(void *arg) { */
/*   struct send_data *data = (struct send_data*) arg; */
/*   usleep(data->delay * 1000); */
/*   send(data->sockfile, &data->frame, 2, 0); */
/*   printf("\t[[ CHANNEL: The frame %d is sent ]]\n", data->frame); */
/*   free(data); */
/* } */

// randomly toggle a bit of the frame
short corrupt(short frame) {
  int bit = rand_lim(15);
  return frame ^ (1 << bit);
}

void mightsend(int sockfile, short frame) {
  int random = rand_lim(100);
  if (random <= SEND_CHANCE) { // chance to send and not get lost on the way
    random = rand_lim(100);
    if (random <= CORRUPT_CHANCE) { // chance to corrupt
      int corrupted_frame = corrupt(frame);
      printf("\t[[ CHANNEL: The frame %d is CORRUPTED into frame %d ]]\n", frame, corrupted_frame);
      frame = corrupted_frame;
    }
    int status = send(sockfile, &frame, 2, 0);
    if (status <= 0) {
      {
        // the Secondary has closed the socket
        printf("CHANNEL: [[ Sending failed, destination not available ]]\n");
      }
    }
    /* printf("\t[[ CHANNEL: The frame %d is SENT ]]\n", frame); */
    /* random = rand_lim(MAX_DELAY_MS); */
    /* struct send_data *data = (struct send_data*) malloc(sizeof(struct send_data)); */
    /* data->sockfile = sockfile; */
    /* data->frame = frame; */
    /* data->delay = random; */
    /* printf("\t[[ CHANNEL: The frame %d will be sent in %d ms ]]\n", frame, random); */
    /* pthread_t pt; */
    /* pthread_create(&pt, NULL, timed_send, data); */

  } else {
    printf("\t[[ CHANNEL: The frame %d is LOST ]]\n", frame);
  }
}

void printbytebits(char byte) {
  int i;
  for (i = 0; i < 8; i++) {
    bool on = testbit(byte, i);
    printf("%d", on);
  }
  printf("\n");
}

void printbits(short frame) {
  int i;
  for (i = 0; i < 16; i++) {
    bool on = testbit(frame, i);
    printf("%d", on);
    if (i == 7) printf(" ");
  }
  printf(" (%d) ", frame);
}

// see http://stackoverflow.com/a/2999130/2593810 for more information
int rand_lim(int limit) {
  /* return a random number uniformly between 0 and limit inclusive.
  */
  int divisor = RAND_MAX/(limit+1);
  int retval;
  do { 
    retval = rand() / divisor;
  } while (retval > limit);
  return retval;
}

int testbit(short frame, int bitorder) {
  return (frame >> bitorder) & 1;
}

void setbit(short *frame, int bitorder, int value) {
  *frame ^= (-value ^ *frame) & (1 << bitorder);
}
