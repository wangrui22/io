#include "../src/mi_uds_server.h"
#include "../src/mi_process_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <boost/thread/thread.hpp>
#include <boost/log/trivial.hpp>

namespace {

class EventEcho : public IEvent {
public:
    EventEcho(std::shared_ptr<UDSServer> server):_server(server) {};
    virtual ~EventEcho() {};
    virtual void execute(int) {
        BOOST_LOG_TRIVIAL(info) << "connected.";
        std::shared_ptr<UDSServer> server = _server.lock();
        if (server) {
            std::string msg = "hello I am server. I got you.";
            IPCDataHeader header;
            header.data_len = msg.size();
            server->sync_send(header, (char*)(msg.c_str()));
        }

    }
private:
    std::weak_ptr<UDSServer> _server;
};

}


int main(int argc, char* argv[]) {
    const int pipe_count = atoi(argv[1]);
    BOOST_LOG_TRIVIAL(info) << "pipeline begin. count : " << pipe_count;

    std::shared_ptr<ProcessPool> process_pool(new ProcessPool("pipeline_worker", "./pipeline_worker")); 

    process_pool->add_worker(5);

    for (int i=0; i<pipe_count; ++i) {
        std::string unix_path;
        pid_t pid(-1);    
        process_pool->acquire_worker(pid, unix_path);
        std::shared_ptr<UDSServer> server(new UDSServer());
        server->register_recv_handler(std::shared_ptr<IPCDataRecvHandler>(new HandlerPrint()));
        server->on_connect(std::shared_ptr<IEvent>(new EventEcho(server)));
        server->set_path(unix_path);
        server->run();

        BOOST_LOG_TRIVIAL(info) << "pipe : " << i << " work done.";
    }

    BOOST_LOG_TRIVIAL(info) << "pipeline done.";

    process_pool->clean();

    sleep(5);

    BOOST_LOG_TRIVIAL(info) << "clean pipeline worker done.";


    return 0;
}
