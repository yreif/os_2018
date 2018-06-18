#ifndef WHATSAPPCOMMON_H
#define WHATSAPPCOMMON_H

#include <unistd.h>

#define NULL_TERMINATOR = '\0'
// TODO: remove this ^ if we don't use it
#define MAX_HOSTNAME 30
#define MAX_PENDING 10
#define MAX_CLIENTS 30

static const char SUCCESS = 1;
static const char NAME_EXISTS = 2;
static const char FAILURE = 3;
static const char SERVER_EXIT = 4;


int receiveData(int fd, char *buf, int n) {
    /* counts bytes read */
    int bcount = 0;
    /* bytes read this pass */
    int br = 0;
    while (bcount < n) { /* loop until full buffer */
        br = read(fd, buf, n-bcount);
        if (br > 0) {
            bcount += br;
            buf += br;
            if (buf[bcount] == '\n') {
                return bcount;
            }
        }
        else if ((br == 0) && (bcount >= 0)) return bcount;
        else if (br < 1) return -1;
    }
    return(bcount);
}

int sendData(int fd, const char *buf, int n) { // TODO: make sure this one is okay
    /* counts bytes read */
    int bcount = 0;
    /* bytes read this pass */
    int br = 0;
    while (bcount < n) { /* loop until full buffer */
        br = write(fd, buf, n-bcount); // TODO: why not send()?
        if (br > 0) {
            bcount += br;
            buf += br;
        }
        else if (br < 1) return -1;
    }
    return(bcount);
}

int sendSuccessSignal(int fd) {
    return (sendData(fd, &SUCCESS, 1) >= 0) ? 0 : -1;
}

int sendFailureSignal(int fd) {
    return (sendData(fd, &FAILURE, 1) >= 0) ? 0 : -1;

}

int sendNameExistsSignal(int fd) {
    return (sendData(fd, &NAME_EXISTS, 1) >= 0) ? 0 : -1;
}

int sendExitSignal(int fd) {
    return (sendData(fd, &SERVER_EXIT, 1) >= 0) ? 0 : -1;
}




#endif //WHATSAPPCOMMON_H
