#ifndef MI_EPOLL_SERVER_SINGLE_H
#define MI_EPOLL_SERVER_SINGLE_H

#include <vector>
#include <set>
#include "mi_message_queue.h"
#include "mi_rpc_common.h"

class EpollServerSingle {
public:
    EpollServerSingle();
    ~EpollServerSingle();

    void listen(int port);
    void run();

private:
    int _port;
    int _sock_fd;
    int _efd;
};



#endif