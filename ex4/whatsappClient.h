#ifndef _WHATSAPPCLIENT_H
#define _WHATSAPPCLIENT_H

#include <sys/socket.h>
#include <sys/types.h>
#include <cstring>
#include <string>
#include <vector>
#include "whatsappCommon.h"
#include "whatsappio.h"


class WhatsappClient
{
public:
    int sockfd;
    struct sockaddr_in* servera;
    struct hostent *hp;
    unsigned short portnum;
    char * name;

    /**
     * sends request to create a new group named “group_name” with <list_of_client_names> as group members.
     * “group_name” is unique (i.e. no other ​ group or client​ with this name is allowed) and includes only letters and digits.
     * <list_of_client_names> is separated by comma without any spaces.
     * @param group_name
     * @param clients_group
     */
    void create_group(const std::string& group_name, const std::vector<std::string>& clients_group, const std::string &command);


    /**
     * If name ​ is a client name it sends <sender_client_name>: <message> ​only to the specified client.
     * If ​ name ​ is a group name it sends <sender_client_name>: <message> ​to all group members (except the sender client).
     * @param send_to
     * @param message
     */
    void send(const std::string& send_to, const std::string& message, const std::string &command);


    /**
     * Sends a request (to the server) to receive a list of currently connected client names (alphabetically order),
     * separated by comma without spaces.
     */
    void who(const std::string &command);


    /**
     * Unregisters the client from the server and removes it from all groups.
     * After the server unregistered the client, the client should print “Unregistered successfully” and then ​exit(0).
     */
    void exit_client(const std::string &command);

protected:
    std::vector<std::string> groups;

};


#endif //_WHATSAPPCLIENT_H
