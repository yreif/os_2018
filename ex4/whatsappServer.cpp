#include "whatsappServer.h"
#include "whatsappio.h"

// https://gist.github.com/Alexey-N-Chernyshov/4634731 - look at shutdown_properly and close, peer_t // TODO: remove this before submission
// https://www.geeksforgeeks.org/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/ // TODO: remove this before submission


void errorHandler(int ret, const char *sysCall, const char *f) {  // TODO: terminate all clients, sockets, ...
    if (ret == 0) {
        return;
    }
    std::cerr << "Server: error in " << sysCall << " during " << f << std::endl;
    exit(1);
}

void WhatsappServer::createGroup(Client& client, const std::string& groupName,
                                  Group& group) {
    if (contains(clients, groupName) or contains(groups, groupName)) {
        printCreateGroup(true, false, name(client), groupName);
        sendNameExistsSignal(fd(client)); // TODO: add error handler
    } else { // TODO: check if name contains illegal stuff
        group.push_back(name(client));
        removeGroupDuplicates(group);
        if (group.size() < 2 or !subset(group, clientsList)) {
            printCreateGroup(true, false, name(client), groupName);
            sendFailureSignal(fd(client)); // TODO: add error handler
        }
        groups[groupName] = group;
        printCreateGroup(true, true, name(client), groupName);
        sendSuccessSignal(fd(client)); // TODO: add error handler
    }
}

void WhatsappServer::send(Client& client, const std::string &sendTo, const std::string &message) {
    if (contains(clients, sendTo)) {
        sendData(clients[sendTo], message.c_str(), (int) message.length()); // TODO: add error handle
        sendSuccessSignal(fd(client)); // TODO: add error handle
        printSend(true, true, name(client), sendTo, message);
    } else if (contains(groups[sendTo], name(client))) {
        const char * msg = message.c_str();
        int len = (int) message.length();
        for (const auto& clientName : groups[sendTo]) {
            sendData(clients[clientName], msg, len); // TODO: add error handle
        }
        sendSuccessSignal(fd(client)); // TODO: add error handle
        printSend(true, true, name(client), sendTo, message);
    } else { // client not in group or sendTo doesn't exist
        sendFailureSignal(fd(client)); // TODO: add error handle
        printSend(true, false, name(client), sendTo, message);
    }
}

void WhatsappServer::who(Client& client) {
    std::string whoList;
    printWhoServer(name(client));
    if (sendData(fd(client), whoList.c_str(), (int) whoList.length()) >= 0) {
        sendSuccessSignal(fd(client)); // TODO: add error handle
    } else {
        sendFailureSignal(fd(client)); // TODO: add error handle
    }
}

void WhatsappServer::clientExit(Client& client) {
    for (auto& group : groups) {
        group.erase(std::remove(grou)); // map iterator
    }
    printClientExit(true, name(client));
}

int WhatsappServer::exit() { // TODO: terminate all clients, sockets, ...
    return 0;
}

void connectNewClient(const WhatsappServer& server)
{
    int newSocket; /* socket of connection */
    errorHandler((newSocket = accept(server.sockfd, nullptr, nullptr)), "accept", "connectNewClient");
    char buf[WA_MAX_NAME];
    errorHandler(receiveData(server.sockfd, buf, WA_MAX_NAME), "read", "connectNewClient");
    std::string newClientName = std::string(buf);
    if (contains(server.clients, newClientName) or contains(server.groups, newClientName)) {
        sendNameExistsSignal(newSocket);
    } else { // TODO: check if name contains illegal stuff
        server.clients[newClientName] = newSocket;
        server.clientsList.push_back(newClientName);
        printConnectionServer(newClientName);
        sendSuccessSignal(newSocket);
    }
}

void serverStdInput(WhatsappServer& server)
{
    char buf[10];
    std::string serverStdin;
    receiveData(STDIN_FILENO, buf, 10);
    serverStdin = std::string(buf);
    if (serverStdin == "exit") {
        server.exit();
    }  // TODO: check updates in forum about what to do here
}

void establish(WhatsappServer& server) {
    {
        char myname[MAX_HOSTNAME+1];

        /* hostnet initialization */
        gethostname(myname, MAX_HOSTNAME);
        server.hp = gethostbyname(myname);
        errorHandler((server.hp == NULL), "gethostbyname", "establish");

        /* sockaddrr_in initialization */
        memset(server.sa, 0, sizeof(struct sockaddr_in));
        server.sa->sin_family = server.hp->h_addrtype;

        /* this is our host address */
        memcpy(&(server.sa->sin_addr), server.hp->h_addr, server.hp->h_length);

        /* this is our port number */
        server.sa->sin_port = htons(server.portnum);

        /* create socket */
        errorHandler(server.sockfd = socket(AF_INET, SOCK_STREAM, 0), "socket", "establish");

        /* bind an address */
        if (bind(server.sockfd, (struct sockaddr *)server.sa, sizeof(struct sockaddr_in)) < 0)
        {
            close(server.sockfd);
            errorHandler(-1, "bind", "establish");
        }

        /* listen */
        listen(server.sockfd, MAX_PENDING); /* max # of queued connects */
    }
}

void handleClientRequest(WhatsappServer& server, Client& client) {
    char buf[WA_MAX_INPUT]; // TODO: do we need +1 for null terminator? for conversion to string?
    receiveData(fd(client), buf, WA_MAX_INPUT); // TODO: add error handling
    std::string command = std::string(buf);
    CommandType commandT;
    std::string name, message;
    Group group;
    parseCommand(command, commandT, name, message, group);
    switch (commandT) {
            case CREATE_GROUP:
                server.createGroup(client, name, group);
            break;
            case SEND:
                server.send(client, name, message);
            break;
            case WHO:
                server.who(client);
            break;
            case EXIT:
                server.clientExit(client);
            break;
            case INVALID:
                std::cout << "BUG: client sent invalid command" << std::endl; // client sent invalid command - bug TODO: remove before submitting
            break;
        }
    }


    int main(int argc, char *argv[])
    {
//    std::vector<ClientDesc> clients; TODO: Yuval to Hagar - These are created when the server is initialized
//    std::vector<Group> groups;
        auto server = WhatsappServer();
        establish(server);
        fd_set clientsfds;
        fd_set readfds;
        FD_ZERO(&clientsfds);
        FD_SET(server.sockfd, &clientsfds);
        FD_SET(STDIN_FILENO, &clientsfds);

        while (true) {
            readfds = clientsfds;
            errorHandler(select(MAX_CLIENTS + 1, &readfds, nullptr, nullptr, nullptr), "select", "main");

            // if something happened on the master socket then it's an incoming connection
            if (FD_ISSET(server.sockfd, &readfds)) {
                connectNewClient(server); // will also add the client to the clientsfds

            }
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                serverStdInput(server);
        }
        else { // will check each client if it’s in readfds and receive it's message
            for (auto& client : server.clients)
            {
                if (FD_ISSET(fd(client) ,&readfds))
                {
                    handleClientRequest(server, client);
                }
            }
        }
    }
    return 0;
}



