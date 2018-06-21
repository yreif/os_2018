#ifndef WHATSAPPCOMMON_H
#define WHATSAPPCOMMON_H

#include <unistd.h>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sys/socket.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstdlib>
#include "whatsappio.h"

#define MAX_PENDING 10
#define MAX_CLIENTS 10
#define SEND_QUE "SEND\n"

using namespace std;


enum Response {SUCCESS, FAILURE, NAME_EXISTS, INVALID_RESPONSE};
int sendSuccessSignal(int fd);

int sendFailureSignal(int fd);

int sendNameExistsSignal(int fd);

int sendExitSignal(int fd);

Response responseType(const string &response) {
    string resp {response};
    if (resp == "success") return SUCCESS;
    if (resp == "failure") return FAILURE;
    if (resp == "name_exists") return NAME_EXISTS;
    return INVALID_RESPONSE;
}

bool is_server_exit(const string &command) {
    return (command == "server_exit");
}

bool is_message_from(const string &command) {
    return (command == SEND_QUE);
}

int read_data(int socket, string &message) {
    char buf[WA_MAX_INPUT + 1] = {0};
    char *bp = buf;
    /* counts bytes read */
    int bcount = 0;
    /* bytes read this pass */
    int br = 0;
    while (bcount < WA_MAX_INPUT) { /* loop until full buffer */

        br = read(socket, buf, WA_MAX_INPUT-bcount);
        if (br > 0) {
            bcount += br;
            bp += br;
        }
        else if (br < 1) return -1;
    }
    message = string(buf);
    return  (bcount);
}

int send_data(int socket, const string &message) {
    char buf[WA_MAX_INPUT + 1] = {0};
    char *bp = buf;
    memcpy(buf, message.c_str(), message.size());

    /* counts bytes read */
    int bcount = 0;
    /* bytes read this pass */
    int br = 0;
    while (bcount < WA_MAX_INPUT) { /* loop until full buffer */
        br = write(socket, bp, WA_MAX_INPUT-bcount);
        if (br > 0) {
            bcount += br;
            bp += br;
        }
        if (br < 1) return -1;
    }
    return bcount;
}

inline bool toUnsignedShort(const char *s, unsigned short& output)
{
    if((s != nullptr && s[0] == '\0') || ((!isdigit(s[0]) && s[0] != '-') && (s[0] != '+'))) return false;

    char * p;
    output = (unsigned short) strtol(s, &p, 10);

    return (*p == 0);
}

int sendSuccessSignal(int fd) {
    return send_data(fd, "success");
}

int sendFailureSignal(int fd) {
    return send_data(fd, "failure");
}

int sendNameExistsSignal(int fd) {
    return send_data(fd, "name_exists");
}

int sendExitSignal(int fd) {
    return send_data(fd, "server_exit");
}



#endif //WHATSAPPCOMMON_H
