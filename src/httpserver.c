#include "httpserver.h"

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <string.h>
#include <stdlib.h>

#define RECV_BUF_SIZE   1024
#define LISTEN_BACKLOG  1
#define THREAD_STACK_SZ 4096
#define THREAD_PRIO     7

static K_THREAD_STACK_DEFINE(server_stack, THREAD_STACK_SZ);
static struct k_thread server_thread;
static volatile bool running;
static int listen_fd = -1;

static uint16_t server_port;
static http_handler_t request_handler;
static void *handler_data;

int httpserver_send(int client_fd, const char *data, size_t len)
{
    while (len > 0) {
        ssize_t n = zsock_send(client_fd, data, len, 0);
        if (n <= 0) return -1;
        data += n;
        len -= n;
    }
    return 0;
}

static void parse_and_dispatch(int client_fd, char *buf, size_t total)
{
    buf[total] = '\0';

    char method[8] = {0};
    char path[64] = {0};

    char *sp1 = strchr(buf, ' ');
    if (!sp1) return;
    size_t mlen = (size_t)(sp1 - buf);
    if (mlen >= sizeof(method)) mlen = sizeof(method) - 1;
    memcpy(method, buf, mlen);

    char *sp2 = strchr(sp1 + 1, ' ');
    if (!sp2) return;
    size_t plen = (size_t)(sp2 - sp1 - 1);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, sp1 + 1, plen);

    char *body = strstr(buf, "\r\n\r\n");
    size_t body_len = 0;
    if (body) {
        body += 4;
        body_len = total - (size_t)(body - buf);
    }

    http_request_t req = {
        .method   = method,
        .path     = path,
        .body     = body,
        .body_len = body_len,
    };

    request_handler(client_fd, &req, handler_data);
}

static void handle_connection(int client_fd)
{
    char buf[RECV_BUF_SIZE];
    ssize_t total = 0;

    while (total < (ssize_t)(sizeof(buf) - 1)) {
        ssize_t n = zsock_recv(client_fd, buf + total,
                               sizeof(buf) - 1 - total, 0);
        if (n <= 0) break;
        total += n;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) break;
    }

    if (total <= 0) return;

    /* If POST, try to read remaining body per Content-Length */
    if (strncmp(buf, "POST", 4) == 0) {
        char *cl = strstr(buf, "Content-Length:");
        if (!cl) cl = strstr(buf, "content-length:");
        if (cl) {
            int content_len = atoi(cl + 15);
            char *body = strstr(buf, "\r\n\r\n");
            if (body) {
                body += 4;
                int body_so_far = (int)(total - (body - buf));
                while (body_so_far < content_len &&
                       total < (ssize_t)(sizeof(buf) - 1)) {
                    ssize_t n = zsock_recv(client_fd, buf + total,
                                           sizeof(buf) - 1 - total, 0);
                    if (n <= 0) break;
                    total += n;
                    buf[total] = '\0';
                    body_so_far += n;
                }
            }
        }
    }

    parse_and_dispatch(client_fd, buf, (size_t)total);
}

static void server_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(server_port),
    };
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    listen_fd = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd < 0) return;

    int opt = 1;
    zsock_setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (zsock_bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        zsock_close(listen_fd);
        listen_fd = -1;
        return;
    }

    if (zsock_listen(listen_fd, LISTEN_BACKLOG) < 0) {
        zsock_close(listen_fd);
        listen_fd = -1;
        return;
    }

    while (running) {
        struct zsock_pollfd pfd = {
            .fd = listen_fd,
            .events = ZSOCK_POLLIN,
        };
        int ret = zsock_poll(&pfd, 1, 1000);
        if (ret <= 0) continue;

        int client = zsock_accept(listen_fd, NULL, NULL);
        if (client < 0) continue;

        handle_connection(client);
        zsock_close(client);
    }

    zsock_close(listen_fd);
    listen_fd = -1;
}

int httpserver_start(uint16_t port, http_handler_t handler, void *user_data)
{
    if (running) return -EALREADY;
    if (!handler) return -EINVAL;

    server_port = port;
    request_handler = handler;
    handler_data = user_data;
    running = true;

    k_thread_create(&server_thread, server_stack,
                    K_THREAD_STACK_SIZEOF(server_stack),
                    server_thread_fn, NULL, NULL, NULL,
                    THREAD_PRIO, 0, K_NO_WAIT);

    return 0;
}

void httpserver_stop(void)
{
    if (!running) return;

    running = false;
    k_thread_join(&server_thread, K_SECONDS(5));
    request_handler = NULL;
    handler_data = NULL;
}
