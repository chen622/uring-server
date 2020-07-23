/*
  Copyright (c) 2020 Chenming C (ccm@ccm.ink)

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/


#include "websever.h"

struct io_uring ring;

extern int type;

int check_kernel_version() {
    struct utsname buffer;
    char *p;
    long ver[16];
    int i=0;

    if (uname(&buffer) != 0) {
        perror("uname");
        exit(EXIT_FAILURE);
    }

    p = buffer.release;

    while (*p) {
        if (isdigit(*p)) {
            ver[i] = strtol(p, &p, 10);
            i++;
        } else {
            p++;
        }
    }
    printf("Minimum kernel version required is: %d.%d\n",
            MIN_KERNEL_VERSION, MIN_MAJOR_VERSION);
    if (ver[0] >= MIN_KERNEL_VERSION && ver[1] >= MIN_MAJOR_VERSION ) {
        printf("Your kernel version is: %ld.%ld\n", ver[0], ver[1]);
        return 0;
    }
    fprintf(stderr, "Error: your kernel version is: %ld.%ld\n",
                    ver[0], ver[1]);
    return 1;
}

int real_add_accept_request(int server_socket, struct sockaddr_in *client_addr,
                       socklen_t *client_addr_len, int epoll_fd) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_accept(sqe, server_socket, (struct sockaddr *) client_addr,
                         client_addr_len, 0);
    struct request *req = malloc(sizeof(*req));
    req->event_type = EVENT_TYPE_ACCEPT;
    io_uring_sqe_set_data(sqe, req);
    io_uring_submit(&ring);

    return 0;
}

int real_add_read_request(int client_socket, int epoll_fd) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    struct request *req = malloc(sizeof(*req) + sizeof(struct iovec));
    req->iov[0].iov_base = malloc(READ_SZ);
    req->iov[0].iov_len = READ_SZ;
    req->event_type = EVENT_TYPE_READ;
    req->client_socket = client_socket;
    memset(req->iov[0].iov_base, 0, READ_SZ);
    /* Linux kernel 5.5 has support for readv, but not for recv() or read() */
    io_uring_prep_readv(sqe, client_socket, &req->iov[0], 1, 0);
    io_uring_sqe_set_data(sqe, req);
    io_uring_submit(&ring);
    return 0;
}

int real_add_write_request(struct request *req, int epoll_fd) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    req->event_type = EVENT_TYPE_WRITE;
    io_uring_prep_writev(sqe, req->client_socket, req->iov, req->iovec_count, 0);
    io_uring_sqe_set_data(sqe, req);
    io_uring_submit(&ring);
    return 0;
}


void server_loop_uring(int server_socket) {
    struct io_uring_cqe *cqe;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    add_accept_request(server_socket, &client_addr, &client_addr_len, -1);

    while (1) {
        int ret = io_uring_wait_cqe(&ring, &cqe);
        struct request *req = (struct request *) cqe->user_data;
        if (ret < 0)
            fatal_error("io_uring_wait_cqe");
        if (cqe->res < 0) {
            fprintf(stderr, "Async request failed: %s for event: %d\n",
                    strerror(-cqe->res), req->event_type);
            exit(1);
        }

        switch (req->event_type) {
            case EVENT_TYPE_ACCEPT:
                add_accept_request(server_socket, &client_addr, &client_addr_len, -1);
                add_read_request(cqe->res,-1);
                free(req);
                break;
            case EVENT_TYPE_READ:
                if (!cqe->res) {
                    fprintf(stderr, "Empty request!\n");
                    break;
                }
                handle_client_request(req,-1);
                free(req->iov[0].iov_base);
                free(req);
                break;
            case EVENT_TYPE_WRITE:
                for (int i = 0; i < req->iovec_count; i++) {
                    free(req->iov[i].iov_base);
                }
                close(req->client_socket);
                free(req);
                break;
        }
        /* Mark this request as processed */
        io_uring_cqe_seen(&ring, cqe);
    }
}

void sigint_handler(int signo) {
    printf("^C pressed. Shutting down.\n");
    io_uring_queue_exit(&ring);
    exit(0);
}

int main() {
    type = 0;
    if (check_kernel_version()) {
        return EXIT_FAILURE;
    }
    check_for_index_file();
    int server_socket = setup_listening_socket(8001);
    printf("ZeroHTTPd listening on port: %d\n", 8001);

    signal(SIGINT, sigint_handler);
    io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
    server_loop_uring(server_socket);

    return 0;
}