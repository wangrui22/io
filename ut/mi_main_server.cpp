#include "../src/mi_server.h"
#include "../src/mi_select_server.h"
#include "../src/mi_epoll_server.h"
#include "../src/mi_epoll_server_single.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <boost/thread/thread.hpp>
#include <boost/log/trivial.hpp>

EpollServer* server = nullptr;

static void echo() {
    int v;
    while (std::cin >> v) {
        if (v == 1) {
            if (server) {
                server->stop();
            }
        }
        /* code */
    }
    
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        BOOST_LOG_TRIVIAL(info) << "please input server's ip";
        return -1;
    }

    const int port = atoi(argv[1]);

    boost::thread th(echo);
    th.detach();

    server = new EpollServer();
    
    server->listen(port);
    server->run();
    
    delete server;

    return 0;
}
