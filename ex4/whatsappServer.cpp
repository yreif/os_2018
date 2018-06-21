#include <sys/param.h>
#include "whatsappServer.h"
#include "whatsappio.h"


int e(int ret, const char *sysCall) {
    if (ret >= 0) return 0;
    cerr << "ERROR: " << sysCall << " " << strerror(errno) << ".\n" << endl;
    return 1;
}

WhatsappServer::WhatsappServer(unsigned short n) {
    portnum = n;
}

void WhatsappServer::createGroup(const Client &client, const string &groupName, GroupUsernames &group) {
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

void WhatsappServer::send(const Client &client, const string &sendTo, const string &message) {
    string command(name(client) + ": " + message);
    string send_pop(SEND_QUE);

    if (contains(clients, sendTo)) /*Send to 1 client */{

        if (e(send_data(clients[sendTo], send_pop), "write") || e(send_data(clients[sendTo], command), "write")) {
            e(sendFailureSignal(fd(client)), "write");
            printSend(true, false, name(client), sendTo, message);
            return;
        }
        e(sendSuccessSignal(fd(client)), "write");

        printSend(true, true, name(client), sendTo, message);
        
    }
    else if (contains(groups, sendTo) && contains(groups[sendTo], name(client))) { /*Send to group */
        
        for (const auto& clientName : groups[sendTo]) 
        {
            if (clientName == name(client)) continue;

            if ((e(send_data(clients[clientName], send_pop), "write") || e(send_data(clients[clientName], command), "write"))) {
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
    string whoList;
    sort(clientsList.begin(), clientsList.end());
    for (auto const& client_name : clientsList) {
        whoList += client_name;
        whoList += ',';
    }
    whoList.pop_back();
    printWhoServer(name(client));
    e(send_data(fd(client), whoList), "write");
}

void WhatsappServer::clientExit(const Client &client) {
    FD_CLR(fd(client), &clientsfds);
    e(close(fd(client)), "close");
    for (auto& groupPair : groups) { // erase client from all groups
        auto& group = groupPair.second;
        group.erase(remove(group.begin(), group.end(), name(client)), group.end());
        if (group.empty()) {
            groups.erase(groupPair.first);
        }
    }
    clientsList.erase(remove(clientsList.begin(), clientsList.end(), name(client)), clientsList.end());
    clients.erase(name(client));
    printClientExit(true, name(client));
}

void WhatsappServer::serverExit() {
    for (auto& client : clients) {
        sendExitSignal(fd(client));
        close(fd(client));
    }
    printServerExit();
    exit(0);
}

void connectNewClient(WhatsappServer& server, fd_set& clients_fds) {
    int newSocket; /* socket of connection */
    if (e(newSocket = accept(server.sockfd, NULL, NULL), "accept")) return;
    FD_SET(newSocket, &clients_fds);

    string newClientName;
    if (e(read_data(newSocket, newClientName), "read")) {
        e(sendFailureSignal(newSocket), "write");
        return;
    }

    if (contains(server.clients, newClientName) or contains(server.groups, newClientName)) {
        sendNameExistsSignal(newSocket);
    } else {
        server.clients[newClientName] = newSocket;
        server.clientsList.push_back(newClientName);
        printConnectionServer(newClientName);
        sendSuccessSignal(newSocket);
    }
}

void serverStdInput(WhatsappServer& server) {
    string serverStdin;
    getline(cin, serverStdin);
    if (serverStdin == "EXIT") {
        server.serverExit();
    }  // otherwise, we ignore the input (according to staff)
}

void establish(WhatsappServer& server) {
    {
        server.sa = new(sockaddr_in);
        char myname[MAXHOSTNAMELEN+1];

        /* hostnet initialization */
        gethostname(myname, MAXHOSTNAMELEN+1);
        server.hp = gethostbyname(myname);
        if (e((server.hp == NULL), "gethostbyname")) return;

        /* sockaddrr_in initialization */
        memset(server.sa, 0, sizeof(struct sockaddr_in));
        /* this is our host address */
        memcpy(server.sa, &server.hp->h_addr, static_cast<size_t>(server.hp->h_length));

        server.sa->sin_family = server.hp->h_addrtype;
        server.sa->sin_addr.s_addr = INADDR_ANY;
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
    string command;
    if (e(read_data(fd(client), command), "read")) {
        e(sendFailureSignal(fd(client)), "write");
        return;
    }
    CommandType commandT;
    string name, message;
    GroupUsernames group;
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
                break;
        }
    }

int main(int argc, char *argv[]) {
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
//        Clients c{};
//        ClientsList g{};
//        server.clients = c;
//        server.clientsList = g;

        fd_set readfds{};
        FD_ZERO(&server.clientsfds);
        FD_SET(server.sockfd, &server.clientsfds);
        FD_SET(STDIN_FILENO, &server.clientsfds);
        while (true) {
            readfds = server.clientsfds;

            if ((select(MAX_CLIENTS + 1, &readfds, NULL, NULL, NULL), "select") < 0) break;

            if (FD_ISSET(server.sockfd, &readfds)) {
                connectNewClient(server, server.clientsfds);
            }
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                serverStdInput(server);
            }
            else {
                for (const auto& client : server.clients){

                    if (FD_ISSET(fd(client) ,&readfds))
                    {
                        handleClientRequest(server, client);
                    }
                }
            }
        }
    return 0;
}


