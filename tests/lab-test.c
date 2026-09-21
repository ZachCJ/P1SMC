#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "harness/unity.h"
#include "../src/lab.h"


void setUp(void) {
  printf("Setting up tests...\n");
}

void tearDown(void) {
  printf("Tearing down tests...\n");
}

void test_message_arg_parser(void) {
  char *argv[] = {"myapp", "-f", "alice@example.com", "-t", "bob@example.com",
                  "-s", "Hello", "-b", "Message", "-p", "2525",
                  "-H", "client.example", "mail.example"};
  Message message = message_arg_parser((int)(sizeof(argv) / sizeof(argv[0])), argv);

  TEST_ASSERT_EQUAL_STRING("alice@example.com", message.from);
  TEST_ASSERT_EQUAL_STRING("bob@example.com", message.to);
  TEST_ASSERT_EQUAL_STRING("Hello", message.subject);
  TEST_ASSERT_EQUAL_STRING("Message", message.body);
  TEST_ASSERT_EQUAL_INT(2525, message.port);
  TEST_ASSERT_EQUAL_STRING("client.example", message.hostname);
  TEST_ASSERT_EQUAL_STRING("mail.example", message.server);
  message_free(&message);
  TEST_ASSERT_NULL(message.from);
  TEST_ASSERT_EQUAL_INT(-1, message.port);
}

void test_message_to_string(void) {
  Message message = {"alice@example.com", "bob@example.com", "Hello", "Message",
                     25, "client.example", "mail.example"};
  char *serialized = message_to_string(&message);

  TEST_ASSERT_NOT_NULL(serialized);
  TEST_ASSERT_EQUAL_STRING("-f 'alice@example.com' -t 'bob@example.com' "
                           "-s 'Hello' -b 'Message' "
                           "-H 'client.example' -S 'mail.example' "
                           "-p 25", serialized);
  free(serialized);
}

void test_invalid_message_args(void) {
  char *argv[] = {"myapp", "-p", "70000"};
  Message message = message_arg_parser(3, argv);
  TEST_ASSERT_EQUAL_INT(-1, message.port);
  TEST_ASSERT_NULL(message.from);
  message_free(&message);
}

void test_get_computer_domain(void) {
  char *domain = get_computer_domain();

  TEST_ASSERT_NOT_NULL(domain);
  TEST_ASSERT_TRUE(domain[0] != '\0');
  free(domain);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_message_arg_parser);
  RUN_TEST(test_message_to_string);
  RUN_TEST(test_invalid_message_args);
  RUN_TEST(test_get_computer_domain);
  return UNITY_END();
}
