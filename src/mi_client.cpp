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
        BOOST_LOG_TRIVIAL(error) <<"create client's socket failed";
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
        BOOST_LOG_TRIVIAL(error) <<"connect server failed: " << errno;
        return;
    } 

    BOOST_LOG_TRIVIAL(error) <<"connect server success >>>>>>";

    std::string str("hello");   
    BufferHeader header;
    header.buffer_size = str.length();

    write(_sock_fd, &header, sizeof(header));
    write(_sock_fd, str.c_str(), str.length());
    
    //sleep(2);

    //shutdown(_sock_fd, SHUT_RDWR);
    close(_sock_fd);

    BOOST_LOG_TRIVIAL(info) << "close client";
    
}
