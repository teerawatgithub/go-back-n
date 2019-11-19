#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "idle-rq.h"
#include "trouble-maker.h"

const int PARITY_BIT = 15;
const int SEQ_BIT = 14;
const int LAST_INDICATOR_BIT = 13;
const int ACK_BIT = 12;
const int TIMEOUT_MSEC = 1200; // milli secs

void joinframes(short *frames, char *buf, int len);
short* makeframes(char *buf, size_t len);
int parity(short frame);
int corrupted(short frame);
void printstat(short frame);

ssize_t mysend(int sockfile, const void *buf, size_t len, int flags) {
  char *tmp = (char*)buf;
  printf("Buf (%zu):\n", len);
  while (*tmp) {
    printbytebits(*tmp);
    tmp++;
  }
  // make frames
  short *frames = makeframes((char*)buf, len);
  // for each frame, send to secondary, set a timer to resend I(N)
  // and wait for secondary to
  // send a frame back, test for ACK/NAK, if it's ACK(N) and not corrupted,
  // send next frame(N+1) else send current frame again
  size_t n = len;
  printf("Frames (%zu):\n", n);
  int i;
  for (i = 0; i < n; i++) {
    // IDLE state: assume that we receive an incoming I-frame
    // from above layer without the need to wait then we send it immediately
    printf(">  Sending I-frame %d: ", i);
    printbits(frames[i]);
    printstat(frames[i]);
    // TxFrame (format and transmit the frame)
    mightsend(sockfile, frames[i]);
    // TODO: Start_timer; Vs++; (Vs=send sequence variable, which is not important
    // in this implementation)

    // set timeout for recv(), see http://stackoverflow.com/a/2939145/2593810
    struct timeval tv;
    tv.tv_sec = TIMEOUT_MSEC / 1000;
    tv.tv_usec = (TIMEOUT_MSEC % 1000) * 1000;
    setsockopt(
      sockfile,
      SOL_SOCKET,
      SO_RCVTIMEO,
      (const char*)&tv,
      sizeof(struct timeval)
    );

    // PresentState = WTACK: wait for Secondary to respond
    short ack; // we prefer ACK_BIT bit to mean ACK, if it's 1 or NAK if 0
    printf("Timer Started: Waiting for an ACK frame ...\n");
    ssize_t status = recv(sockfile, &ack, 2, 0); // receiving the ACK frame
    // TODO: TEXP: if time expire do RetxFrame; Start_timer; PresentState=WTACK;

    if (status == 0) {
      // the Secondary has closed the socket
      printf("Secondary has closed connection, indicating proper transmission. ACK frame not needed. Primary process is terminating.\n");
      break;
    } else if (status < 2) { // Timer expired
      printf("TIMEOUT: No response within %d millisecs. Retransmit this I-frame again.\n", TIMEOUT_MSEC);
      i--;
      continue;
    }
    int isack = testbit(ack, ACK_BIT);
    int corrup = corrupted(ack);
    printf(" < Receiving %s frame: ", corrup ? "a corrupted" : isack ? "ACK" : "NAK");
    printbits(ack);
    printstat(ack);
    if (isack) {
      int NS = testbit(frames[i], SEQ_BIT);
      int NR = testbit(ack, SEQ_BIT);
      int P0 = NS == NR;
      int P1 = !corrup;
      if (P0) {
        if (P1) {
          // TODO: Stop_timer; (in our implementation, we won't do anything)
          printf("Timer Stopped: Valid ACK N(S)=N(R)=%d is received.\n", NR);
          // State=IDLE (in our implementation, go send another frame immediately)
          continue;
        } else {
          printf("ACK frame received is corrupted\n");
          // RetxFrame; retransmit I-frame waiting acknowledgement
          printf("Resend this I-frame again.\n");
          // Start_timer;
          // PresentState = WTACK
          // all of the above can be accomplished by redoing this loop
          i--;
          continue;
        }
      } else {
          printf("Error: Expected N(S)=N(R)=%d, got N(S)=%d\n", NR, NS);
        if (!P1) {
          // PresentState = IDLE;
          // Error++;
          printf("Error: Wrong Seq ACK and corrupted\n");
        } else {
          // do nothing
          printf("Error: Wrong Seq ACK and not corrupted\n");
        }
      }
    } else {
      // RetxFrame; Start_timer; PresentState=WTACK;
      printf("Resend this I-frame again.\n");
      i--;
      continue;
    }
  }
  printf("All %zu frames sent\n", n);
  return len;
}

ssize_t myrecv(int sockfile, void *buf, size_t len, int flags) {
  // forever try to receive frames
  // for each frame, if corrupted or not proper order send NAK frame,
  // else send ACK and if last frame break
  short frames[len+1], frame;
  short ack; // declared for temporary uses
  int i = 0; // frame number that we are waiting for
  while (1) {
    // PresentState=WTIFM; Waiting for event: IRCVD
    printf("Waiting for an I-frame ...\n");
    ssize_t status = recv(sockfile, &frame, 2, 0);
    if (status == 0) {
      fprintf(stderr, "Primary has closed connection, unexpected behavior!\n");
      return i;
    }
    int corrup = corrupted(frame);
    int NS = testbit(frame, SEQ_BIT);
    int Vr = i % 2;
    int P0 = NS == Vr;
    int P1 = !corrup;
    int P2 = NS == !Vr;
    int last = testbit(frame, LAST_INDICATOR_BIT) && P0 && P1;
    printf(" < Receiving %sI-frame %d: ", corrup ? "a corrupted " : last ? "the last " : "", i);
    printbits(frame);
    printstat(frame);
    ack = 0;
    int X = NS;
    int isack;
    if (!P1) {
      // TxNAK(X);
      isack = 0;
    } else {
      if (P2) {
        // TxACK(X);
        isack = 1;
        fprintf(stderr, "The I-frame order is invalid. Duplication detected.\n");
        printf("Expected N(S)=Vr=%d, got N(S)=%d\n", Vr, X);
      } else if (P0) {
        // LDATAind: Pass contents of received I-frame to user AP with
        // L_DATA.indication primitive
        frames[i] = frame;
        // TxACK(X);
        isack = 1;
        // Vr = Vr + 1;
        i++;
      } else {
        printf("P1 and not P2 and not P0, impossible\n");
      }
    }
    setbit(&ack, ACK_BIT, isack);
    setbit(&ack, SEQ_BIT, X);
    setbit(&ack, PARITY_BIT, parity(ack));
    printf(">  Sending %s frame: ", testbit(ack, ACK_BIT) ? "ACK" : "NAK");
    printbits(ack);
    printstat(ack);
    mightsend(sockfile, ack);

    // check for last frame
    if (last) {
      /* printf("This is the last I-frame.\n"); */
      // you can try to not send the last ACK frame back and let Primary notice
      // that you got the last I-frame by closing the socket
      // Primary should check if the socket is closed that mean you are done
      // and it should stop resending the last I-frame to you and stop waiting
      // for the ack from you
      printf("Got last frame, stopped\n");
      break;
    }
  }

  // join packets from frames together into *buf
  printf("Joining frames ...\n");
  joinframes(frames, buf, i);
  return strlen(buf);
}

// join frames into buffer data
void joinframes(short *frames, char* buf, int len) {
  size_t i, j;
  for (i = 0; i < len; i++) { // for each buffer
    buf[i] = 0;
    for (j = 0; j < 8; j++) { // for each bit
      if (testbit(frames[i], j)) {
        buf[i] |= 1 << j;
      }
    }
  }
  buf[i] = 0;
}

// split data into packets then make frames containing them
// 4th bit is nothing, 5th bit is last frame, 6th bit is seqNo, 7th bit is parity
short *makeframes(char *buf, size_t len) {
  short *frames = (short*) malloc(len+1);
  int fNo = 0; // current frame number that we are filling bits into
  int done = 0;
  while (!done) {
    frames[fNo] = 0;
    int i;
    for (i = 0; i < 8; i++) {
      if (buf[fNo] & (1 << i)) {
        frames[fNo] |= 1 << i;
      }
    }

    // add seqNo bit
    if (fNo % 2) { // if current frame sequence number is odd
      setbit(frames+fNo, SEQ_BIT, 1); // turn on the seqNo bit
    }

    if (buf[fNo+1] == 0 || fNo+1 >= len) { // if buf[fNo] is the last element
        setbit(frames+fNo, LAST_INDICATOR_BIT, 1); // last frame indicator
        done = 1;
        frames[fNo+1] = 0;
    }

    // add parity bit
    if (parity(frames[fNo])) {
      setbit(frames+fNo, PARITY_BIT, 1);
    }
    fNo++;
  }
  return frames;
}

// return parity function of 0th to 7th bit
// currently, the parity function is just XOR
int parity(short frame) {
  int result = 0, i;
  for (i = 0; i < PARITY_BIT; i++) {
    result ^= (frame >> i) & 1;
  }
  return result;
}

int corrupted(short frame) {
  return parity(frame) != ((frame >> PARITY_BIT) & 1);
}

void printstat(short frame) {
  /* printf("ACK: %d ", testbit(frame, ACK_BIT)); */
  /* printf("Last: %d ", testbit(frame, LAST_INDICATOR_BIT)); */
  /* printf("SEQ: %d ", testbit(frame, SEQ_BIT)); */
  /* printf("Parity: %d\n", testbit(frame, PARITY_BIT)); */
  printf("\n");
}
