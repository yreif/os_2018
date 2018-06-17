#ifndef _WHATSAPPSERVER_H
#define _WHATSAPPSERVER_H

#include <iostream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include <stdlib.h>
#include <bits/unordered_set.h>

#define MAXHOSTNAME 30
#define MAX_PENDING 10
#define MAX_CLIENTS 30

typedef std::pair<const std::string*, int > ClientDesc;
typedef std::unordered_set<ClientDesc*> Group;

//int get_connection(int s);
//
//int establish();


class WhatsappServer
{
public:
    int sockfd;
    struct sockaddr_in* sa;
    struct hostent *hp;
    unsigned short portnum;
    bool stillRunning = true;
//    std::vector<ClientDesc> clients;
    std::map<const std::string, int> clients;
    std::vector<Group> groups;

    /**
     * Creates a new group named “group_name” with <list_of_client_names>, client, as group members.
     * “group_name” is unique (i.e. no other ​ group or client​ with this name is allowed) and includes only letters and digits.
     * <list_of_client_names> is separated by comma without any spaces.
     * @param group_name
     * @param clients_group
     */
    void create_group(ClientDesc* client, const std::string& group_name, const std::vector<std::string>& clients_group);


    /**
     * Sends a message from client to send_to
     * If name​ is a client name it sends <sender_client_name>: <message> ​only to the specified client.
     * If ​name ​is a group name it sends <sender_client_name>: <message> ​to all group members (except the sender client).
     * @param send_to
     * @param message
     */
    void send(ClientDesc* client, const std::string& send_to, const std::string& message);


    /**
     * Returns to socket a list of currently connected client names (alphabetically order),
     * separated by comma without spaces.
     */
    void who(ClientDesc* client);


    /**
     * Unregisters the client from the server and removes it from all groups.
     * After the server unregistered the client, the client should print “Unregistered successfully” and then ​exit(0).
     */
    void client_exit(ClientDesc* client);


    int exit();


    ~WhatsappServer();

};


#endif //_WHATSAPPSERVER_H
