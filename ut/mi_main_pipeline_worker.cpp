#include "../src/mi_uds_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/memfd.h>
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
            //1 read shm from  parent process 
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
            
            std::shared_ptr<UDSClient> client = _client.lock();
            if (!client) {
                return 0;
            }

            sleep(1);
            BOOST_LOG_TRIVIAL(info) << "";
            std::vector<int*> map_buffer;
            //2 create shm and send to parent process
            {
                std::vector<int> fds;
                const int shm_count = 4;
                for (int i=0; i<shm_count; ++i) {
                    const size_t len = 16*sizeof(int);
                    const int fd_shm = syscall(SYS_memfd_create,"pipeline", MFD_ALLOW_SEALING);
                    if (-1 == fd_shm) {
                        BOOST_LOG_TRIVIAL(error) << "create memfd failed.";
                        return -1;
                    }
                    BOOST_LOG_TRIVIAL(info) << "create memfd success.";
                    fds.push_back(fd_shm);
                    lseek(fd_shm, 0x1000, SEEK_SET);
                    write(fd_shm, " ", 1);
                    int offset_shm = 0;
                    int* ptr2 = (int*)mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd_shm, offset_shm);
                    if (MAP_FAILED == ptr2) {
                        BOOST_LOG_TRIVIAL(error) << "mmap failed";
                        return -1;
                    }
                    *ptr2 = *(ptr+header.reserved1) + 10*i;

                    //munmap(ptr2, 16*sizeof(int));
                    map_buffer.push_back(ptr2);

                    BOOST_LOG_TRIVIAL(info) << "worker create shm, value: " << *ptr2 << "  " << *(ptr+header.reserved1);
                }

                for(int* ptr2 : map_buffer) {
                    munmap(ptr2, 16*sizeof(int));
                }
                

                
                
                IPCDataHeader header_ret;
                std::string msg = "worker send shm";
                header_ret.data_len = msg.size();
                
                BOOST_LOG_TRIVIAL(info) << "worker begin send fd count: " << fds.size();
                client->sync_send_fd(header_ret, (char*)(msg.c_str()), fds);

                sleep(1);
                BOOST_LOG_TRIVIAL(info) << "";

            }

            //3 send quit signal
            if (client) {
                IPCDataHeader header_ret;
                std::string msg = "quit";
                header_ret.data_len = msg.size();
                client->sync_send(header_ret, (char*)(msg.c_str()));
            }

            

            munmap(ptr, 16*sizeof(int));

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
