#include "../src/mi_uds_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <boost/thread/thread.hpp>
#include <boost/log/trivial.hpp>

namespace {

class EventEcho : public IEvent {
public:
    EventEcho(std::shared_ptr<UDSClient> client):_client(client) {};
    virtual ~EventEcho() {};
    virtual void execute(int) {
        BOOST_LOG_TRIVIAL(info) << "connected.";
        std::shared_ptr<UDSClient> client = _client.lock();
        if (client) {
            std::string msg = "hello I am client. I got you.";
            IPCDataHeader header;
            header.data_len = msg.size();
            client->sync_send(header, (char*)(msg.c_str()));
        }

    }
private:
    std::weak_ptr<UDSClient> _client;
};

}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        BOOST_LOG_TRIVIAL(error) << "please input uds server's path";
        return -1;
    }

    const std::string path = argv[1];
    
    std::shared_ptr<UDSClient> client(new UDSClient());
    client->register_recv_handler(std::shared_ptr<IPCDataRecvHandler>(new HandlerPrint()));
    client->on_connect(std::shared_ptr<IEvent>(new EventEcho(client)));
    client->set_path(path);
    client->run();

    return 0;
}
