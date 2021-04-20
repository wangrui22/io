#include "mi_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <iostream>
#include <sstream>

#include "mi_rpc_common.h"

Client::Client() {

}

Client::~Client() {

}

void Client::set_host(const std::string& ip, const int port) {
    _ip = ip;
    _port = port;
}

void Client::run() {
    _sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (_sock_fd < 0) {
        std::cerr << "create client's socket failed\n";
        return;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    inet_pton(AF_INET, _ip.c_str(), &addr.sin_addr);

    socklen_t addr_len = sizeof(addr);
    int ret = connect(_sock_fd, (sockaddr*)(&addr), addr_len);
    if (ret < 0) {
        std::cerr << "connect server failed: " << errno << "\n";
        return;
    } 

    std::cerr << "connect server success >>>>>>\n";

    BufferHeader header;
    // std::string str("hello");
    // header.buffer_size = str.length()+1;
    
    for (int i=0; i<1000 ; ++i) {
        std::stringstream ss;
        ss << "hello" << i;
        const std::string str = ss.str();
        header.buffer_size = str.length();
        write(_sock_fd, &header, sizeof(header));
        write(_sock_fd, str.c_str(), str.length());
    }
    
    
    //sleep(10);

    shutdown(_sock_fd, SHUT_RDWR);
    close(_sock_fd);

    std::cout << "close client\n";
    
}
