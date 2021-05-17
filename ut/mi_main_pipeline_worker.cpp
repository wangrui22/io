#include "../src/mi_uds_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <iostream>
#include <sstream>
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

            // msg = "quit";
            // header.data_len = msg.size();
            // client->sync_send(header, (char*)(msg.c_str()));
        }

    }
private:
    std::weak_ptr<UDSClient> _client;
};

class HandlerPipeWorker : public IPCDataRecvHandler {
public:
    HandlerPipeWorker(std::shared_ptr<UDSClient> client) :_client(client) {};
    virtual ~HandlerPipeWorker() {};
    virtual int handle(const IPCDataHeader& header , char* buffer) {
        if (buffer && header.data_len > 0) {
            const std::string msg = std::string(buffer, header.data_len);
            BOOST_LOG_TRIVIAL(info) << "worker recv msg: " << msg;

            int* ptr = (int*)mmap(NULL, 16*sizeof(int), PROT_READ, MAP_SHARED, header.reserved0, 0);
            if (MAP_FAILED == ptr) {
                BOOST_LOG_TRIVIAL(error) << "mmap failed. fd: " << header.reserved0 << " data offset: " << header.reserved1;
                return 0;
            } else {
                BOOST_LOG_TRIVIAL(info) << "mmap sucesss. fd: " << header.reserved0 << " data offset: " << header.reserved1;
            }

            BOOST_LOG_TRIVIAL(info) << "worker read shared memmory on fd: " << header.reserved0 << " idx: " << header.reserved1 << " value: " <<  *(ptr+header.reserved1);

            munmap(ptr, 16*sizeof(int));

            std::shared_ptr<UDSClient> client = _client.lock();
            if (client) {
                IPCDataHeader header_ret;
                std::string msg = "quit";
                header_ret.data_len = msg.size();
                client->sync_send(header_ret, (char*)(msg.c_str()));
            }

            if (msg == "quit") {
                return QUIT_SIGNAL;
            }
        }
        return 0;
    }
private:
    std::weak_ptr<UDSClient> _client;
};

}


int main(int argc, char* argv[]) {
    std::string path = (argc > 1) ? argv[1] : "";
    std::string puid = path;
    for (int i = static_cast<int>(path.size()) - 1; i >= 0; --i) {
        if (path[i] == '/') {
            puid = path.substr(i+1 , path.size() - i - 1);
            break;
        }
    }

    const bool keep_connect = argc>2 && std::string(argv[2]) == "keepconnect";
    
    std::shared_ptr<UDSClient> client(new UDSClient());
    client->register_recv_handler(std::shared_ptr<IPCDataRecvHandler>(new HandlerPipeWorker(client)));
    client->on_connect(std::shared_ptr<IEvent>(new EventEcho(client)));
    client->set_path(path);
    client->set_keep_connecting(keep_connect);
    client->run();

    return 0;
}
