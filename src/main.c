#include "lab.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#ifdef TEST
#define main main_exclude
#endif

int check_socket_message(int sockfd, int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN; // Check for incoming data
    pfd.revents = 0;

    // Call poll with a set timeout to check if the socket has data available
    int ret = poll(&pfd, 1, timeout_ms);

    if (ret > 0 && (pfd.revents & POLLIN)) {
        return 1; // Message/data is waiting
    }
    
    return 0; // No message waiting or error
}

int trigger_server_response(int expected_code, int socket_fd, int timeout_ms) {
    char response[4096];
    char *response_full_str = malloc( sizeof(char)* 4096 + 1);
    while (check_socket_message(socket_fd, timeout_ms) == 1) {
        ssize_t response_length = recv(socket_fd, response, sizeof(response), 0);
        if (response_length == -1) {
            if (errno == EINTR) {
                return trigger_server_response(expected_code, socket_fd, timeout_ms);
            }
            perror("recv");
            return 1;
        }
        size_t response_length_size_t = (size_t) response_length; // Can do this because already checked for -1 error case
        if (response_length == 0) {
            fprintf(stderr, "Server closed the response connection.\n");
            return ECANCELED;
        }
        if ( ends_with_crlf(response, response_length_size_t) == 1 ) {
            int response_result = proccess_server_response(expected_code, response, response_length_size_t, stdout);
            if (response_result != 0) {
                if (response_result == ECANCELED) {
                fprintf(stderr, "Server did not respond with %d on connection.\n", expected_code);
                } else {
                    fprintf(stderr, "Failed to parse server response on connection.\n");
                }
            }
            if (response_full_str == NULL) {
                fprintf(stderr, "Failed to allocate memory for combined response.\n");
                return ENOTRECOVERABLE;
            }
            strcpy(response_full_str, response);

        } else {
            // Combine the current response with the next one to ensure we have a complete response ending with CRLF
            if (response_full_str == NULL) {
                fprintf(stderr, "Failed to allocate memory for combined response.\n");
                return ENOTRECOVERABLE;
            }
            strcpy(response_full_str, response);
        }
    }
    if (ends_with_crlf(response_full_str, strlen(response_full_str)) == 0 ) {
        fprintf(stderr, "Server response did not end with CRLF.\n");
        free(response_full_str);
        return ENOTRECOVERABLE;
    }
    free(response_full_str);
    return 0;
}

/*
int trigger_server_response(int expected_code, int socket_fd) {
    char response[4096];
    struct pollfd server_socket = {socket_fd, POLLIN, 0};
    int poll_timeout = -1;
    int poll_result;

    for (;;) {
        poll_result = poll(&server_socket, 1, poll_timeout);
        if (poll_result == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            return 1;
        }
        if (poll_result == 0) {
            return 0;
        }
        // Makes sure the socket is still readable for the loop to terminate if it's not.
        if ((server_socket.revents & POLLIN) == 0) {
            fprintf(stderr, "Server response socket is no longer readable.\n");
            return 1;
        }

        ssize_t response_length = recv(socket_fd, response, sizeof(response), 0);
        if (response_length == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            return 1;
        }
        if (response_length == 0) {
            fprintf(stderr, "Server closed the response connection.\n");
            return 1;
        }

        int response_result = proccess_server_response(expected_code, response,
                                                        response_length, stdout);
        if (response_result != 0) {
            if (response_result == ECANCELED) {
                fprintf(stderr, "Server did not respond with %d on connection.\n", expected_code);
            } else {
                fprintf(stderr, "Failed to parse server response on connection.\n");
            }
            return response_result;
        }

        // Drain any additional responses already waiting without blocking.
        poll_timeout = 0;
    }
}
*/

int cleanup(int socket_fd, Message *message, int error_code) {
    close(socket_fd);
    message_free(message);
    return error_code;
}

void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s -f <from> -t <to> [-s subject] [-b body] [-p port] [-H helo-host] <server>\n", progname);
    fprintf(stderr, "  -f <from>       envelope sender, for example you@example.com\n");
    fprintf(stderr, "  -t <to>         envelope recipient\n");
    fprintf(stderr, "  -s <subject>    subject line (default: empty)\n");
    fprintf(stderr, "  -b <body>       message body (default: read from stdin)\n");
    fprintf(stderr, "  -p <port>       port or service name (default: 25)\n");
    fprintf(stderr, "  -H <helo-host>  host name sent with HELO (default: localhost)\n");
    fprintf(stderr, "  <server>        host name or address of the mail server\n");
}

/*
 * @brief The main entry point for the SMTP client.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, non-zero on failure.
 * 
 * @use Usage: myapp -f <from> -t <to> [-s subject] [-b body] [-p port]
          [-H helo-host] <server>

  -f <from>       envelope sender, for example you@example.com
  -t <to>         envelope recipient
  -s <subject>    subject line (default: empty)
  -b <body>       message body (default: read from stdin)
  -p <port>       port or service name (default: 25)
  -H <helo-host>  host name sent with HELO (default: localhost)
  <server>        host name or address of the mail server
 */
int main(int argc, char *argv[]) {
    Message message = message_arg_parser(argc, argv);
    // Message validation: Ensure required fields are present
    if(message.from == NULL || message.to == NULL || message.server == NULL) {
        print_usage(argv[0]);
        message_free(&message);
        return 1;
    }
    
    //TCP connection setup
    struct addrinfo hints = {0};
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    char port[sizeof("65535")];
    int socket_fd = -1;
    int address_result;
    
    // Format the server port into a string for getaddrinfo
    if (snprintf(port, sizeof(port), "%d", message.port) < 0) {
        fprintf(stderr, "Could not format the server port.\n");
        message_free(&message);
        return 1;
    }

    // Get address info for the server
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    address_result = getaddrinfo(message.server, port, &hints, &addresses);
    if (address_result != 0) {
        fprintf(stderr, "Could not resolve %s: %s\n", message.server,
                gai_strerror(address_result));
        message_free(&message);
        return 1;
    }
    // Try each address until we find one that works
    for (address = addresses; address != NULL; address = address->ai_next) {
        socket_fd = socket(address->ai_family, address->ai_socktype,
                           address->ai_protocol);
        if (socket_fd == -1) {
            continue;
        }
        // Attempt to connect to server and break if successful
        if (connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }
        close(socket_fd);
        socket_fd = -1;
    }
    freeaddrinfo(addresses);
    // Make sure the socket exists
    if (socket_fd == -1) {
        perror("connect");
        message_free(&message);
        return 1;
    }
    //Begin Client Server interaction
    int exit_value;
    // Expecting 220 response code from server for successful connection
    exit_value = trigger_server_response(220, socket_fd, 5000);
    if (exit_value != 0) {
        return cleanup(socket_fd, &message, exit_value);
    }
    //Client Initiation (HELO)
    char *computer_domain = get_computer_domain();
    if (computer_domain == NULL) {
        fprintf(stderr, "Could not identify the computer's domain.\n");
        message_free(&message);
        return 1;
    }
    char *helo_command = format_command("HELO", computer_domain);
    free(computer_domain);
    if (helo_command == NULL) {
        fprintf(stderr, "Could not format HELO command.\n");
        return cleanup(socket_fd, &message, ENOMEM);
    }
    exit_value = (int)send(socket_fd, helo_command, strlen(helo_command), 0);
    free(helo_command);
    if (exit_value < 0) {
        printf("Error: Failed to send HELO command.\n");
        return cleanup(socket_fd, &message, exit_value);
    }
    // Expecting 250 response code from server for successful connection
    exit_value = trigger_server_response(250, socket_fd, 5000);
    if (exit_value != 0) {
        printf("Error: Server did not respond with 250 after HELO command.\n");
        return cleanup(socket_fd, &message, exit_value);
    }
    

    // Close the socket and cleanup
    return cleanup(socket_fd, &message, 0);
}