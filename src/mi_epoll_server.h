#ifndef MI_EPOLL_SERVER_H
#define MI_EPOLL_SERVER_H

#include <vector>
#include <set>
#include <boost/thread/mutex.hpp>
#include "mi_message_queue.h"
#include "mi_rpc_common.h"

class EpollServer {
public:
    EpollServer();
    ~EpollServer();

    void listen(int port);
    void run();
    void stop();
    void send(int sockfd, char* buffer, int buffer_size);

private:
    void recv_worker(int idx);
    void add_client(int efd, int fd);
    void remove_client(int efd, int fd);

    int _port;
    int _sock_fd;
    int _efd;
    int _sock_inter;
    bool _alive;

    boost::mutex _mutex;

    std::vector<std::shared_ptr<MessageQueue<int>>> _msg_queue;
    std::vector<int> _worker_efd;
    std::set<int> _client_fd;
};



#endif