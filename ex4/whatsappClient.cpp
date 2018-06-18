#include <netinet/in.h>
#include <iostream>
#include "whatsappClient.h"
#include "whatsappServer.h"


void errorHandler(int ret, const char *sysCall, const char *f) {  // TODO: terminate all clients, sockets, ...
    if (ret == 0) {
        return;
    }
    std::cerr << "Server: error in " << sysCall << " during " << f << std::endl;
    exit(1);
}


void WhatsappClient::create_group(const std::string &group_name, const std::vector<std::string> &clients_group) {

}

void WhatsappClient::send(const std::string &send_to, const std::string &message) {

}

void WhatsappClient::who() {

}

void WhatsappClient::exit() {

}


bool vlidate_name(const std::string &name) {
    for (const char &c : name){
        if (std::isalnum(c)==0) {
            return false;
        }
    }
    return true;
}

WhatsappClient initialization(int argc, char *av[]) {
    if (argc != 4)
    {
        std::cerr << "WhatsappClient: invalid call, to initialize call: whatsappClient clientName serverAddress serverPort" << std::endl;

    }
    else
    {
        vlidate_name(av[1]); //TODO: improve!!
        WhatsappClient client;


        /* sockaddrr_in initialization */
        memset(client.servera, 0, sizeof(struct sockaddr_in));
        /* Getting the ip address*/
        struct hostent *hp;
        int s;
        if ((hp = gethostbyname (av[2])) == NULL) {
            return(-1); //TODO: errors
        }
        memcpy((char *)&client.servera->sin_addr , hp->h_addr , hp->h_length);
        client.servera->sin_family = hp->h_addrtype;
        client.servera->sin_port = htons((u_short)std::stoi(av[3]));

        if ((client.sockfd = socket(hp->h_addrtype, SOCK_STREAM,0))
            < 0) {
            return(-1);//TODO: errors
        }

        if (connect(client.sockfd, (struct sockaddr *)&client.servera , sizeof(client.servera)) < 0) {
            close(client.sockfd);
            return(-1);//TODO: errors
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

    while (true) {
        std::istream::get(curr_command, '\n'); //TODO: handle this bitch
        parseCommand(curr_command, commandT, name, message, clients);

        switch (commandT) {
            case CREATE_GROUP:
                client.createGroup(client, name, group);
                break;
            case SEND:
                client.send(client, name, message);
                break;
            case WHO:
                client.who(client);
                break;
            case EXIT:
                client.clientExit(client);
                break;
            case INVALID:
                std::cout << "BUG: client sent invalid command" << std::endl; // client sent invalid command - bug TODO: remove before submitting
                break;
        }
    }
    return 0;
}