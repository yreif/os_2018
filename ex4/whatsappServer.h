#ifndef _WHATSAPPSERVER_H
#define _WHATSAPPSERVER_H


#include "whatsappCommon.h"
#include <vector>
#include <algorithm> // for std::find used in vector contains, for std::sort and std::include
#include <map>

typedef std::pair<std::string, int> Client;
typedef std::map<std::string, int> Clients;
typedef std::vector<std::string> ClientsList;
typedef std::vector<std::string> GroupUsernames;
typedef std::map<std::string, GroupUsernames> Groups;



inline bool contains(const Clients& clients, const std::string& element) {
    clients.find(element);
    return clients.find(element) != clients.end();
}

inline bool contains(const Groups& groups, const std::string& element) {
    return groups.find(element) != groups.end();
}

inline bool contains(const GroupUsernames& groupUsernames, const std::string& element) {
    return find(groupUsernames.begin(), groupUsernames.end(), element) != groupUsernames.end();
}

bool subset(GroupUsernames& groupUsernames, ClientsList& clientsList) {
    std::sort(groupUsernames.begin(), groupUsernames.end());
    std::sort(clientsList.begin(), clientsList.end());
    return std::includes(clientsList.begin(), clientsList.end(),
                         groupUsernames.begin(), groupUsernames.end());
}

inline std::string name(const Client& client) { return client.first; }
inline int fd(const Client& client) { return client.second; }

void removeGroupDuplicates(GroupUsernames& group) {

    std::sort(group.begin(), group.end());
    group.erase(std::unique(group.begin(), group.end()), group.end());
}

class WhatsappServer {
public:
    int sockfd;
    struct sockaddr_in* sa = nullptr;
    struct hostent *hp = nullptr;
    unsigned short portnum;
    Clients clients;
    Groups groups;
    ClientsList clientsList;
    fd_set clientsfds;

    explicit WhatsappServer(unsigned short portnum);

    /**
     * Creates a new group named “group_name” with <list_of_client_names>, client, as group members.
     * “group_name” is unique (i.e. no other ​ group or client​ with this name is allowed) and includes only letters and digits.
     * <list_of_client_names> is separated by comma without any spaces.
     * @param groupName
     * @param clientsGroup
     */
    void createGroup(const Client &client, const std::string &groupName, std::vector<std::string> &clientsGroup);


    /**
     * Sends a message from client to send_to
     * If name​ is a client name it sends <sender_client_name>: <message> ​only to the specified client.
     * If ​name ​is a group name it sends <sender_client_name>: <message> ​to all group members (except the sender client).
     * @param sendTo
     * @param message
     */
    void send(const Client &client, const std::string &sendTo, const std::string &message);


    /**
     * Returns to socket a list of currently connected client names (alphabetically order),
     * separated by comma without spaces.
     */
    void who(const Client &client);


    /**
     * Unregisters the client from the server and removes it from all groups.
     * After the server unregistered the client, the client should print “Unregistered successfully” and then ​exit(0).
     */
    void clientExit(const Client &client);

    void serverExit();
};


#endif //_WHATSAPPSERVER_H
