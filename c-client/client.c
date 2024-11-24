#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_MESSAGE_LENGTH 512
#define MAX_USERNAME_LENGTH 64

static char message_buffer[MAX_MESSAGE_LENGTH] = {0};
static char username[MAX_USERNAME_LENGTH] = {0};
static int message_ready = 0;
static int should_exit = 0;

// WebSocket callback
static int callback_ws(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len)
{
  switch (reason)
  {
  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    printf("\nConnection established\n");
    lws_callback_on_writable(wsi);
    break;

  case LWS_CALLBACK_CLIENT_WRITEABLE:
    if (message_ready)
    {
      size_t msg_len = strlen(message_buffer);
      unsigned char buf[LWS_PRE + MAX_MESSAGE_LENGTH];

      memcpy(&buf[LWS_PRE], message_buffer, msg_len);

      int written = lws_write(wsi, &buf[LWS_PRE], msg_len, LWS_WRITE_TEXT);
      if (written < (int)msg_len)
      {
        fprintf(stderr, "Error writing to WebSocket\n");
      }

      message_ready = 0; // Reset flag after sending
    }

    // Allow another writable callback
    lws_callback_on_writable(wsi);
    break;

  case LWS_CALLBACK_CLIENT_RECEIVE:
    // the response will be like {"username":"Alice","message":"Hello, World!"}
    // we want to print it as "[Alice]: Hello, World!"

    char *username_start = strstr(in, "\"username\":\"") + 12;
    char *username_end = strstr(username_start, "\"") - 1;
    char *message_start = strstr(username_end, "\"message\":\"") + 11;
    char *message_end = strstr(message_start, "\"") - 1;

    char username[MAX_USERNAME_LENGTH] = {0};
    char message[MAX_MESSAGE_LENGTH] = {0};

    strncpy(username, username_start, username_end - username_start + 1);
    strncpy(message, message_start, message_end - message_start + 1);

    printf("[%s]: %s\n", username, message);

    break;

  case LWS_CALLBACK_CLOSED:
    printf("Connection closed\n");
    should_exit = 1; // Stop the main loop
    break;

  default:
    break;
  }
  return 0;
}

// Function to handle user input
void *user_input_handler(void *arg)
{
  struct lws *wsi = (struct lws *)arg;
  char input[MAX_MESSAGE_LENGTH - MAX_USERNAME_LENGTH - 20]; // Leave space for JSON overhead

  while (!should_exit)
  {
    if (fgets(input, sizeof(input), stdin))
    {
      // Remove trailing newline
      size_t len = strlen(input);
      if (len > 0 && input[len - 1] == '\n')
      {
        input[len - 1] = '\0';
      }

      // Format message as JSON
      int written = snprintf(
          message_buffer,
          MAX_MESSAGE_LENGTH,
          "{\"username\":\"%s\",\"message\":\"%s\"}",
          username,
          input);

      if (written >= MAX_MESSAGE_LENGTH)
      {
        fprintf(stderr, "Message too long. Truncated.\n");
        continue;
      }

      message_ready = 1;             // Mark message as ready to send
      lws_callback_on_writable(wsi); // Request writable callback
    }
  }

  return NULL;
}

int main()
{
  struct lws_context_creation_info ctx_info = {0};
  struct lws_client_connect_info client_info = {0};
  struct lws_context *context;
  struct lws *client_wsi;
  pthread_t input_thread;

  // Get the username from the user
  printf("Enter your username: ");
  if (fgets(username, MAX_USERNAME_LENGTH, stdin))
  {
    size_t len = strlen(username);
    if (len > 0 && username[len - 1] == '\n')
    {
      username[len - 1] = '\0';
    }
  }

  // Initialize context information
  ctx_info.port = CONTEXT_PORT_NO_LISTEN;
  ctx_info.protocols = (struct lws_protocols[]){
      {"ws-protocol", callback_ws, 0, MAX_MESSAGE_LENGTH},
      {NULL, NULL, 0, 0}};

  // Create context
  context = lws_create_context(&ctx_info);
  if (!context)
  {
    fprintf(stderr, "Failed to create context\n");
    return 1;
  }

  // Configure client connection
  client_info.context = context;
  client_info.address = "localhost"; // Replace with your server address
  client_info.port = 3001;           // Replace with your server port
  client_info.path = "/";
  client_info.host = lws_canonical_hostname(context);
  client_info.origin = "origin";
  client_info.protocol = "ws-protocol";

  // Connect to the server
  client_wsi = lws_client_connect_via_info(&client_info);
  if (!client_wsi)
  {
    fprintf(stderr, "Failed to connect\n");
    lws_context_destroy(context);
    return 1;
  }

  // Start a separate thread to handle user input
  if (pthread_create(&input_thread, NULL, user_input_handler, (void *)client_wsi) != 0)
  {
    fprintf(stderr, "Failed to create input thread\n");
    lws_context_destroy(context);
    return 1;
  }

  // Main loop to handle WebSocket events
  while (!should_exit)
  {
    if (lws_service(context, 1000) < 0)
    {
      break;
    }
  }

  // Wait for the input thread to finish
  pthread_join(input_thread, NULL);

  // Cleanup
  lws_context_destroy(context);
  return 0;
}
