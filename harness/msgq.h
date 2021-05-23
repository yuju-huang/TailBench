#ifndef __MSGQ_H
#define __MSGQ_H

#define NUM_LATS 3
#define BUFF_SIZE (NUM_LATS * sizeof(uint64_t))

#define CMD_GET_LAT 3
#define CMD_PUT_LAT 2
#define CMD_FINISH 1

typedef double lats_t[NUM_LATS]; // p50, p95, p99

union msgdata {
    unsigned char raw[BUFF_SIZE];
    lats_t lats;
};

struct mq_msgbuf {
    long type;
    union msgdata data;
};

int mq_init();
int mq_recv(int mqid, struct mq_msgbuf* buf);
int mq_send(int mqid, const struct mq_msgbuf* buf);

#endif  // __MSGQ_H
