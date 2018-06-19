#include <netinet/in.h>
#include <iostream>
#include "whatsappClient.h"
#include "whatsappServer.h"


bool vlidate_name(const std::string &name) {
    for (const char &c : name){
        if (std::isalnum(c)==0) {
            return false;
        }
    }
    return true;
}


void errorHandler(int ret, const char *sysCall, const char *f) {  // TODO: terminate all clients, sockets, ...
    if (ret == 0) {
        return;
    }
    std::cerr << "Server: error in " << sysCall << " during " << f << std::endl;
    exit(1);
}


void WhatsappClient::create_group(const std::string &group_name, const std::vector<std::string> &clients_group, const std::string &command) {
    if (vlidate_name(group_name)){
        for (int i = 0; i < clients_group.size(); ++i) {
            if (!vlidate_name(clients_group[i])){
                printInvalidInput();
                //TODO: validity error
            }
        }
        sendData(sockfd, command.c_str(), int(command.size()));
        char response;
        if (recv(sockfd, &response, sizeof(response), 0) != 1){
            //TODO: ERROR
        }

        switch (response) {
            case SUCCESS:
                printCreateGroup(false, true, name, group_name);
                break;
            case NAME_EXISTS:
                printCreateGroup(false, false, name, group_name);
        }
    } else {
        //TODO: validity error
    }
}
//
void WhatsappClient::send(const std::string &send_to, const std::string &message,  const std::string &command) {
    if (!vlidate_name(send_to)){
        printInvalidInput();
    } else{
        sendData(sockfd, command.c_str(), int(command.size()));
        char response;
        if (recv(sockfd, &response, sizeof(response), 0) != 1){
            //TODO: ERROR
        } else {
            switch (response) {
                case SUCCESS:
                    printSend(false, true, name, send_to, message);
                    break;
                case FAILURE:
                    printSend(false, false, name, send_to, message);
                    break;
            }
        }
    }
}
//
void WhatsappClient::who(const std::string &command) {
    sendData(sockfd, command.c_str(), int(command.size()));
    std::string who(WA_MAX_INPUT,'');
    if (recv(sockfd, &who, WA_MAX_INPUT, 0) != 1){
        //TODO: ERROR
    } else {
        //TODO: is that enough?
        printf(who.c_str());

//        switch (response) {
//            case SUCCESS:
//
//                printSend(false, true, name, send_to, message);
//                break;
//            case FAILURE:
//                printSend(false, false, name, send_to, message);
//                break;
//        }
    }
}


void WhatsappClient::exit_client(const std::string &command) {
    sendData(sockfd, command.c_str(), int(command.size()));
    close(sockfd);
    printClientExit(false, name);
    exit(1);
}


WhatsappClient initialization(int argc, char *av[]) {
    if (argc != 4)
    {
        printClientUsage();

    }
    else
    {
        if (!vlidate_name(av[1])){
            printInvalidInput();
            return(-1); //TODO: errors
        } //TODO: improve!!
        WhatsappClient client;
        client.name = av[1];


        /* sockaddrr_in initialization */
        memset(client.servera, 0, sizeof(struct sockaddr_in));
        /* Getting the ip address*/
        struct hostent *hp;
        if ((hp = gethostbyname (av[2])) == NULL) {
            return(-1); //TODO: errors
        }
        memcpy((char *)&client.servera->sin_addr , hp->h_addr , hp->h_length);
        client.servera->sin_family = hp->h_addrtype;
        client.servera->sin_port = htons((u_short)std::stoi(av[3]));

        if ((client.sockfd = socket(hp->h_addrtype, SOCK_STREAM,0)) < 0)
        {
            return(-1);//TODO: errors
        }

        if (connect(client.sockfd, (struct sockaddr *)&client.servera , sizeof(client.servera)) < 0)
        {
            printFailedConnection();

            close(client.sockfd);
            return(-1);//TODO: errors
        }

        sendData(client.sockfd, client.name, sizeof(client.name));
        char response;
        if (recv(client.sockfd, &response, sizeof(response), 0) != 1){
            //TODO: ERROR
        } else {
            switch (response) {
                case SUCCESS:
                    printConnection();
                    break;
                case FAILURE:
                    printFailedConnection();
                    break;
                case NAME_EXISTS:
                    printDupConnection();
                    break;
            }
        }
        //TODO: writename, approvl...
        return client;

    }
}


int main(int argc, char *argv[])
{
    auto client  = initialization(argc, argv);

    std::string curr_command;
    CommandType commandT;
    std::string name;
    std::string message;
    std::vector<std::string> clients(0);

    fd_set clientfds;
    fd_set readfds;
    FD_ZERO(&clientfds);
    FD_SET(client.sockfd, &clientfds);
    FD_SET(STDIN_FILENO, &clientfds);

    while (true) {
        readfds = clientfds;
        if (error(select(MAX_CLIENTS + 1, &readfds, nullptr, nullptr, nullptr), "select")) continue;

        // if something happened on the server socket then it's an incoming message or exit process
        if (FD_ISSET(client.sockfd, &readfds)) {
            std::istream::get(curr_command, '\n'); //TODO: handle this bitch
            if (!strcmp(curr_command.c_str(), &SERVER_EXIT)){
                close(client.sockfd);
                printClientExit(false, name);
                exit(1);

            } else if ((!strcmp(curr_command.c_str(), "SEND:\n")) || (!strcmp(curr_command.c_str(), "SEND:"))){
                std::istream::get(curr_command, '\n');//TODO: handle this bitch
                printf(curr_command.c_str());

            }
        }
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            std::istream::get(curr_command, '\n'); //TODO: handle this bitch
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
                    std::cout << "BUG: client sent invalid command" << std::endl; // client sent invalid command - bug TODO: remove before submitting
                    break;
            }        }
    }

    return 0;
}