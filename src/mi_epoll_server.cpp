#include "mi_epoll_server.h"

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
#include<boost/log/trivial.hpp>


#include "mi_rpc_common.h"

#define MAX_EVENT_NUMBER 4096
#define MAX_THREAD 7

static boost::mutex _MUTEX;

//#define GLOBAL_LOCK boost::mutex::scoped_lock locker(_MUTEX);

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
    event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
    epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

void removefd(int efd, int fd) {
    epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
}

void EpollServer::add_client(int efd, int fd) {
    boost::mutex::scoped_lock locker(_mutex);
    auto it = _client_fd.find(fd);
    if (it != _client_fd.end()) {
        BOOST_LOG_TRIVIAL(error) <<"<><><><><><><><>add err<><><><><><><><>";
        return;
    }
    addfd(efd, fd);
    _client_fd.insert(fd);
}

void EpollServer::remove_client(int efd, int fd) {
    boost::mutex::scoped_lock locker(_mutex);
    auto it = _client_fd.find(fd);
    if (it == _client_fd.end()) {
        BOOST_LOG_TRIVIAL(error) <<"<><><><><><><><>remove err<><><><><><><><>";
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "------------------------ remove sock -------------------------: " << fd << " ";

    removefd(efd, fd);
    shutdown(fd, SHUT_RDWR);
    close(fd);
    _client_fd.erase(it);
}


void EpollServer::recv_worker(int idx) {
    const int efd = _worker_efd[idx];
    epoll_event events[MAX_EVENT_NUMBER];
    addfd(efd, _sock_inter);
    while (1) {
        int ret = epoll_wait(efd, events,MAX_EVENT_NUMBER, 30);
        if (ret < 0) {
            BOOST_LOG_TRIVIAL(info) << "epoll wait falied: " << ret;
        }
        for (int i=0; i<ret; ++i) {
            int sockfd = events[i].data.fd;
            BOOST_LOG_TRIVIAL(info) << "worker catch sock: " << sockfd << "'s event: " << events[i].events;
            if (sockfd == _sock_inter) {
                if (!_alive) {
                    BOOST_LOG_TRIVIAL(info) << "&&&&&&& worker catch shutdown signal &&&&&&&";
                    return;
                }
                BOOST_LOG_TRIVIAL(info) << "()()())()() worker catch inter sock ()()())()()";
                uint64_t time_val = 10;
                read(sockfd, &time_val, 8);
                
            } else if (events[i].events & EPOLLHUP || events[i].events & EPOLLERR || events[i].events & EPOLLRDHUP) {
                BOOST_LOG_TRIVIAL(info) << "recv peer sock:" << sockfd << " close";
                remove_client(efd, sockfd);
            } else if (events[i].events & EPOLLIN) {
                BOOST_LOG_TRIVIAL(info) << "worker catch sock:" << sockfd << " recv ";
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
                        BOOST_LOG_TRIVIAL(info) << "break here 1";
                        break;
                    } else if (cur_size == 0) {
                        BOOST_LOG_TRIVIAL(info) << "recv ret 0 close";
                        remove_client(efd, sockfd);
                        break;
                    } else if (cur_size < 0) {
                        BOOST_LOG_TRIVIAL(info) << "recv ret err: " << errno;
                        remove_client(efd, sockfd);
                        break;
                    }
                    accum_size += cur_size;
                    try_size -= cur_size;
                }
                if (cur_size <=0) {
                    break;
                }
                BOOST_LOG_TRIVIAL(info) << "buffer size: " << header.buffer_size;

                
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
                        BOOST_LOG_TRIVIAL(info) << "break here 2";
                        break;
                    } else if (cur_size == 0) {
                        BOOST_LOG_TRIVIAL(info) << "recv ret 0 close";
                        remove_client(efd, sockfd);
                        break;
                    } else if (cur_size < 0) {
                        BOOST_LOG_TRIVIAL(info) << "recv ret err: " << errno;
                        remove_client(efd, sockfd);
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
                BOOST_LOG_TRIVIAL(info) << "read client: " << str;
                delete [] data_buffer;

            } else {
                BOOST_LOG_TRIVIAL(info) << "something else happended";
            }
        }
    }
    
}

EpollServer::EpollServer(): _alive(true) {
}

EpollServer::~EpollServer() {

}

void EpollServer::listen(int port) {
    _port = port;    
}

void EpollServer::stop() {
    _alive = false;
    int64_t time_val = 10;
    write(_sock_inter, &time_val, 8);
}

void EpollServer::run() {
    _sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (_sock_fd < 0) {
        BOOST_LOG_TRIVIAL(error) <<"create EpollServer's socket failed";
        return;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);

    int ret = bind(_sock_fd, (sockaddr*)(&addr), sizeof(addr));
    if (ret < 0) {
        BOOST_LOG_TRIVIAL(error) <<"EpollServer bind failed";
        return;
    }

    ret = ::listen(_sock_fd, 100);
    if (ret < 0) {
        BOOST_LOG_TRIVIAL(error) <<"EpollServer listen failed";
        return;
    }


    BOOST_LOG_TRIVIAL(info) << "EpollServer running >>>>>>";

    epoll_event events[MAX_EVENT_NUMBER];
    _efd = epoll_create(5);
    if (_efd < 0) {
        BOOST_LOG_TRIVIAL(info) << "create EpollServer's efd failed.";
        return;
    }

    addfd(_efd, _sock_fd);

    _sock_inter = eventfd(10,0);
    addfd(_efd, _sock_inter);

    for (int i=0; i<MAX_THREAD; ++i) {
        _msg_queue.push_back(std::shared_ptr<MessageQueue<int>>(new MessageQueue<int>()));
        _msg_queue[i]->activate();
    }

    for (int i=0; i<MAX_THREAD; ++i) {
        int efd = epoll_create(5);
        if (efd < 0) {
            BOOST_LOG_TRIVIAL(info) << "create EpollServer's efd failed.";
            return;
        }
        _worker_efd.push_back(efd);
    }

    std::vector<boost::thread> ths;
    for (int i=0; i<MAX_THREAD; ++i) {
        ths.push_back(boost::thread(boost::bind(&EpollServer::recv_worker, this, i)));
    }

    bool alive = true;
    while(1) {
        int ret = epoll_wait(_efd, events,MAX_EVENT_NUMBER, 30);
        if (ret<0) {
            BOOST_LOG_TRIVIAL(info) << "epoll wait falied: " << ret << ", err: " << errno;
            break;
        }
        for (int i=0; i<ret; ++i) {
            int sockfd = events[i].data.fd;
            // {
            //     GLOBAL_LOCK
            //     BOOST_LOG_TRIVIAL(info) << "sock: " << sockfd <<  " event: " << events[i].events;
            // }
            
            if (sockfd == _sock_fd) {
                sockaddr_in client_address;
                socklen_t client_address_ln = sizeof(client_address);
                int connfd = accept(_sock_fd, (sockaddr*)&client_address, &client_address_ln);
                if (connfd < 0) {
                    BOOST_LOG_TRIVIAL(info) << "accept client failed: " << connfd << ", err: " << errno; 
                } else {
                    add_client(_worker_efd[sockfd%MAX_THREAD], connfd);
                    // uint64_t time_val = 10;
                    // int err = write(_sock_inter, &time_val, 8);
                    // if (err <=0 ) {
                    //     BOOST_LOG_TRIVIAL(info) << "send inter sock failed." << errno;
                    // }
                }
            } else if (sockfd == _sock_inter) {
                BOOST_LOG_TRIVIAL(info) << "()()())()() main catch inter sock ()()())()()";
                if (!_alive) {
                    BOOST_LOG_TRIVIAL(info) << "&&&&&&& main catch shutdown signal &&&&&&&";
                    return;
                }
                // uint64_t time_val = 10;
                // read(sockfd, &time_val, 8);
                
            } else if (events[i].events & EPOLLIN) {
                BOOST_LOG_TRIVIAL(info) << "main catch write sock";
                //_msg_queue[sockfd%MAX_THREAD]->push(sockfd);

            } else {
                BOOST_LOG_TRIVIAL(info) << "something else happended";
            }
        }

        if (!alive) {
            break;
        }
    }

    for (int i=0; i<MAX_THREAD; ++i) {
        if (ths[i].joinable()) {
            ths[i].join();
        }
    }

    close(_sock_fd);
    shutdown(_sock_fd, SHUT_RDWR);
    for (auto it=_client_fd.begin(); it!=_client_fd.end(); ++it) {
        close(*it);
        shutdown(*it, SHUT_RDWR);
    }
    _client_fd.clear();
    
    close(_sock_inter);

    BOOST_LOG_TRIVIAL(info) << "EpollServer shutdown >>>>>>";
}