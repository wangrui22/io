#include "../src/mi_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include <boost/thread/thread.hpp>
#include <boost/log/trivial.hpp>

#define CLIENT_NUM 2048
#define THREAD_NUM 32
boost::mutex MUTEX;

static void test_client(int idx, const std::string& ip, int port) {
    for (int i=0; i<CLIENT_NUM; ++i) {
        {   
            boost::mutex::scoped_lock locker(MUTEX);
            BOOST_LOG_TRIVIAL(info) << "run :" << idx << " idx: " << i;
        }
        
        Client client;
        client.set_host(ip, port);
        client.run();
        //sleep(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        BOOST_LOG_TRIVIAL(info) << "please input server's ip port";
        return -1;
    }

    std::string ip(argv[1]);
    int port(atoi(argv[2]));

    // Client client;
    // client.set_host(ip, port);
    // client.run();

    // return 0;

    std::vector<boost::thread> ths;
    for (int i=0; i<THREAD_NUM; ++i) {
        ths.push_back(boost::thread(test_client, i, ip, port));
    }

    for (int i=0; i<THREAD_NUM; ++i) {
        if (ths[i].joinable()) {
            ths[i].join();
        }
    }

    

    return 0;
}
