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

//0 liburing; 1 epoll
int type;

/*
 One function that prints the system call and the error details
 and then exits with error code 1. Non-zero meaning things didn't go well.
 */
void fatal_error(const char *syscall) {
    perror(syscall);
    exit(1);
}

void check_for_index_file() {
    struct stat st;
    int ret = stat("../public/index.html", &st);
    if (ret < 0) {
        fprintf(stderr, "ZeroHTTPd needs the \"public\" directory to be "
                        "present in the current directory.\n");
        fatal_error("stat: public/index.html");
    }
}

/*
 * Utility function to convert a string to lower case.
 * */
void strtolower(char *str) {
    for (; *str; ++str)
        *str = (char) tolower(*str);
}

/*
 * Helper function for cleaner looking code.
 * */
void *zh_malloc(size_t size) {
    void *buf = malloc(size);
    if (!buf) {
        fprintf(stderr, "Fatal error: unable to allocate memory.\n");
        exit(1);
    }
    return buf;
}

/*
 * This function is responsible for setting up the main listening socket used by the
 * web server.
 * */
int setup_listening_socket(int port) {
    int sock;
    struct sockaddr_in srv_addr;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        fatal_error("socket()");

    int enable = 1;
    if (setsockopt(sock,
                   SOL_SOCKET, SO_REUSEADDR,
                   &enable, sizeof(int)) < 0)
        fatal_error("setsockopt(SO_REUSEADDR)");


    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* We bind to a port and turn this socket into a listening
     * socket.
     * */
    if (bind(sock,
             (const struct sockaddr *) &srv_addr,
             sizeof(srv_addr)) < 0)
        fatal_error("bind()");

    if (listen(sock, 10) < 0)
        fatal_error("listen()");

    return (sock);
}

void send_static_string_content(const char *str, int client_socket, int epoll_fd) {
    struct request *req = zh_malloc(sizeof(*req) + sizeof(struct iovec));
    unsigned long slen = strlen(str);
    req->iovec_count = 1;
    req->client_socket = client_socket;
    req->iov[0].iov_base = zh_malloc(slen);
    req->iov[0].iov_len = slen;
    memcpy(req->iov[0].iov_base, str, slen);
    add_write_request(req, epoll_fd);
}


int add_accept_request(int server_socket, struct sockaddr_in *client_addr,
                       socklen_t *client_addr_len, int epoll_fd) {
    real_add_accept_request(server_socket, client_addr, client_addr_len, epoll_fd);
//    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
//    io_uring_prep_accept(sqe, server_socket, (struct sockaddr *) client_addr,
//                         client_addr_len, 0);
//    struct request *req = malloc(sizeof(*req));
//    req->event_type = EVENT_TYPE_ACCEPT;
//    io_uring_sqe_set_data(sqe, req);
//    io_uring_submit(&ring);

    return 0;
}

int add_read_request(int client_socket, int epoll_fd) {

    real_add_read_request(client_socket, epoll_fd);
//    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
//    struct request *req = malloc(sizeof(*req) + sizeof(struct iovec));
//    req->iov[0].iov_base = malloc(READ_SZ);
//    req->iov[0].iov_len = READ_SZ;
//    req->event_type = EVENT_TYPE_READ;
//    req->client_socket = client_socket;
//    memset(req->iov[0].iov_base, 0, READ_SZ);
//    /* Linux kernel 5.5 has support for readv, but not for recv() or read() */
//    io_uring_prep_readv(sqe, client_socket, &req->iov[0], 1, 0);
//    io_uring_sqe_set_data(sqe, req);
//    io_uring_submit(&ring);
    return 0;
}

int add_write_request(struct request *req, int epoll_fd) {

    real_add_write_request(req, epoll_fd);
//    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
//    req->event_type = EVENT_TYPE_WRITE;
//    io_uring_prep_writev(sqe, req->client_socket, req->iov, req->iovec_count, 0);
//    io_uring_sqe_set_data(sqe, req);
//    io_uring_submit(&ring);
    return 0;
}

/*
 * When ZeroHTTPd encounters any other HTTP method other than GET or POST, this function
 * is used to inform the client.
 * */
void handle_unimplemented_method(int client_socket, int epoll_fd) {
    send_static_string_content(UNIMPLEMENT, client_socket, epoll_fd);
}

/*
 * This function is used to send a "HTTP Not Found" code and message to the client in
 * case the file requested is not found.
 * */
void handle_http_404(int client_socket, int epoll_fd) {
    send_static_string_content(ERRORMSG_404, client_socket, epoll_fd);
}

/*
 * Once a static file is identified to be served, this function is used to read the file
 * and write it over the client socket using Linux's sendfile() system call. This saves us
 * the hassle of transferring file buffers from kernel to user space and back.
 * */
void copy_file_contents(char *file_path, off_t file_size, struct iovec *iov) {
    int fd;
    char *buf = zh_malloc(file_size);
    strcpy(buf, HTML);
//    fd = open(file_path, O_RDONLY);
//    if (fd < 0)
//        fatal_error("read");
//
//    /* We should really check for short reads here */
//    int ret = read(fd, buf, file_size);
//    if (ret < file_size) {
//        fprintf(stderr, "Encountered a short read.\n");
//    }
//    close(fd);

    iov->iov_base = buf;
    iov->iov_len = file_size;
}

/*
 * Simple function to get the file extension of the file that we are about to serve.
 * */
const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";
    return dot + 1;
}

/*
 * Sends the HTTP 200 OK header, the server string, for a few types of files, it can also
 * send the content type based on the file extension. It also sends the content length
 * header. Finally it send a '\r\n' in a line by itself signalling the end of headers
 * and the beginning of any content.
 * */
void send_headers(const char *path, off_t len, struct iovec *iov) {
    char small_case_path[1024];
    char send_buffer[1024];
    strcpy(small_case_path, path);
    strtolower(small_case_path);

    char *str = "HTTP/1.0 200 OK\r\n";
    unsigned long slen = strlen(str);
    iov[0].iov_base = zh_malloc(slen);
    iov[0].iov_len = slen;
    memcpy(iov[0].iov_base, str, slen);

    slen = strlen(SERVER_STRING);
    iov[1].iov_base = zh_malloc(slen);
    iov[1].iov_len = slen;
    memcpy(iov[1].iov_base, SERVER_STRING, slen);

    /*
     * Check the file extension for certain common types of files
     * on web pages and send the appropriate content-type header.
     * Since extensions can be mixed case like JPG, jpg or Jpg,
     * we turn the extension into lower case before checking.
     * */
    const char *file_ext = get_filename_ext(small_case_path);
    if (strcmp("jpg", file_ext) == 0)
        strcpy(send_buffer, "Content-Type: image/jpeg\r\n");
    if (strcmp("jpeg", file_ext) == 0)
        strcpy(send_buffer, "Content-Type: image/jpeg\r\n");
    if (strcmp("png", file_ext) == 0)
        strcpy(send_buffer, "Content-Type: image/png\r\n");
    if (strcmp("gif", file_ext) == 0)
        strcpy(send_buffer, "Content-Type: image/gif\r\n");
    if (strcmp("htm", file_ext) == 0)
        strcpy(send_buffer, "Content-Type: text/html\r\n");
    if (strcmp("html", file_ext) == 0)
        strcpy(send_buffer, "Content-Type: text/html\r\n");
    if (strcmp("js", file_ext) == 0)
        strcpy(send_buffer, "Content-Type: application/javascript\r\n");
    if (strcmp("css", file_ext) == 0)
        strcpy(send_buffer, "Content-Type: text/css\r\n");
    if (strcmp("txt", file_ext) == 0)
        strcpy(send_buffer, "Content-Type: text/plain\r\n");
    slen = strlen(send_buffer);
    iov[2].iov_base = zh_malloc(slen);
    iov[2].iov_len = slen;
    memcpy(iov[2].iov_base, send_buffer, slen);

    /* Send the content-length header, which is the file size in this case. */
    sprintf(send_buffer, "content-length: %ld\r\n", len);
    slen = strlen(send_buffer);
    iov[3].iov_base = zh_malloc(slen);
    iov[3].iov_len = slen;
    memcpy(iov[3].iov_base, send_buffer, slen);

    /*
     * When the browser sees a '\r\n' sequence in a line on its own,
     * it understands there are no more headers. Content may follow.
     * */
    strcpy(send_buffer, "\r\n");
    slen = strlen(send_buffer);
    iov[4].iov_base = zh_malloc(slen);
    iov[4].iov_len = slen;
    memcpy(iov[4].iov_base, send_buffer, slen);
}

void handle_get_method(char *path, int client_socket, int epoll_fd) {
    char final_path[1024] = {'\0'};
    strcpy(final_path, "../");

    /*
     If a path ends in a trailing slash, the client probably wants the index
     file inside of that directory.
     */
    if (path[strlen(path) - 1] == '/') {
        strcat(final_path, "public");
        strcat(final_path, path);
        strcat(final_path, "index.html");
    } else {
        strcat(final_path, "public");
        strcat(final_path, path);
    }
    printf("%s", final_path);

    /* The stat() system call will give you information about the file
     * like type (regular file, directory, etc), size, etc. */
//    struct stat path_stat;
//    if (stat(final_path, &path_stat) == -1) {
//        printf("404 Not Found: %s (%s)\n", final_path, path);
//        handle_http_404(client_socket, epoll_fd);
//    } else {
        /* Check if this is a normal/regular file and not a directory or something else */
//        if (S_ISREG(path_stat.st_mode)) {
            struct request *req = zh_malloc(sizeof(*req) + (sizeof(struct iovec) * 6));
            req->iovec_count = 6;
            req->client_socket = client_socket;
            long str_size = sizeof(HTML);
            send_headers(final_path, str_size, req->iov);
            copy_file_contents(final_path, str_size, &req->iov[5]);
            printf("200 %s %ld bytes\n", final_path, str_size);
            add_write_request(req, epoll_fd);
//        } else {
//            handle_http_404(client_socket, epoll_fd);
//            printf("404 Not Found: %s\n", final_path);
//        }
//    }
}

/*
 * This function looks at method used and calls the appropriate handler function.
 * Since we only implement GET and POST methods, it calls handle_unimplemented_method()
 * in case both these don't match. This sends an error to the client.
 * */

void handle_http_method(char *method_buffer, int client_socket, int epoll_fd) {
    char *method, *path, *saveptr;

    method = strtok_r(method_buffer, " ", &saveptr);
    strtolower(method);
    path = strtok_r(NULL, " ", &saveptr);

    if (strcmp(method, "get") == 0) {
        handle_get_method(path, client_socket, epoll_fd);
    } else {
        handle_unimplemented_method(client_socket, epoll_fd);
    }
}

int get_line(const char *src, char *dest, int dest_sz) {
    for (int i = 0; i < dest_sz; i++) {
        dest[i] = src[i];
        if (src[i] == '\r' && src[i + 1] == '\n') {
            dest[i] = '\0';
            return 0;
        }
    }
    return 1;
}

int handle_client_request(struct request *req, int epoll_fd) {
    char http_request[1024];
    /* Get the first line, which will be the request */
    if (get_line(req->iov[0].iov_base, http_request, sizeof(http_request))) {
        fprintf(stderr, "Malformed request\n");
        exit(1);
    }
    handle_http_method(http_request, req->client_socket, epoll_fd);
    return 0;
}