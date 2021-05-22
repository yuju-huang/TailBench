/*C code*/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

#include "msgq.h"

#define GETEKYDIR ("/tmp")
#define PROJECTID  (2333)

static void err_exit(char *buf) {
    fprintf(stderr, "%s\n", buf);
    exit(1);
}

int mq_init() {
    key_t key = ftok(GETEKYDIR, PROJECTID);
    if (key < 0)
        err_exit("ftok error");

    int mqid;
    mqid = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if (mqid == -1) {
        if ( errno == EEXIST ) {
            printf("message queue already exist\n");
            mqid = msgget(key, 0);
            printf("reference mqid = %d\n", mqid);
        } else {
            perror("errno");
            err_exit("msgget error");
        }
    }
    printf("%s finish, return %d\n", __func__, mqid);
    return mqid;
}

int mq_recv(int mqid, struct mq_msgbuf* buf) {
    const size_t msg_size = sizeof(struct mq_msgbuf) - sizeof(long);
    assert(buf);

    if (msgrcv(mqid, buf, msg_size, 0, /* all type */0) == -1) {
        perror( "msgrcv() failed");
        exit(1);
    }

    printf("%s type=%ld, recv %s\n", __func__, buf->type, buf->data.raw);
    assert(buf->type == CMD_GET_LAT);
    return 0;
}

int mq_send(int mqid, const struct mq_msgbuf* buf) {
    const size_t msg_size = sizeof(struct mq_msgbuf) - sizeof(long);
    assert(buf);
    assert(buf->type == CMD_PUT_LAT);

    if (msgsnd(mqid, buf, msg_size, 0) == -1) {
        perror( "msgsnd() failed");
        exit(1);
    }
    return 0;
}
