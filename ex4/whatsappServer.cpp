#include "whatsappServer.h"

//WhatsappServer::WhatsappServer()
//{
//
//}

void errorHandler(int ret, const char* sysCall, const char* f) {
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


int get_connection(int s) {
    int t; /* socket of connection */
    errorHandler((t = accept(s,NULL,NULL)), "accept", "get_connection");
    return t;
}

void establish(WhatsappServer* server) { //was int
    {
        char myname[MAXHOSTNAME+1];

        /*hostnet initialization */
        gethostname(myname, MAXHOSTNAME);
        server->hp = gethostbyname(myname);
        errorHandler((server->hp == NULL),"gethostbyname", "establish");

        /*sockaddrr_in initlization */
        memset(server->sa, 0, sizeof(struct sockaddr_in));
        server->sa->sin_family = server->hp->h_addrtype;

        /* this is our host address */
        memcpy(&(server->sa->sin_addr), server->hp->h_addr, server->hp->h_length);

        /* this is our port number */
        server->sa->sin_port = htons(*server->portnum);

        /* create socket */
        errorHandler(server->sfd = socket(AF_INET, SOCK_STREAM, 0),"socket", "establish");

        if (bind(server->sfd, (struct sockaddr *)server->sa, sizeof(struct sockaddr_in)) < 0)
        {
            close(server->sfd);
            errorHandler(-1 ,"bind", "establish");
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

    int curr_socket;

    while (true)
    {
        curr_socket = get_connection(server->sfd);

    }

}
