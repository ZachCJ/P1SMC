#ifndef LAB_H
#define LAB_H

#include <stdio.h>

/**
 * @file lab.h
 * @author Zach Johnston
 * @date 9-18-26
 * @brief Contains structs and function prototypes for message handling, and SMTP client helper functions.
 */

 /**
  * @brief Reads the input from the response; parses the response code and checks if it matches the expected code.
  * @param expected_code The expected response code to match against.
  * @param response The pointer to theresponse string to parse.
  * @param response_length The length of the response string.
  * @param output_target The file to write the response to.
  * @return 0 if the response code matches the expected code, ECANCELED if it does not match indicating a response error, 
  *     or ENOTRECOVERABLE if the response fails to parse for any reason.
  */
int proccess_server_response(int expected_code, char *response, size_t response_length, FILE *output_target);

/**
 * @struct Message
 * @brief Represents an email message with associated fields. 
 */
typedef struct Message {
    char *from;
    char *to;
    char *subject ;
    char *body;
    int port;
    char *hostname;
    char *server;
} Message;

/**
 * @brief Parses SMTP client options into a message.
 *
 * argv includes the program name at index zero. Supported options are
 * -f/--from, -t/--to, -s/--subject, -b/--body, -p/--port, and
 * -H/--helo-host. The mail server is the required positional argument.
 * Defaults are port 25 and hostname localhost. Invalid input returns a
 * zeroed message with port set to -1.
 */
Message message_arg_parser(int size, char *argv[]);

/** 
 * @brief Serializes a message as a shell-safe argument string. 
 * @param msg The message to serialize.
 * @return A string representing the serialized message, or NULL on failure.
 */
char *message_to_string(const Message *msg);

/**
 * @brief Releases all strings owned by a message and resets it.
 * @param msg The message to free.
 */
void message_free(Message *msg);

/**
 * @brief Checks if a string ends with CRLF.
 * @param str The string to check.
 * @param len The length of the string.
 * @return 1 if the string ends with CRLF, 0 otherwise.
 */
int ends_with_crlf(const char *str, size_t len);

/**
 * @brief Finds the computer's canonical host name.
 * @return A dynamically allocated canonical host name, or NULL on failure.
 */
char *get_computer_domain(void);

/**
 * @brief Formats a command with an argument into a string.
 * @param command The command to format.
 * @param argument The argument to include in the command.
 * @return A string representing the formatted command, or NULL on failure.
 */
char *format_command(const char *command, const char *argument);

typedef char *(*FormatCommand)(const char *command, const char *argument);
typedef int (*EndsWithCRLF)(const char *str, size_t len);
typedef int (*ProcessServerResponse)(int expected_code, char *response, size_t response_length, FILE *output_target);
typedef Message (*MessageArgParser)(int size, char *argv[]);
typedef char *(*MessageToString)(const Message *msg);


#endif // LAB_H
