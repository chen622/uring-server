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


#ifndef URING_SERVER_WEBSEVER_H
#define URING_SERVER_WEBSEVER_H

#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <liburing.h>
#include <sys/epoll.h>
#include <errno.h>

#define SERVER_STRING           "Server: zerohttpd/0.1\r\n"
#define DEFAULT_SERVER_PORT     8000
#define QUEUE_DEPTH             100
#define READ_SZ                 8192

#define EVENT_TYPE_ACCEPT       0
#define EVENT_TYPE_READ         1
#define EVENT_TYPE_WRITE        2

#define MIN_KERNEL_VERSION      5
#define MIN_MAJOR_VERSION       5

#define MAX_EVENTS 100

#define UNIMPLEMENT "HTTP/1.0 400 Bad Request\r\n"\
                                "Content-type: text/html\r\n"\
                                "\r\n"\
                                "<html>"\
                                "<head>"\
                                "<title>ZeroHTTPd: Unimplemented</title>"\
                                "</head>"\
                                "<body>"\
                                "<h1>Bad Request (Unimplemented)</h1>"\
                                "<p>Your client sent a request ZeroHTTPd did not understand and it is probably not your fault.</p>"\
                                "</body>"\
                                "</html>"

#define ERRORMSG_404 "HTTP/1.0 404 Not Found\r\n"\
                                "Content-type: text/html\r\n"\
                                "\r\n"\
                                "<html>"\
                                "<head>"\
                                "<title>ZeroHTTPd: Not Found</title>"\
                                "</head>"\
                                "<body>"\
                                "<h1>Not Found (404)</h1>"\
                                "<p>Your client is asking for an object that was not found on this server.</p>"\
                                "</body>"\
                                "</html>"

#define HTML "<!DOCTYPE html>"\
                "<html lang=\"en\">"\
           "<head>"\
           "<title>Welcome to ZeroHTTPd</title>"\
"<style>"\
        "body {"\
        "font-family: sans-serif;"\
        "margin: 15px;"\
        "text-align: center;"\
"}"\
"</style>"\
"</head>"\
"<body>"\
"<h1>It works! (kinda)</h1>"\
"<img src=\"tux.png\" alt=\"Tux, the Linux mascot\">"\
                                              "<p>It is sure great to get out of that socket!</p>"\
                                              "<p>ZeroHTTPd is a ridiculously simple (and incomplete) web server written for learning about Linux performance.</p>"\
                                              "<p>Learn more about Linux performance at <a href=\"https://unixism.net/2019/04/28/linux-applications-performance-introduction/\">unixism.net</a></p>"\
"<p>This version of ZeroHTTPd uses io_uring. Learn more about io_uring <a href=\"https://unixism.net/loti\">here</a>.</p>"\
"</body>"\
"</html>"

struct request {
    int event_type;
    int iovec_count;
    int client_socket;
    struct iovec iov[];
};


void fatal_error(const char *syscall);

void check_for_index_file();

void strtolower(char *str);

void *zh_malloc(size_t size);

int setup_listening_socket(int port);

int add_accept_request(int server_socket, struct sockaddr_in *client_addr,
                       socklen_t *client_addr_len, int epoll_fd);

int add_read_request(int client_socket, int epoll_fd);

int add_write_request(struct request *req, int epoll_fd);

void send_static_string_content(const char *str, int client_socket, int epoll_fd);

void handle_unimplemented_method(int client_socket, int epoll_fd);

void handle_http_404(int client_socket, int epoll_fd);

void copy_file_contents(char *file_path, off_t file_size, struct iovec *iov);

const char *get_filename_ext(const char *filename);

void send_headers(const char *path, off_t len, struct iovec *iov);

void handle_get_method(char *path, int client_socket, int epoll_fd);

void handle_http_method(char *method_buffer, int client_socket, int epoll_fd);

int get_line(const char *src, char *dest, int dest_sz);

int handle_client_request(struct request *req, int epoll_fd);


int real_add_accept_request(int server_socket, struct sockaddr_in *client_addr,
                            socklen_t *client_addr_len, int epoll_fd);

int real_add_read_request(int client_socket, int epoll_fd);

int real_add_write_request(struct request *req, int epoll_fd);

#endif //URING_SERVER_WEBSEVER_H
