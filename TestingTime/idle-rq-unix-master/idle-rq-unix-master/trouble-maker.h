
// this is a header file for the troublesome communication channel
// it randomly corrupts messages and sometime discards them all
// but it should not keep the message and delay the transmission
// by sending it after a new message, that would mimic network layer's
// premature timeout problem, but we want to mimic link layer.
// idle RQ does not work if premature timeout problem is found

short corrupt(short frame);
void mightsend(int sockfile, short frame);
void printbytebits(char byte);
void printbits(short frame);
int rand_lim(int limit);
int testbit(short frame, int bitorder);
void setbit(short *frame, int bitorder, int value);
