#include <csignal>
#include "whatsappServer.h"
#include "whatsappio.h"

//WhatsappServer::WhatsappServer()
//{
//
//}

//__sighandler_t whatsappServer_exit();

void errorHandler(int ret, const char* sysCall, const char* f, WhatsappServer* server) {
    if (ret == 0) {
        return;
    }
    std::cerr << "Server: error in " << sysCall << " during " << f << std::endl;
    exit(1);
}



//WhatsappServer::WhatsappServer(int sfd, struct sockaddr_in &sa, struct hostent &hp, unsigned short &portnum,
//                               std::vector<ClientDesc> &clients) {
//    sfd = sfd;
//    sa = sa;
//    hp = hp;
//    portnum = portnum;
//    clients = clients;
//
//}

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

WhatsappServer::~WhatsappServer() {

}

int read_data(int s, char *buf, int n) {
    int bcount;
/* counts bytes read */
    int br;
/* bytes read this pass */
    bcount= 0;
    br= 0;
    while (bcount < n) { /* loop until full buffer */
        br = read(s, buf, n-bcount);

        if (br > 0)
        {
            bcount += br;
            if (buf[bcount] == '\n'){
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


ClientDesc* connectNewClient(int s , WhatsappServer* server)
{
    int t; /* socket of connection */
    errorHandler((t = accept(s,NULL,NULL)), "accept", "get_connection", server);
    ClientDesc client;
    char * buf = new(char);
    errorHandler(read(s, buf, WA_MAX_NAME), "couldn't read or nothinf to read", "read_data", server);
    client.first = buf;
    client.second = t;
    return &client

}

int get_connection(int s , WhatsappServer* server) {
    int t; /* socket of connection */
    errorHandler((t = accept(s,NULL,NULL)), "accept", "get_connection", server);
    return t;
}

void establish(WhatsappServer* server) { //was int
    {
        char myname[MAXHOSTNAME+1];

        /*hostnet initialization */
        gethostname(myname, MAXHOSTNAME);
        server->hp = gethostbyname(myname);
        errorHandler((server->hp == NULL),"gethostbyname", "establish", server);

        /*sockaddrr_in initlization */
        memset(server->sa, 0, sizeof(struct sockaddr_in));
        server->sa->sin_family = server->hp->h_addrtype;

        /* this is our host address */
        memcpy(&(server->sa->sin_addr), server->hp->h_addr, server->hp->h_length);

        /* this is our port number */
        server->sa->sin_port = htons(*server->portnum);

        /* create socket */
        errorHandler(server->sfd = socket(AF_INET, SOCK_STREAM, 0),"socket", "establish", server);

        if (bind(server->sfd, (struct sockaddr *)server->sa, sizeof(struct sockaddr_in)) < 0)
        {
            close(server->sfd);
            errorHandler(-1 ,"bind", "establish", server);
        }

        listen(server->sfd, MAX_PENDING); /* max # of queued connects */
//        return(server->sfd);
    }
}


int manage_client(ClientDesc* client)
{

}

int main(int argc, char *argv[])
{
    std::vector<ClientDesc> clients;
    std::vector<Group> groups;
    WhatsappServer* server = new(WhatsappServer);
    server->clients = &clients;
    server->groups = &groups;
    establish(server);

    fd_set clientsfds;
    fd_set readfds;
    FD_ZERO(&clientsfds);
    FD_SET(server->sfd, &clientsfds);
    FD_SET(STDIN_FILENO, &clientsfds);

//    signal(SIGINT, whatsappServer_exit());

    while (true) {
        readfds = clientsfds;
        errorHandler(select(MAX_CLIENTS + 1, &readfds, NULL, NULL, NULL) ,"select", "main", server);

        if (FD_ISSET(server->sfd, &readfds)) {
            //will also add the client to the clientsfds
            get_connection();
            connectNewClient();
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            serverStdInput();
        }
        else {
            //will check each client if it’s in readfds
            //and then receive a message from him
            handleClientRequest();
        }
    }
    return 0;
}

__sighandler_t whatsappServer_exit() {
    return nullptr;
}


