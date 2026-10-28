#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <stddef.h>
#include <stdint.h>

/*
 * HTTP request passed to the handler callback.
 */
typedef struct {
    const char *method;
    const char *path;
    const char *body;
    size_t body_len;
} http_request_t;

/*
 * Handler callback.
 *
 * Called for each incoming HTTP request.  The handler writes its
 * response by calling httpserver_send() with the client socket fd
 * provided as the first argument.
 */
typedef void (*http_handler_t)(int client_fd, const http_request_t *req,
                               void *user_data);

/*
 * httpserver_start - Start a single-client HTTP listener thread.
 *
 * @port:      TCP port to listen on.
 * @handler:   Callback invoked for every request.
 * @user_data: Opaque pointer forwarded to the handler.
 *
 * Returns 0 on success, negative errno on failure.
 */
int httpserver_start(uint16_t port, http_handler_t handler, void *user_data);

/*
 * httpserver_stop - Stop the listener thread and close the socket.
 */
void httpserver_stop(void);

/*
 * httpserver_send - Send raw bytes on a client connection.
 *
 * Returns 0 on success, -1 on failure.
 */
int httpserver_send(int client_fd, const char *data, size_t len);

#endif
