#include <netinet/in.h>
#include <iostream>
#include "whatsappClient.h"
#include "whatsappServer.h"
#include "whatsappio.h"
#include "whatsappCommon.h"

int e(int ret, const char *sysCall) {
    if (ret >= 0) return 0;
    cerr << "ERROR: " << sysCall << " " << strerror(errno) << ".\n" << endl;
    exit(1);
}

bool validateName(const string &name) {
//    return find_if(name.begin(), name.end(), isalnum) == name.end();
    for (const char &c : name){ // we need to iterate over the c_str itself
        if (isalnum(c)==0) {
            return false;
        }
    }
    return true;
}



void WhatsappClient::create_group(const string &group_name, const vector<string> &clients_group, const string &command) {
    if (!validateName(group_name)) {
        printCreateGroup(false, false, name, group_name);
        return;
    }
    if (group_name == name){
        printCreateGroup(false, false, name, group_name);
        return;
    }
    for (const auto &i : clients_group) {
        if (!validateName(i)) {
            printCreateGroup(false, false, name, group_name);
            return;
        }
    }

    send_data(sockfd, command);
    string response;
    e(read_data(sockfd, response), "read");
    printCreateGroup(false, responseType(response) == SUCCESS, name, group_name);

}

void WhatsappClient::send(const string &send_to, const string &message,  const string &command) {
    if (!validateName(send_to) || send_to == name){
        printSend(false, false, name, send_to, message);
        return;
    }
    e(send_data(sockfd, command), "write");

    string response;
    e(read_data(sockfd, response), "read");

    printSend(false, responseType(response) == SUCCESS, name, send_to, message);

}

void WhatsappClient::who(const string &command) {
    e(send_data(sockfd, command), "write");
    string who;
    e(read_data(sockfd, who), "read");
    cout << who << endl;
}

void WhatsappClient::exit_client(const string &command) {
    e(send_data(sockfd, command), "write");
    e(close(sockfd), "close");
    printClientExit(false, name);
    exit(0);
}

int initialization(int argc, char *av[], WhatsappClient& client) {
    struct sockaddr_in server;
    if (argc != 4) return -1;
    string n(av[1]);
    client.name = n;

    if (!validateName(client.name)) return -1;
    /* sockaddrr_in initialization */

    memset(&server, 0, sizeof(struct sockaddr_in));

    /* Getting the ip address*/
    struct hostent *hp;
    e(((hp = gethostbyname(av[2])) == nullptr), "gethostbyname");
    memcpy((char *)&server.sin_addr , hp->h_addr , hp->h_length);
    server.sin_family = hp->h_addrtype;
    server.sin_port = htons((u_short)stoi(av[3]));
    e(client.sockfd = socket(hp->h_addrtype, SOCK_STREAM, 0), "socket");

    if (connect(client.sockfd, (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        printFailedConnection();
        close(client.sockfd);
        exit(1);
    }
    send_data(client.sockfd, client.name);
    string response;
    e(read_data(client.sockfd, response), "read");
    switch (responseType(response)){
        case SUCCESS:
            printConnection();
            return 0;
        case NAME_EXISTS:
            printDupConnection();
            close(client.sockfd);
            exit(1);
        case FAILURE:
            printFailedConnection();
            close(client.sockfd);
            exit(1);
        case INVALID_RESPONSE:
            printFailedConnection();
            close(client.sockfd);
            exit(1);
    }
    return 0;
}

int main(int argc, char *argv[]) {

    WhatsappClient client;
    if (initialization(argc, argv, client) < 0) {
        printClientUsage();
        return 0;
    }

    string curr_command;
    CommandType commandT;
    string name;
    string message;
    vector<string> clients;

    fd_set clientfds;
    fd_set readfds;
    FD_ZERO(&clientfds);
    FD_SET(client.sockfd, &clientfds);
    FD_SET(STDIN_FILENO, &clientfds);

    while (true) {
        curr_command.clear();
        readfds = clientfds;
        (e(select(MAX_CLIENTS + 1, &readfds, nullptr, nullptr, nullptr), "select"));

        if (FD_ISSET(client.sockfd, &readfds)) /*message from server */
        {
            e(read_data(client.sockfd, curr_command), "read");

            if (is_server_exit(curr_command)) /*EXIT from server */{
                close(client.sockfd);
                printClientExit(false, client.name);
                exit(0);
                
            } else if (is_incoming_message(curr_command)) /*message from other client */ {
                cout << get_incoming_message(curr_command) << endl;
            }
        }
        else if (FD_ISSET(STDIN_FILENO, &readfds)) {
            getline(cin, curr_command);
            try {
                parseCommand(curr_command, commandT, name, message, clients);
            } catch (const exception& e) {
                printInvalidInput();
                continue;
            }

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
    return 0;
}