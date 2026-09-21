#include "lab.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>


/**
 * @brief Reads the input from the response; parses the response code and checks if it matches the expected code.
 * @param expected_code The expected response code to match against.
 * @param response The pointer to theresponse string to parse.
 * @param response_length The length of the response string.
 * @param output_target The file to write the response to.
 * @return 0 if the response code matches the expected code, ECANCELED if it does not match indicating a response error, 
 *     or ENOTRECOVERABLE if the response fails to parse for any reason.
 */
int proccess_server_response(int expected_code, char *response, size_t response_length, FILE *output_target) {

	if (response == NULL || response_length <= 0) {
		return ENOTRECOVERABLE;
	}

	// Write the response to the output target
	if (output_target != NULL) {
		fwrite("S: ", 1, strlen("S: "), output_target);
		fwrite(response, 1, response_length, output_target);
		fwrite("\n", 1, 1, output_target);
		fflush(output_target);
	}

	// Parse the response code from the response string
	char *endptr;
	long code = strtol(response, &endptr, 10);
	if (endptr == response || *endptr != ' ' || code < 100 || code > 599) {
		return ENOTRECOVERABLE;
	}

	// Check if the parsed code matches the expected code
	if ((int)code != expected_code) {
		return ECANCELED;
	}

	//Check if the response ends with CRLF
	if (ends_with_crlf(response, response_length) == 0) {
		return ENOTRECOVERABLE;
	}

	return 0; // Success
}

//Format Command 
char *format_command(const char *command, const char *argument) {
	size_t command_length = strlen(command);
	size_t argument_length = argument ? strlen(argument) : 0;
	size_t total_length = command_length + 1 + argument_length + 2; // +1 for space, +2 for CRLF

	char *formatted_command = malloc(total_length + 1); // +1 for null terminator
	if (!formatted_command) {
		return NULL; // Memory allocation failed
	}

	if (argument) {
		snprintf(formatted_command, total_length, "%s %s\r\n", command, argument);
	} else {
		snprintf(formatted_command, total_length, "%s\r\n", command);
	}

	return formatted_command;
}

// Checks if an char array with a given length ends with CRLF (\r\n) and returns 1 if it does, or 0 if it does not.
int ends_with_crlf(const char *str, size_t len) {
	if (len < 2) {
		return 0;
	}
	return (str[len - 2] == '\r' && str[len - 1] == '\n');
}



/////////////////////////////////////////////
//		Arg Parsing helper functions	   //
/////////////////////////////////////////////

/**
 * @brief Copy a string to a new memory location.
 * @param value The string to copy.
 * @return A pointer to the new string, or NULL if allocation fails.
 */
static char *copy_string(const char *value)
{
	size_t length;
	char *copy;

	if (value == NULL) {
		return NULL;
	}
	length = strlen(value) + 1;
	copy = malloc(length);
	if (copy != NULL) {
		memcpy(copy, value, length);
	}
	return copy;
}

/**
 * @brief Parse a string as a port number.
 * @param value The string to parse.
 * @param port A pointer to the integer to store the parsed port.
 * @return 1 on success, 0 on failure.
 */
static int parse_port(const char *value, int *port)
{
	char *end;
	long parsed;

	if (value == NULL || value[0] == '\0') {
		return 0;
	}
	errno = 0;
	parsed = strtol(value, &end, 10);
	if (errno != 0 || *end != '\0' || parsed < 1L || parsed > 65535L) {
		return 0;
	}
	*port = (int) parsed;
	return 1;
}

/**
 * @brief Set the value of a string pointer.
 * @param destination A pointer to the string pointer to set.
 * @param value The value to set.
 * @return 1 on success, 0 on failure.
 */
static int set_value(char **destination, const char *value)
{
	char *copy = copy_string(value);

	if (copy == NULL) {
		return 0;
	}
	free(*destination);
	*destination = copy;
	return 1;
}

/**
 * Buffered stdin reader for filling out the message body.
 * 		-Uses enter to terminate the input. 
 * 
 * @brief Read the entire contents of stdin into a dynamically allocated string.
 * @return A pointer to the string, or NULL if allocation fails.
 */
static char *read_stdin(void) {
	
	size_t buffer_size = 1024;
	size_t length = 0;
	char *buffer = malloc(buffer_size);
	if (buffer == NULL) {
		return NULL;
	}

	int c;
	while ((c = fgetc(stdin)) != '\n') {
		if (length + 1 >= buffer_size) {
			buffer_size *= 2;
			char *new_buffer = realloc(buffer, buffer_size);
			if (new_buffer == NULL) {
				free(buffer);
				return NULL;
			}
			buffer = new_buffer;
		}
		buffer[length++] = (char)c;
	}
	buffer[length] = '\0';
	return buffer;
}

/**
 * Frees resources and set the port to -1 to indicate an error occurred during parsing. 
 * 
 * @brief Handle parsing errors by freeing allocated resources and setting the port to -1.
 * @param msg The Message struct to free.
 * @return A Message struct with the port set to -1.
 */
static Message parseError(Message msg) {
	message_free(&msg);
	msg.port = -1;
	return msg;
}



//////////////////////////////////////////////
// 		Message Formation and Parsing		//
//////////////////////////////////////////////

/**
 * @brief Parse command-line arguments into a Message struct.
 * @param size The number of arguments.
 * @param argv The array of command-line arguments.
 * @return A Message struct initialized with the parsed arguments.
 */
Message message_arg_parser(int size, char *argv[])
{
	Message message = {0};
	int index;
	//Sets defaults
	message.port = 25;
	message.hostname = copy_string("localhost");
	//Check initialization
	if (size < 1 || argv == NULL || message.hostname == NULL) {
		return parseError(message);
	}
	//Parsing loop for command line arguments, checks for valid input and sets the message struct accordingly
	for (index = 1; index < size; index++) {
		const char *option = argv[index];
		const char *value;

		if (option == NULL) {
			return parseError(message);
		}
		//Checks for options
		if (strcmp(option, "--") == 0) {
			if (++index >= size || argv[index] == NULL || message.server != NULL) {
				return parseError(message);
			}
			message.server = copy_string(argv[index]);
			if (message.server == NULL) {
				return parseError(message);
			}
			continue;
		}
		if (option[0] != '-' || option[1] == '\0' || index + 1 >= size) {
			if (message.server != NULL || option[0] == '-') {
				return parseError(message);
			}
			message.server = copy_string(option);
			if (message.server == NULL) {
				return parseError(message);
			}
			continue;
		}
		value = argv[++index];
		// Sets message values based on the option found above, if the option doesn't exist or fails to set the value: the parse returns an error Message state. 
		if (strcmp(option, "--from") == 0 || strcmp(option, "-f") == 0) {
			if (!set_value(&message.from, value)) {
				return parseError(message);
			}
		} else if (strcmp(option, "--to") == 0 || strcmp(option, "-t") == 0) {
			if (!set_value(&message.to, value)) {
				return parseError(message);
			}
		} else if (strcmp(option, "--subject") == 0 || strcmp(option, "-s") == 0) {
			if (!set_value(&message.subject, value)) {
				return parseError(message);
			}
		} else if (strcmp(option, "--body") == 0 || strcmp(option, "-b") == 0) {
			if (!set_value(&message.body, value)) {
				return parseError(message);
			}
		} else if (strcmp(option, "--helo-host") == 0 || strcmp(option, "-H") == 0) {
			if (!set_value(&message.hostname, value)) {
				return parseError(message);
			}
		} else if (strcmp(option, "--port") == 0 || strcmp(option, "-p") == 0) {
			if (!parse_port(value, &message.port)) {
				return parseError(message);
			}
		} else {
			return parseError(message);
		}
	}
	//Checks if the mesage is valid, if not free the message and set port to -1 to indicate error
	if (message.from == NULL || message.to == NULL || message.server == NULL) {
		return parseError(message);
	}
	// STDIN reading for if body is not provided
	if(!message.body) {
		message.body = read_stdin();
	}
	return message;
}

char *message_to_string(const Message *msg)
{
	const char *values[6];
	const char *options[6] = {"-f", "-t", "-s", "-b",
							  "-H", "-S"};
	size_t required = 32;
	size_t used = 0;
	size_t index;
	char *result;

	if (msg == NULL || msg->port < 1 || msg->port > 65535) {
		return NULL;
	}
  //Loads msg into array for string assembly
	values[0] = msg->from;
	values[1] = msg->to;
	values[2] = msg->subject;
	values[3] = msg->body;
	values[4] = msg->hostname;
	values[5] = msg->server;
	for (index = 0; index < 6; index++) {
		if (values[index] != NULL) {
			required += strlen(options[index]) + strlen(values[index]) + 3;
		}
	}
	result = malloc(required);
	if (result == NULL) {
		return NULL;
	}
	for (index = 0; index < 6; index++) {
		int written;

		if (values[index] == NULL) {
			continue;
		}
		written = snprintf(result + used, required - used, "%s '%s' ", options[index], values[index]);
		if (written < 0 || (size_t) written >= required - used) {
			free(result);
			return NULL;
		}
		used += (size_t) written;
	}
	if (snprintf(result + used, required - used, "-p %d", msg->port) < 0) {
		free(result);
		return NULL;
	}
	return result;
}

void message_free(Message *msg)
{
	if (msg == NULL) {
		return;
	}
	free(msg->from);
	free(msg->to);
	free(msg->subject);
	free(msg->body);
	free(msg->hostname);
	free(msg->server);
	*msg = (Message){0};
	msg->port = -1;
}

char *get_computer_domain(void) {
	char hostname[256];
	struct addrinfo hints = {0};
	struct addrinfo *addresses = NULL;
	char *domain;
	int result;

	if (gethostname(hostname, sizeof(hostname)) != 0) {
		return NULL;
	}
	hostname[sizeof(hostname) - 1] = '\0';

	hints.ai_flags = AI_CANONNAME;
	result = getaddrinfo(hostname, NULL, &hints, &addresses);
	if (result != 0 || addresses == NULL) {
		freeaddrinfo(addresses);
		return NULL;
	}
	domain = copy_string(addresses->ai_canonname != NULL
		? addresses->ai_canonname : hostname);
	freeaddrinfo(addresses);
	return domain;
}