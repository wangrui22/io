#include "../src/mi_uds_server.h"
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
    if (argc != 2) {
        BOOST_LOG_TRIVIAL(error) << "please input uds server's path";
        return -1;
    }

    const std::string path = argv[1];
    
    std::shared_ptr<UDSServer> server(new UDSServer());
    server->register_recv_handler(std::shared_ptr<IPCDataRecvHandler>(new HandlerPrint()));
    server->on_connect(std::shared_ptr<IEvent>(new EventEcho(server)));
    server->set_path(path);
    server->run();

    return 0;
}
