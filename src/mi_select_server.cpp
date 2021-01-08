#include "mi_select_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>

#include <iostream>
#include <sstream>

#include "mi_rpc_common.h"

SelectServer::SelectServer() {

}

SelectServer::~SelectServer() {

}

void SelectServer::listen(int port) {
    _port = port;    
}

void SelectServer::run() {
    _sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (_sock_fd < 0) {
        std::cerr << "create SelectServer's socket failed\n";
        return;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);

    int ret = bind(_sock_fd, (sockaddr*)(&addr), sizeof(addr));
    if (ret < 0) {
        std::cerr << "SelectServer bind failed\n";
        return;
    }

    ret = ::listen(_sock_fd, 100);
    if (ret < 0) {
        std::cerr << "SelectServer listen failed\n";
        return;
    }


    std::cout << "SelectServer running >>>>>>\n";

    sockaddr_in conn_addr;    
    socklen_t conn_addr_len = sizeof(sockaddr_in);
    int connfd = accept(_sock_fd, (sockaddr*)(&conn_addr), &conn_addr_len);
    if (connfd < 0) {
        std::cerr << "SelectServer accept failed\n";
        return;
    }   

    char buf[1204];
    fd_set read_fds;
    fd_set exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&exception_fds);

    while (1) {
        
        memset(buf, 0, sizeof(buf));
        FD_SET(connfd, &read_fds);
        FD_SET(connfd, &exception_fds);

        ret = select(connfd+1, &read_fds, NULL, &exception_fds, NULL);
        if (ret < 0) {
            std::cout << "selection failed.\n";
            break;
        }

        if (FD_ISSET(connfd, &read_fds)) {
            BufferHeader header;
            ret = read(connfd, &header, sizeof(header));
            if (ret != sizeof(header)) {
                std::cout << "read client: " << ret << " err: " << errno <<  std::endl; 
                break;
            }
            char* data_buf = new char[header.buffer_size];
            read(connfd, data_buf, header.buffer_size);

            std::string str(data_buf, header.buffer_size);
            std::cout << "read client: " << str << std::endl;
            delete [] data_buf;
            data_buf = nullptr;
        } else if (FD_ISSET(connfd, &exception_fds)) {
            //异常事件的
            std::cout << "oob data." << std::endl;
            break;
        }
    }
    

    

    close(connfd);
    close(_sock_fd);

    std::cout << "SelectServer shutdown >>>>>>\n";
}