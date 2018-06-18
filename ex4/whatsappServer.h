#ifndef _WHATSAPPSERVER_H
#define _WHATSAPPSERVER_H


#include "whatsappCommon.h"
#include <iostream>
#include <vector>
#include <unordered_map>
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
#include <cstdlib>
#include <bits/unordered_set.h>
#include <string>


//typedef std::pair<const std::string*, int > ClientDesc;
//typedef std::unordered_set<ClientDesc*> Group;

typedef std::pair<std::string, int> Client;
typedef std::unordered_map<std::string, int> Clients;
typedef std::vector<std::string> Group;
typedef std::unordered_map<std::string, Group> Groups;

//int get_connection(int s);
//
//int establish();

inline bool contains(const Clients& clients, const std::string& element) {
    return clients.find(element) != clients.cend();
}

inline bool contains(const Groups& groups, const std::string& element) {
    return groups.find(element) != groups.cend();

}
inline std::string name(const Client& client) { return client.first; }
inline int fd(const Client& client) { return client.second; }

class WhatsappServer
{
public:
    int sockfd;
    struct sockaddr_in* sa;
    struct hostent *hp;
    unsigned short portnum;
//    std::vector<ClientDesc> clients;
    Clients clients;
    Groups groups;

    /**
     * Creates a new group named “group_name” with <list_of_client_names>, client, as group members.
     * “group_name” is unique (i.e. no other ​ group or client​ with this name is allowed) and includes only letters and digits.
     * <list_of_client_names> is separated by comma without any spaces.
     * @param groupName
     * @param clientsGroup
     */
    void createGroup(Client &client, const std::string &groupName, const std::vector<std::string> &clientsGroup);


    /**
     * Sends a message from client to send_to
     * If name​ is a client name it sends <sender_client_name>: <message> ​only to the specified client.
     * If ​name ​is a group name it sends <sender_client_name>: <message> ​to all group members (except the sender client).
     * @param sendTo
     * @param message
     */
    void send(Client& client, const std::string& sendTo, const std::string& message);


    /**
     * Returns to socket a list of currently connected client names (alphabetically order),
     * separated by comma without spaces.
     */
    void who(Client& client);


    /**
     * Unregisters the client from the server and removes it from all groups.
     * After the server unregistered the client, the client should print “Unregistered successfully” and then ​exit(0).
     */
    void clientExit(Client &client);

    int exit();


    ~WhatsappServer();

};


#endif //_WHATSAPPSERVER_H
