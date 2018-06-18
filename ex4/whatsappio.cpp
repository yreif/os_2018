#include "whatsappio.h"
#include <cstdio>

void printServerExit() {
    printf("EXIT command is typed: server is shutting down\n");
}

void printConnection() {
    printf("Connected Successfully.\n");
}

void printConnectionServer(const std::string &client) {
    printf("%s connected.\n", client.c_str());
}


void printDupConnection() {
    printf("Client name is already in use.\n");
}

void printFailedConnection() {
    printf("Failed to connect the server\n");
}

void printServerUsage() {
    printf("Usage: whatsappServer portNum (portNum is only digits, [0-9])\n");
}

void printClientUsage() {
    printf("Usage: whatsappClient clientName serverAddress serverPort\n");
}

void printCreateGroup(bool server, bool success,
                      const std::string &client, const std::string &group) {
    if(server) {
        if(success) {
            printf("%s: Group \"%s\" was created successfully.\n",
                   client.c_str(), group.c_str());
        } else {
            printf("%s: ERROR: failed to create group \"%s\"\n",
                   client.c_str(), group.c_str());
        }
    }
    else {
        if(success) {
            printf("Group \"%s\" was created successfully.\n", group.c_str());
        } else {
            printf("ERROR: failed to create group \"%s\".\n", group.c_str());
        }
    }
}

void printSend(bool server, bool success, const std::string &client,
               const std::string &name, const std::string &message) {
    if(server) {
        if(success) {
            printf("%s: \"%s\" was sent successfully to %s.\n",
                   client.c_str(), message.c_str(), name.c_str());
        } else {
            printf("%s: ERROR: failed to sendData \"%s\" to %s.\n",
                   client.c_str(), message.c_str(), name.c_str());
        }
    }
    else {
        if(success) {
            printf("Sent successfully.\n");
        } else {
            printf("ERROR: failed to sendData.\n");
        }
    }
}

void printMessage(const std::string &client, const std::string &message) {
    printf("%s: %s\n", client.c_str(), message.c_str());
}

void printWhoServer(const std::string &client) {
    printf("%s: Requests the currently connected client names.\n", client.c_str());
}

void printWhoClient(bool success, const std::vector<std::string> &clients) {
    if(success) {
        bool first = true;
        for (const std::string& client: clients) {
            printf("%s%s", first ? "" : ",", client.c_str());
            first = false;
        }
        printf("\n");
    } else {
        printf("ERROR: failed to receive list of connected clients.\n");
    }
}

void printClientExit(bool server, const std::string &client) {
    if(server) {
        printf("%s: Unregistered successfully.\n", client.c_str());
    } else {
        printf("Unregistered successfully.\n");
    }
}

void printInvalidInput() {
    printf("ERROR: Invalid input.\n");
}

void printError(const std::string &function_name, int error_number) {
    printf("ERROR: %s %d.\n", function_name.c_str(), error_number);
}

void parseCommand(const std::string &command, CommandType &commandT,
                  std::string &name, std::string &message,
                  std::vector<std::string> &clients) {
    char c[WA_MAX_INPUT];
    const char *s; 
    char *saveptr;
    name.clear();
    message.clear();
    clients.clear();
    
    strcpy(c, command.c_str());
    s = strtok_r(c, " ", &saveptr);
    
    if(!strcmp(s, "createGroup")) {
        commandT = CREATE_GROUP;
        s = strtok_r(NULL, " ", &saveptr);
        if(!s) {
            commandT = INVALID;
            return;
        } else {
            name = s;
            while((s = strtok_r(NULL, ",", &saveptr)) != NULL) {
                clients.emplace_back(s);
            }
        }
    } else if(!strcmp(s, "sendData")) {
        commandT = SEND;
        s = strtok_r(NULL, " ", &saveptr);
        if(!s) {
            commandT = INVALID;
            return;
        } else {
            name = s;
            message = command.substr(name.size() + 6); // 6 = 2 spaces + "sendData"
        }
    } else if(!strcmp(s, "who")) {
        commandT = WHO;
    } else if(!strcmp(s, "serverExit")) {
        commandT = EXIT;
    } else {
        commandT = INVALID;
    }
}
