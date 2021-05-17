#include "../src/mi_uds_server.h"
#include "../src/mi_process_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <iostream>
#include <boost/thread/thread.hpp>
#include <boost/log/trivial.hpp>

namespace {

class EventEcho : public IEvent {
public:
    EventEcho(std::shared_ptr<UDSServer> server, int fd, int id):_server(server),_fd_shm(fd), _id(id) {};
    virtual ~EventEcho() {};
    virtual void execute(int) {
        BOOST_LOG_TRIVIAL(info) << "connected.";
        std::shared_ptr<UDSServer> server = _server.lock();
        if (server) {
            std::string msg = "hello I am server. I got you.";
            IPCDataHeader header;
            header.reserved0 = (unsigned int)_fd_shm;
            header.reserved1 = (unsigned int)_id;
            header.data_len = msg.size();
            server->sync_send(header, (char*)(msg.c_str()));
        }

    }
private:
    std::weak_ptr<UDSServer> _server;
    int _fd_shm;
    int _id;
};

}
#define FILE_LENGTH 0x1000 

int main(int argc, char* argv[]) {
    const int pipe_count = atoi(argv[1]);
    BOOST_LOG_TRIVIAL(info) << "pipeline begin. count : " << pipe_count;

    const size_t len = 16*sizeof(int);
    const int fd_shm = open("/tmp/shtest", O_RDWR);
    if (fd_shm < 0) {
        BOOST_LOG_TRIVIAL(error) << "open file failed.";
        return -1;
    }
    lseek(fd_shm, FILE_LENGTH, SEEK_SET);
    write(fd_shm, " ", 1);
    int offset_shm = 0;
    int* ptr = (int*)mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd_shm, offset_shm);
    if (MAP_FAILED == ptr) {
        BOOST_LOG_TRIVIAL(error) << "mmap failed";
        return -1;
    }
    for (int i=0; i<16; ++i) {
        ptr[i] = i+1;
    }

    std::shared_ptr<ProcessPool> process_pool(new ProcessPool("pipeline_worker", "./pipeline_worker")); 
    

    process_pool->add_worker(5);

    for (int i=0; i<pipe_count; ++i) {
        std::string unix_path;
        pid_t pid(-1);    
        process_pool->acquire_worker(pid, unix_path);
        std::shared_ptr<UDSServer> server(new UDSServer());
        server->register_recv_handler(std::shared_ptr<IPCDataRecvHandler>(new HandlerPrint()));
        server->on_connect(std::shared_ptr<IEvent>(new EventEcho(server, fd_shm, i)));
        server->set_path(unix_path);
        server->run();

        BOOST_LOG_TRIVIAL(info) << "pipe : " << i << " work done.";
    }

    BOOST_LOG_TRIVIAL(info) << "pipeline done.";

    process_pool->clean();

    sleep(5);

    close(fd_shm);
    munmap(ptr, len);

    BOOST_LOG_TRIVIAL(info) << "clean pipeline worker done.";


    return 0;
}
