#include <csignal>
#include "whatsappServer.h"
#include "whatsappio.h"

//WhatsappServer::WhatsappServer()
//{
//
//}

//__sighandler_t whatsappServer_exit();

// https://gist.github.com/Alexey-N-Chernyshov/4634731 - look at shutdown_properly and close, peer_t
// https://www.geeksforgeeks.org/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/

#define REQUEST_CLIENT_NAME "0"

void errorHandler(int ret, const char *sysCall, const char *f) {
    if (ret == 0) {
        return;
    }
    std::cerr << "Server: error in " << sysCall << " during " << f << std::endl;
    exit(1);
}

void WhatsappServer::create_group(ClientDesc *client, const std::string &group_name,
                                  const std::vector<std::string> &clients_group) {
}

void WhatsappServer::send(ClientDesc *client, const std::string &send_to, const std::string &message) {
}

void WhatsappServer::who(ClientDesc *client) {
}

void WhatsappServer::client_exit(ClientDesc* client) {
}

int WhatsappServer::exit() {
    return 0;
}

WhatsappServer::~WhatsappServer() = default;

int read_data(int s, char *buf, int n) {
    /* counts bytes read */
    int bcount;
    /* bytes read this pass */
    int br;

    bcount= 0;
    br = 0;
    while (bcount < n) { /* loop until full buffer */
        br = read(s, buf, n-bcount);

        if (br > 0)
        {
            bcount += br;
            if (buf[bcount] == '\n') {
                return bcount;
            }
        }
        else if ((br == 0) && (bcount >= 0)) return bcount;

        else if ((br < 1) )
        {
            return(-1);
        }

    }
    return(bcount);
}


void connectNewClient(const WhatsappServer& server)
{
    int newSocket; /* socket of connection */
    errorHandler((t = accept(server.sockfd, NULL, NULL)), "accept", "get_connection");
    std::string newClientName;
    errorHandler(send(newSocket, ))
    errorHandler(read(server.sockfd, buf, WA_MAX_NAME), "couldn't read or nothinf to read", "read_data");


}

int get_connection(int s , WhatsappServer* server) {
    int t; /* socket of connection */
    errorHandler((t = accept(s, NULL, NULL)), "accept", "get_connection");
    return t;
}

void establish(WhatsappServer& server) {
    {
        char myname[MAXHOSTNAME+1];

        /* hostnet initialization */
        gethostname(myname, MAXHOSTNAME);
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

int manage_client(ClientDesc* client) {

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

    while (server.stillRunning) {
        readfds = clientsfds;
        errorHandler(select(MAX_CLIENTS + 1, &readfds, nullptr, nullptr, nullptr), "select", "main");

        // if something happened on the master socket then it's an incoming connection
        if (FD_ISSET(server.sockfd, &readfds)) {
            connectNewClient(server); // will also add the client to the clientsfds

        }
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            serverStdInput();
        }
        else {
            // will check each client if it’s in readfds
            // and then receive a message from him
            handleClientRequest();
        }
    }
    return 0;
}

__sighandler_t whatsappServer_exit() {
    return nullptr;
}


