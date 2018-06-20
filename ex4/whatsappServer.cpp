#include <sys/param.h>
#include "whatsappServer.h"
#include "whatsappio.h"

// https://gist.github.com/Alexey-N-Chernyshov/4634731 - look at shutdown_properly and close, peer_t // TODO: remove this before submission
// https://www.geeksforgeeks.org/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/ // TODO: remove this before submission


int e(int ret, const char *sysCall) {
    if (ret >= 0) return 0;
    std::cerr << "ERROR: " << sysCall << " " << std::strerror(errno) << ".\n" << std::endl;
    return 1;
}

WhatsappServer::WhatsappServer(unsigned short n) {
    portnum = n;
}

void WhatsappServer::createGroup(const Client &client, const std::string &groupName,
                                 Group &group) {
    if (contains(clients, groupName) or contains(groups, groupName)) {
        printCreateGroup(true, false, name(client), groupName);
        e(sendNameExistsSignal(fd(client)), "write");
    } else {
        group.push_back(name(client));
        removeGroupDuplicates(group);
        if (group.size() < 2 or !subset(group, clientsList)) {
            printCreateGroup(true, false, name(client), groupName);
            e(sendFailureSignal(fd(client)), "write");
            return;
        }
        groups[groupName] = group;
        printCreateGroup(true, true, name(client), groupName);
        e(sendSuccessSignal(fd(client)), "write");
    }
}

void WhatsappServer::send(const Client &client, const std::string &sendTo, const std::string &message) {
    if (contains(clients, sendTo)) { //TODO: Hagar changed below for compliance
        std::string command = "SEND:\n" + name(client) + ": " + message;
        std::cout << "here1" << "\n";

        if (e(sendData(clients[sendTo], command.c_str(), (int) command.length()), "write")) {
            e(sendFailureSignal(fd(client)), "write");
            printSend(true, false, name(client), sendTo, message);
            return;
        }
        std::cout << "here2" << "\n";

        e(sendSuccessSignal(fd(client)), "write");
        std::cout << "here3" << "\n";

        printSend(true, true, name(client), sendTo, message);
    } else if (contains(groups[sendTo], name(client))) {
        const char * msg = message.c_str();
        int len = (int) message.length();
        for (const auto& clientName : groups[sendTo]) {
            if (e(sendData(clients[clientName], msg, len), "write")) {
                e(sendFailureSignal(fd(client)), "write");
                printSend(true, false, name(client), sendTo, message);
                return;
            }
        }
        e(sendSuccessSignal(fd(client)), "write");
        printSend(true, true, name(client), sendTo, message);
    } else { // client not in group or sendTo doesn't exist
        e(sendFailureSignal(fd(client)), "write");
        printSend(true, false, name(client), sendTo, message);
    }
}

void WhatsappServer::who(const Client &client) {
    std::string whoList;
    std::sort(clientsList.begin(), clientsList.end());
    for (auto const& client_name : clientsList) {
        whoList += client_name;
        whoList += ',';
    }
    printWhoServer(name(client)); // TODO: what is the protocol here?
    e(sendData(fd(client), whoList.c_str(), (int) whoList.length()), "write");
//        e(sendFailureSignal(fd(client)), "write");
//    } else {
//        e(sendSuccessSignal(fd(client)), "write");
//    }
}

void WhatsappServer::clientExit(const Client &client) {
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

void connectNewClient(WhatsappServer& server, fd_set clients_fds)
{
    int newSocket; /* socket of connection */
    if (e(newSocket = accept(server.sockfd, NULL, NULL), "accept")) return;
    FD_SET(newSocket, &clients_fds);

    char buf[WA_MAX_NAME + 1];
    std::cout << "wen1" << "\n";
    if (e(receiveData(newSocket, buf, WA_MAX_NAME + 1), "read")) {
        e(sendFailureSignal(newSocket), "write");
        return;
    }
    std::cout << "wen2" << "\n";

    std::string newClientName {buf};

    if (contains(server.clients, newClientName) or contains(server.groups, newClientName)) {
        sendNameExistsSignal(newSocket);
    } else {
        std::cout << "perhaps2" << "\n";

        server.clients[std::string(newClientName)] = newSocket; //TODO: segfault
        server.clientsList.push_back(newClientName);
        printConnectionServer(newClientName);
        sendSuccessSignal(newSocket);
    }
}

void serverStdInput(WhatsappServer& server)
{
    char buf[10];
    std::string serverStdin;
    if (e(receiveData(STDIN_FILENO, buf, 10), "read")) return;
    serverStdin = std::string(buf);
    if (serverStdin == "EXIT") {
        server.serverExit();
    }  // otherwise, we ignore the input (according to staff)
}

void establish(WhatsappServer& server) {
    {
//        struct sockaddr_in sah;
        server.sa = new(sockaddr_in);
        char myname[MAXHOSTNAMELEN+1];

        /* hostnet initialization */
        gethostname(myname, MAXHOSTNAMELEN+1); //TODO: not sure if needed, also need to check and transfer address types.
        server.hp = gethostbyname(myname);
        if (e((server.hp == NULL), "gethostbyname")) return;

        /* sockaddrr_in initialization */
        memset(server.sa, 0, sizeof(struct sockaddr_in));
        /* this is our host address */
        memcpy(server.sa, &server.hp->h_addr, static_cast<size_t>(server.hp->h_length));

        server.sa->sin_family = server.hp->h_addrtype;
        /* this is our port number */
        server.sa->sin_port = htons(server.portnum);
        /* create socket */
        if (e(server.sockfd = socket(AF_INET, SOCK_STREAM, 0), "socket")) return;

        /* bind an address */
        if (bind(server.sockfd, (struct sockaddr *)server.sa, sizeof(struct sockaddr_in)) < 0)
        {
            e(-1, "bind");
            e(close(server.sockfd), "close");
            return;
        }

        /* listen */
        if (e(listen(server.sockfd, MAX_PENDING), "listen")) return; /* max # of queued connects */
    }
}

void handleClientRequest(WhatsappServer& server, const Client& client) {
    char buf[WA_MAX_INPUT]; // TODO: do we need +1 for null terminator? for conversion to string?
    if (e(receiveData(fd(client), buf, WA_MAX_INPUT), "read")) {
        e(sendFailureSignal(fd(client)), "write");
        return;
    }
    std::string command {buf};
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
        Clients c{};
        ClientsList g{};
        server.clients = c;
        server.clientsList = g;

        fd_set clientsfds{};
        fd_set readfds{};
        FD_ZERO(&clientsfds);
        FD_SET(server.sockfd, &clientsfds);
        FD_SET(STDIN_FILENO, &clientsfds);


        while (true) {
            readfds = clientsfds;

            std::cout << "hellllloo" << "\n";

            if ((select(MAX_CLIENTS + 1, &readfds, NULL, NULL, NULL), "select") < 0) break;
            std::cout << "here" << "\n";

            // if something happened on the master socket then it's an incoming connection
            if (FD_ISSET(server.sockfd, &clientsfds)) {
                connectNewClient(server, readfds); // will also add the client to the clientsfds
                std::cout << "here2" << "\n";

            }
            std::cout << "here3" << "\n";

            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                serverStdInput(server);
                std::cout << "here4" << "\n";

            }
            else { // will check each client if it’s in readfds and receive it's message
                std::cout << "nope" << "\n";

                for (const auto& client : server.clients)
                {
                    std::cout << "kkkk" << "\n";

                    if (FD_ISSET(fd(client) ,&readfds))
                    {
                        std::cout << "noooo" << "\n";

                        handleClientRequest(server, client);
                    }
                }
            }
        }
    return 0;
}


