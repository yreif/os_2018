#ifndef _WHATSAPPIO_H
#define _WHATSAPPIO_H

#include <cstring>
#include <string>
#include <vector>

#define WA_MAX_NAME 30
#define WA_MAX_MESSAGE 256
#define WA_MAX_GROUP 50
#define WA_MAX_INPUT ((WA_MAX_NAME+1)*(WA_MAX_GROUP+2))

enum CommandType {CREATE_GROUP, SEND, WHO, EXIT, INVALID};

/**
 * Description: Prints to the screen a message when the user terminate the
 * server
*/
void printServerExit();

/**
 * Description: Prints to the screen a message when the client established
 * connection to the server, in the client
*/
void printConnection();

/**
 * Description: Prints to the screen a message when the client established
 * connection to the server, in the server
 * client: Name of the sender
*/
void printConnectionServer(const std::string &client);

/**
 * Description: Prints to the screen a message when the client tries to
 * use a name which is already in use
*/
void printDupConnection();

/**
 * Description: Prints to the screen a message when the client fails to
 * establish connection to the server
*/
void printFailedConnection();

/**
 * Description: Prints to the screen the usage message of the server
*/
void printServerUsage();

/**
 * Description: Prints to the screen the usage message of the client
*/
void printClientUsage();

/**
 * Description: Prints to the screen the messages of "create_group" command
 * server: true for server, false for client
 * success: Whether the operation was successful
 * client: Client name
 * group: Group name
*/
void printCreateGroup(bool server, bool success, const std::string &client, const std::string &group);

/**
 * Description: Prints to the screen the messages of "send" command
 * server: true for server, false for client
 * success: Whether the operation was successful
 * client: Client name
 * name: Name of the client/group destination of the message
 * message: The message
*/
void printSend(bool server, bool success, const std::string &client, const std::string &name,
               const std::string &message);

/**
 * Description: Prints to the screen the messages recieved by the client
 * client: Name of the sender
 * message: The message
*/
void printMessage(const std::string &client, const std::string &message);

/**
 * Description: Prints to the screen the messages of "who" command in the server
 * client: Name of the sender
*/
void printWhoServer(const std::string &client);

/**
 * Description: Prints to the screen the messages of "who" command in the client
 * success: Whether the operation was successful
 * clients: a vector containing the names of all clients
*/
void printWhoClient(bool success, const std::vector<std::string> &clients);

/**
 * Description: Prints to the screen the messages of "exit" command
 * server: true for server, false for client
 * client: Client name
*/
void printClientExit(bool server, const std::string &client);

/**
 * Description: Prints to the screen the messages of invalid command
*/
void printInvalidInput();

/**
 * Description: Prints to the screen the messages of system-call error
*/
void printError(const std::string &function_name, int error_number);

/**
 * Description: Parse user input from the argument "command". The other arguments
 * are used as output of this function.
 * command: The user input
 * commandT: The command type
 * name: Name of the client/group
 * message: The message
 * clients: a vector containing the names of all clients
*/
void parseCommand(const std::string &command, CommandType &commandT, std::string &name, std::string &messsage,
                  std::vector<std::string> &clients);

#endif
