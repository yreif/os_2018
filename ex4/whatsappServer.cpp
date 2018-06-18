#include "whatsappServer.h"
#include "whatsappio.h"

// https://gist.github.com/Alexey-N-Chernyshov/4634731 - look at shutdown_properly and close, peer_t // TODO: remove this before submission
// https://www.geeksforgeeks.org/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/ // TODO: remove this before submission


int error(int ret, const char *sysCall) {
    if (ret == 0) return -1;
    std::cerr << "ERROR: " << sysCall << " " << std::strerror(errno) << ".\n" << std::endl;
    return 0;
}

WhatsappServer::WhatsappServer(unsigned short n) {
    portnum = n;
}

void WhatsappServer::createGroup(Client& client, const std::string& groupName,
                                  Group& group) {
    if (contains(clients, groupName) or contains(groups, groupName)) {
        printCreateGroup(true, false, name(client), groupName);
        error(sendNameExistsSignal(fd(client)), "write");
    } else {
        group.push_back(name(client));
        removeGroupDuplicates(group);
        if (group.size() < 2 or !subset(group, clientsList)) {
            printCreateGroup(true, false, name(client), groupName);
            error(sendFailureSignal(fd(client)), "write");
            return;
        }
        groups[groupName] = group;
        printCreateGroup(true, true, name(client), groupName);
        error(sendSuccessSignal(fd(client)), "write");
    }
}

void WhatsappServer::send(Client& client, const std::string &sendTo, const std::string &message) {
    if (contains(clients, sendTo)) {
        if (error(sendData(clients[sendTo], message.c_str(), (int) message.length()), "write")) {
            error(sendFailureSignal(fd(client)), "write");
            printSend(true, false, name(client), sendTo, message);
            return;
        }
        error(sendSuccessSignal(fd(client)), "write");
        printSend(true, true, name(client), sendTo, message);
    } else if (contains(groups[sendTo], name(client))) {
        const char * msg = message.c_str();
        int len = (int) message.length();
        for (const auto& clientName : groups[sendTo]) {
            if (error(sendData(clients[clientName], msg, len), "write")) {
                error(sendFailureSignal(fd(client)), "write");
                printSend(true, false, name(client), sendTo, message);
                return;
            }
        }
        error(sendSuccessSignal(fd(client)), "write");
        printSend(true, true, name(client), sendTo, message);
    } else { // client not in group or sendTo doesn't exist
        error(sendFailureSignal(fd(client)), "write");
        printSend(true, false, name(client), sendTo, message);
    }
}

void WhatsappServer::who(Client& client) {
    std::string whoList;
    std::sort(clientsList.begin(), clientsList.end());
    for (auto const& client : clientsList) {
        whoList += client;
        whoList += ',';
    }
    printWhoServer(name(client));
    if (error(sendData(fd(client), whoList.c_str(), (int) whoList.length()), "write")) {
        error(sendFailureSignal(fd(client)), "write");

    } else {
        error(sendSuccessSignal(fd(client)), "write");
    }
}

void WhatsappServer::clientExit(Client& client) {
    for (auto& groupPair : groups) { // erase client from all groups
        auto& group = groupPair.second;
        group.erase(std::remove(group.begin(), group.end(), name(client)), group.end());
    }
    clientsList.erase(std::remove(clientsList.begin(), clientsList.end(), name(client)), clientsList.end());
    clients.erase(name(client));
    printClientExit(true, name(client)); // TODO: who closes the socket?
}

void WhatsappServer::serverExit() { // TODO: terminate all sockets?
    for (auto& client : clients) sendExitSignal(fd(client));
    printServerExit();
    exit(0);
}

void connectNewClient(const WhatsappServer& server)
{
    int newSocket; /* socket of connection */
    if (error(newSocket = accept(server.sockfd, nullptr, nullptr), "accept")) return;
    char buf[WA_MAX_NAME];
    if (error(receiveData(server.sockfd, buf, WA_MAX_NAME), "read")) {
        error(sendFailureSignal(newSocket), "write");
        return;
    }
    std::string newClientName = std::string(buf);
    if (contains(server.clients, newClientName) or contains(server.groups, newClientName)) {
        sendNameExistsSignal(newSocket);
    } else {
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
    if (error(receiveData(STDIN_FILENO, buf, 10), "read")) return;
    serverStdin = std::string(buf);
    if (serverStdin == "EXIT") {
        server.serverExit();
    }  // otherwise, we ignore the input (according to staff)
}

void establish(WhatsappServer& server) {
    {
        char myname[MAX_HOSTNAME+1];

        /* hostnet initialization */
        gethostname(myname, MAX_HOSTNAME);
        server.hp = gethostbyname(myname);
        if (error((server.hp == NULL), "gethostbyname")) return;

        /* sockaddrr_in initialization */
        memset(server.sa, 0, sizeof(struct sockaddr_in));
        server.sa->sin_family = server.hp->h_addrtype;

        /* this is our host address */
        memcpy(&(server.sa->sin_addr), server.hp->h_addr, server.hp->h_length);

        /* this is our port number */
        server.sa->sin_port = htons(server.portnum);

        /* create socket */
        if (error(server.sockfd = socket(AF_INET, SOCK_STREAM, 0), "socket")) return;

        /* bind an address */
        if (bind(server.sockfd, (struct sockaddr *)server.sa, sizeof(struct sockaddr_in)) < 0)
        {
            error(-1, "bind");
            error(close(server.sockfd), "close");
            return;
        }

        /* listen */
        if (error(listen(server.sockfd, MAX_PENDING), "listen")) return; /* max # of queued connects */
    }
}

void handleClientRequest(WhatsappServer& server, Client& client) {
    char buf[WA_MAX_INPUT]; // TODO: do we need +1 for null terminator? for conversion to string?
    if (error(receiveData(fd(client), buf, WA_MAX_INPUT), "read")) {
        error(sendFailureSignal(fd(client)), "write");
        return;
    }
    std::string command = std::string(buf);
    CommandType commandT;
    std::string name, message;
    Group group;
    parseCommand(command, commandT, name, message, group); // TODO: fix their parse command
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
        if (argc != 2) {
            printServerUsage();
            return 0;
        }
        unsigned short portnum;
        if (!toUnsignedShort(argv[1], portnum)) {
            printServerUsage();
            return 0;
        }
        auto server = WhatsappServer(portnum);
        establish(server);
        fd_set clientsfds;
        fd_set readfds;
        FD_ZERO(&clientsfds);
        FD_SET(server.sockfd, &clientsfds);
        FD_SET(STDIN_FILENO, &clientsfds);

        while (true) {
            readfds = clientsfds;
            if (error(select(MAX_CLIENTS + 1, &readfds, nullptr, nullptr, nullptr), "select")) continue;

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
}



