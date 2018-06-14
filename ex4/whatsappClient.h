#ifndef _WHATSAPPCLIENT_H
#define _WHATSAPPCLIENT_H

#include "whatsappio.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <cstring>
#include <string>
#include <vector>

class WhatsappClient
{
public:
    /**
     * sends request to create a new group named “group_name” with <list_of_client_names> as group members.
     * “group_name” is unique (i.e. no other ​ group or client​ with this name is allowed) and includes only letters and digits.
     * <list_of_client_names> is separated by comma without any spaces.
     * @param group_name
     * @param clients_group
     */
    void create_group(const std::string& group_name, const std::vector<std::string>& clients_group);


    /**
     * If name ​ is a client name it sends <sender_client_name>: <message> ​only to the specified client.
     * If ​ name ​ is a group name it sends <sender_client_name>: <message> ​to all group members (except the sender client).
     * @param send_to
     * @param message
     */
    void send(const std::string& send_to, const std::string& message);


    /**
     * Sends a request (to the server) to receive a list of currently connected client names (alphabetically order),
     * separated by comma without spaces.
     */
    void who();


    /**
     * Unregisters the client from the server and removes it from all groups.
     * After the server unregistered the client, the client should print “Unregistered successfully” and then ​exit(0).
     */
    void exit();

protected:
    std::vector<std::string> groups;

};


#endif //_WHATSAPPCLIENT_H