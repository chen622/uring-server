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

extern int type;


int real_add_accept_request(int server_socket, struct sockaddr_in *client_addr,
                            socklen_t *client_addr_len, int epoll_fd) {
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event);
}

int real_add_read_request(int client_socket, int epoll_fd) {
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = client_socket;
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &event);
}

int real_add_write_request(struct request *req, int epoll_fd) {
    struct epoll_event event;
    event.events = EPOLLOUT;
    event.data.fd = req->client_socket;
    event.data.ptr = req;
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, req->client_socket, &event);
}


void server_loop_epoll(int server_socket) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int epoll_fd = epoll_create(MAX_EVENTS);

    if (epoll_fd == -1) {
        perror("epoll_create failed\n");
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[MAX_EVENTS];

    signal(SIGPIPE, SIG_IGN);

    add_accept_request(server_socket, &client_addr, &client_addr_len, epoll_fd);

    int nfds, connFd, read_size, write_size;
    char line[READ_SZ];
    while (1) {
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 10);
        if (nfds != 0)
            printf("\nnfds:%d\n", nfds);
        if (nfds == -1) {
            perror("start epoll_wait failed\n");
            continue;
        }

        for (int j = 0; j < nfds; ++j) {
            if (events[j].data.fd == server_socket && events[j].events == EPOLLIN) { //接受新连接
                if ((connFd = accept(server_socket, (struct sockaddr *) &client_addr, &client_addr_len)) < 0) {
                    perror("accept conn_fd failed");
                    exit(EXIT_FAILURE);
                }
                printf("accept:%d\n", connFd);
                add_read_request(connFd, epoll_fd);
            } else if (events[j].events == EPOLLIN) { //读取请求内容
                char buffer[1024] = {'\0'};
                int ret = recv(events[j].data.fd, buffer, 1024, 0);
                if (ret > 0) {
                    struct request *req = zh_malloc(sizeof(*req) + sizeof(struct iovec));
                    req->iov[0].iov_base = buffer;
                    req->iov[0].iov_len = ret;
                    req->client_socket = events[j].data.fd;
                    handle_client_request(req, epoll_fd);
                    free(req);
                }
                //发生错误
                if (ret == -1) {
                    //EAGAIN/EWOULDBLOCK提示你的应用程序现在没有数据可读请稍后再试
                    //EINTR指操作被中断唤醒，需要重新读
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK ||errno == EINTR)) {
                        continue;
                    }
                    //异常断开情况
                    else {
                        epoll_ctl(epoll_fd,EPOLL_CTL_DEL,events[j].data.fd,&events[j]);
                        close(events[j].data.fd);
                        perror("Read failed.");
                        break;
                    }
                }
                //接收到主动关闭请求
                if (ret == 0) {
                    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,events[j].data.fd,&events[j]);
                    close(events[j].data.fd);
                    perror("Read failed.");
                    break;
                }
            } else {
                struct request * req = events[j].data.ptr;
                write_size = writev(req->client_socket, req->iov,req->iovec_count);
                if (write_size <= 0){
                    perror("Write failed.");
                }
                epoll_ctl(epoll_fd,EPOLL_CTL_DEL,req->client_socket,&events[j]);
                close(req->client_socket);
                free(req);
            }
        }
    }
}

int main() {
    type = 1;
    check_for_index_file();
    int server_socket = setup_listening_socket(DEFAULT_SERVER_PORT);
    printf("ZeroHTTPd listening on port: %d\n", DEFAULT_SERVER_PORT);
    server_loop_epoll(server_socket);
    return 0;
}