#include "mi_server.h"

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

Server::Server() {

}

Server::~Server() {

}

void Server::listen(int port) {
    _port = port;    
}

void Server::run() {
    _sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (_sock_fd < 0) {
        BOOST_LOG_TRIVIAL(error) <<"create server's socket failed";
        return;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);

    int ret = bind(_sock_fd, (sockaddr*)(&addr), sizeof(addr));
    if (ret < 0) {
        BOOST_LOG_TRIVIAL(error) <<"server bind failed";
        return;
    }

    ret = ::listen(_sock_fd, 100);
    if (ret < 0) {
        BOOST_LOG_TRIVIAL(error) <<"server listen failed";
        return;
    }


    BOOST_LOG_TRIVIAL(info) << "server running >>>>>>";

    sockaddr_in conn_addr;    
    socklen_t conn_addr_len = sizeof(sockaddr_in);
    int connfd = accept(_sock_fd, (sockaddr*)(&conn_addr), &conn_addr_len);
    if (connfd < 0) {
        BOOST_LOG_TRIVIAL(error) <<"server accept failed";
        return;
    }   

    BufferHeader header;
    read(connfd, &header, sizeof(header));
    char* buf = new char[header.buffer_size];
    read(connfd, buf, header.buffer_size);

    std::string str(buf, header.buffer_size);
    BOOST_LOG_TRIVIAL(info) << "read client: " << str;
    delete [] buf;
    buf = nullptr;

    close(connfd);
    close(_sock_fd);

    BOOST_LOG_TRIVIAL(info) << "server shutdown >>>>>>";
}