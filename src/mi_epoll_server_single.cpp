#include "mi_epoll_server_single.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/eventfd.h>

#include <iostream>
#include <sstream>
#include <sys/epoll.h>
#include<netinet/tcp.h>

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

#include "mi_rpc_common.h"

#define MAX_EVENT_NUMBER 2048

#define MAX_THREAD 7

int check_sock_alive(int sock) {
    struct tcp_info info; 
    int len = sizeof(info); 
    getsockopt(sock, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&len);
    if((info.tcpi_state==TCP_ESTABLISHED)) {
        return 0;
    } else {
        return 1;
    }
}

int set_nonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int efd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    //EPOLLET 这里用水平触发的方式(不采用边缘触发)，因为边缘触发在处理数据的时候，会无法触发close的EOF消息
    event.events = EPOLLIN | EPOLLERR | EPOLLRDHUP | EPOLLHUP;
    epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

void removefd(int efd, int fd) {
    std::cout << "<><><<><><><><><><><><> remove fd: " << fd << "\n";
    epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
}

EpollServerSingle::EpollServerSingle() {
}

EpollServerSingle::~EpollServerSingle() {

}

void EpollServerSingle::listen(int port) {
    _port = port;    
}

void EpollServerSingle::run() {
    _sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (_sock_fd < 0) {
        std::cerr << "create EpollServerSingle's socket failed\n";
        return;
    }
    std::cout << "server sock: " << _sock_fd << std::endl;

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);

    int ret = bind(_sock_fd, (sockaddr*)(&addr), sizeof(addr));
    if (ret < 0) {
        std::cerr << "EpollServerSingle bind failed\n";
        return;
    }

    ret = ::listen(_sock_fd, 100);
    if (ret < 0) {
        std::cerr << "EpollServerSingle listen failed\n";
        return;
    }


    std::cout << "EpollServerSingle running >>>>>>\n";

    epoll_event events[MAX_EVENT_NUMBER];
    _efd = epoll_create(5);
    if (_efd < 0) {
        std::cout << "create EpollServerSingle's efd failed.\n";
        return;
    }

    addfd(_efd, _sock_fd);

    while(1) {
        int ret = epoll_wait(_efd, events,MAX_EVENT_NUMBER, -1);
        if (ret<0) {
            std::cout << "epoll wait falied: " << ret << ", err: " << errno << std::endl;
            break;
        }
        std::cout << "epoll wait: " << ret << std::endl;
        for (int i=0; i<ret; ++i) {
            int sockfd = events[i].data.fd;
            std::cout << "sock: " << sockfd <<  " event: " << events[i].events << std::endl;
            
            if (sockfd == _sock_fd) {
                sockaddr_in client_address;
                socklen_t client_address_ln = sizeof(client_address);
                int connfd = accept(_sock_fd, (sockaddr*)&client_address, &client_address_ln);
                if (connfd < 0) {
                    std::cout << "accept client failed: " << connfd << ", err: " << errno << std::endl; 
                } else {
                    addfd(_efd, connfd);
                }
            } else if (events[i].events & EPOLLIN) {
                std::cout << "catch write sock\n";
                BufferHeader header;
                char* data_buffer = nullptr;
                
                int cur_size = 0;
                int accum_size = 0;
                int try_size = sizeof(BufferHeader);
                char* buffer = (char*)(&header);
                while (accum_size < (int)sizeof(BufferHeader)) {
                    cur_size = recv(sockfd, buffer+accum_size, try_size, 0);
                    if (cur_size < 0 && errno == EINTR) {
                        continue;
                    } else if (cur_size < 0 &&  (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        //非阻塞IO，遇到这种情况表示数据传输完成
                        std::cout << "break here 1\n";
                        break;
                    } else if (cur_size == 0) {
                        std::cout << "recv ret 0 close\n";
                        close(sockfd);
                        removefd(_efd, sockfd);
                        break;
                    } else if (cur_size < 0) {
                        std::cout << "recv ret err: " << errno << "\n";
                        close(sockfd);
                        removefd(_efd, sockfd);
                        break;
                    }
                    accum_size += cur_size;
                    try_size -= cur_size;
                    std::cout << "here: " <<accum_size <<  "\n";
                }
                if (cur_size <=0) {
                    break;
                }
                std::cout << "buffer size: " << header.buffer_size << std::endl;

                
                cur_size = 0;
                accum_size = 0;
                try_size = header.buffer_size;
                data_buffer = new char[header.buffer_size];
                buffer = data_buffer;
                while (accum_size < header.buffer_size) {
                    cur_size = recv(sockfd, buffer+accum_size, try_size, 0);
                    if (cur_size < 0 && errno == EINTR) {
                        continue;
                    } else if (cur_size < 0 &&  (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        //非阻塞IO，遇到这种情况表示数据传输完成
                        std::cout << "break here 2\n";
                        break;
                    } else if (cur_size == 0) {
                        std::cout << "recv ret 0 close\n";
                        close(sockfd);
                        removefd(_efd, sockfd);
                        break;
                    } else if (cur_size < 0) {
                        std::cout << "recv ret err: " << errno << "\n";
                        close(sockfd);
                        removefd(_efd, sockfd);
                        break;
                    }
                    accum_size += cur_size;
                    try_size -= cur_size;
                }

                if (cur_size <=0) {
                    break;
                }

                //完整的pkg

                std::string str(data_buffer, header.buffer_size);
                std::cout << "read client: " << str << std::endl;
                delete [] data_buffer;
            } else if (events[i].events & EPOLLHUP || events[i].events & EPOLLERR || events[i].events & EPOLLRDHUP) {
                std::cout << "catch client sock:" << sockfd << " close\n";
                close(sockfd);
                removefd(_efd, sockfd);

            } else {
                std::cout << "something else happended\n";
            }
        }
    }



    std::cout << "EpollServerSingle shutdown >>>>>>\n";
}