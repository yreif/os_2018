#include <netinet/in.h>
#include <iostream>
#include "whatsappClient.h"
#include "whatsappServer.h"
#include "whatsappio.h"
#include "whatsappCommon.h"

int e(int ret, const char *sysCall) {
    if (ret >= 0) return 0;
    std::cerr << "ERROR: " << sysCall << " " << std::strerror(errno) << ".\n" << std::endl;
    exit(1);
}

bool validateName(const std::string &name) {
//    return find_if(name.begin(), name.end(), isalnum) == name.end();
    for (const char &c : name){ // we need to iterate over the c_str itself
        if (std::isalnum(c)==0) {
            return false;
        }
    }
    return true;
}



void WhatsappClient::create_group(const std::string &group_name, const std::vector<std::string> &clients_group, const std::string &command) {
    if (validateName(group_name)){
        for (unsigned int i = 0; i < clients_group.size(); ++i) {
            if (!validateName(clients_group[i])){
                printInvalidName();
                printCreateGroup(false, false, name, group_name);
                return;
            }
        }
        sendData(sockfd, command.c_str(), int(command.size()));
        char response;
        e(receiveData(sockfd, &response, sizeof(response)), "read");
        if (response == SUCCESS) {
            printCreateGroup(false, true, name, group_name);
        } else { // FAILURE
            printCreateGroup(false, false, name, group_name);
        }
    } else {
        printInvalidName();
        printCreateGroup(false, false, name, group_name);
    }
}
//
void WhatsappClient::send(const std::string &send_to, const std::string &message,  const std::string &command) {
    if (!validateName(send_to)){
        printInvalidName();
        printSend(false, false, name, send_to, message);
        return;
    } else {
        std::cout << "why2" << "\n";

        e(sendData(sockfd, command.c_str(), int(command.size()) +1), "write");
        char response;
        std::cout << "why3" << "\n";

        e(receiveData(sockfd, &response, sizeof(response) + 1), "read");

        std::cout << "why4" << "\n";


        if (response == SUCCESS) {
            printSend(false, true, name, send_to, message);
        } else { // FAILURE
            printSend(false, false, name, send_to, message);
        }

    }
}
//
void WhatsappClient::who(const std::string &command) {
    e(sendData(sockfd, command.c_str(), int(command.size())), "write");
    char buf[WA_MAX_INPUT];
    e(receiveData(sockfd, buf, WA_MAX_INPUT), "read");
    std::string who {buf};
    std::cout << who; // TODO: make sure during testing that we don't need endl here
}


void WhatsappClient::exit_client(const std::string &command) {
    e(sendData(sockfd, command.c_str(), int(command.size())), "write");
    e(close(sockfd), "close");
    printClientExit(false, name);
    exit(0);
}


int initialization(int argc, char *av[], WhatsappClient& client) {

    if (argc != 4) return -1;
    client.name = std::string(av[1]);
    if (!validateName(client.name)) return -1;
    client.name = std::string(av[1]);
    /* sockaddrr_in initialization */
    memset(client.server, 0, sizeof(struct sockaddr_in));
    /* Getting the ip address*/
    struct hostent *hp;
    e(((hp = gethostbyname(av[2])) == nullptr), "gethostbyname");
    memcpy((char *)&client.server->sin_addr , hp->h_addr , hp->h_length);
    client.server->sin_family = hp->h_addrtype;
    client.server->sin_port = htons((u_short)std::stoi(av[3]));
    e(client.sockfd = socket(hp->h_addrtype, SOCK_STREAM, 0), "socket");

    if (connect(client.sockfd, (struct sockaddr *)client.server , sizeof(*client.server)) < 0)
    {
        printFailedConnection();
        close(client.sockfd);
        exit(1);
    }

    sendData(client.sockfd, (client.name + "\n").c_str(), (int) (client.name + "\n").size());
    char response;
    e(receiveData(client.sockfd, &response, sizeof(response)), "read");
    if (response == SUCCESS) {
        printConnection();
        return 0;
    } else if (response == NAME_EXISTS) {
        printDupConnection();
        close(client.sockfd);
        exit(1);
    } else {
        printFailedConnection();
        close(client.sockfd);
        exit(1);
    }
}


int main(int argc, char *argv[])
{
    WhatsappClient client;
    if (initialization(argc, argv, client) < 0) {
        printClientUsage();
        return 0;
    }

    std::string curr_command;
    CommandType commandT;
    std::string name;
    std::string message;
    std::vector<std::string> clients;

    fd_set clientfds;
    fd_set readfds;
    FD_ZERO(&clientfds);
    FD_SET(client.sockfd, &clientfds);
    FD_SET(STDIN_FILENO, &clientfds);

    while (true) {

        readfds = clientfds;
        (e(select(MAX_CLIENTS + 1, &readfds, nullptr, nullptr, nullptr), "select")); // TODO: why MAX_CLIENTS?
        char buf[WA_MAX_INPUT]; // TODO: change size here to fit needs
        // if something happened on the server socket then it's an incoming message or exit process
        if (FD_ISSET(client.sockfd, &readfds)) {
//            std::istream::get(curr_command, '\n'); //TODO: handle this bitch
            e(receiveData(client.sockfd, buf, WA_MAX_INPUT*sizeof(char)), "read"); // TODO: change size here to fit needs
            if (!strcmp(curr_command.c_str(), &SERVER_EXIT)){
                close(client.sockfd);
                printClientExit(false, name);
                exit(0);
            } else if ((!strcmp(curr_command.c_str(), "SEND:\n"))) { // TODO: decide on a protocol
//                std::istream::get(curr_command, '\n');//TODO: handle this bitch
                e(receiveData(client.sockfd, buf, WA_MAX_INPUT*sizeof(char)), "read"); // TODO: change size here to fit needs
                std::string msg {buf};
                printf(msg.c_str()); // TODO: maybe this prints 2 newline, if so user cout <<
            }
        }
        else if (FD_ISSET(STDIN_FILENO, &readfds)) {
            std::cout << "why" << "\n";

//            std::istream::get(curr_command, '\n'); //TODO: handle this bitch
            std::getline(std::cin, curr_command);
            parseCommand(curr_command, commandT, name, message, clients);

            switch (commandT) {
                case CREATE_GROUP:
                    client.create_group(name, clients, curr_command);
                    break;
                case SEND:
                    client.send(name, message, curr_command);
                    break;
                case WHO:
                    client.who(curr_command);
                    break;
                case EXIT:
                    client.exit_client(curr_command);
                    break;
                case INVALID:
                    printInvalidInput();
                    break;
            }
        }
    }
}