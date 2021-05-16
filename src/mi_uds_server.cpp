#include "mi_uds_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>
#include <boost/exception/all.hpp>

#define EAGAIN_WAIT 10000

UDSServer::UDSServer(): _server_fd(-1), _client_fd(-1) {

}

UDSServer::~UDSServer() {

}

void UDSServer::set_path(const std::string& path) {
    _path = path;
}

void UDSServer::register_recv_handler(std::shared_ptr<IPCDataRecvHandler> handler) {
    _handler = handler;
}

int UDSServer::sync_send(const IPCDataHeader& header , char* buffer) {
    if (-1 == _client_fd || -1 == _server_fd) {
        BOOST_LOG_TRIVIAL(error) << "server sync send failed. invalid fd.";
        return -1;
    }

    int err = 0;
    int offset = 0;
    int rest = (int)sizeof(header);
    while (true) {
        err = ::send(_client_fd , &header+offset , rest , MSG_NOSIGNAL);
        if (err < 0 && errno == EINTR) {
            continue;
        } else if (err < 0) {
            break;
        } else {
            offset += err;
            rest -= err;
            if (rest <= 0) {
                break;
            } else {
                continue;
            }
        }
    }
    if (-1 == err) {
        BOOST_LOG_TRIVIAL(error) << "server send data header failed with errno: " << errno;
        return -1;
    }

    if (buffer != nullptr && header.data_len > 0) {
        int offset = 0;
        int rest = (int)header.data_len;
        while (true) {
            err = ::send(_client_fd , buffer+offset , rest , MSG_NOSIGNAL);
            if (err < 0 && errno == EINTR) {
                continue;
            } else if (err < 0) {
                break;
            } else {
                offset += err;
                rest -= err;
                if (rest <= 0) {
                    break;
                } else {
                    continue;
                }
            }
        }
        if (-1 == err) {
            BOOST_LOG_TRIVIAL(error) << "server send data failed with errno: " << errno;
            return -1;
        }
    }
    return 0;
}

void UDSServer::run() {
    if (_path.empty()) {
        BOOST_LOG_TRIVIAL(error) << "empty path";
        return;
    }

    _server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_server_fd < 0) {
        BOOST_LOG_TRIVIAL(error) << "create sock failed." << " err: " << errno;
        return;
    }

    sockaddr_un server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    for (int i=0; i<_path.length(); ++i) {
        server_addr.sun_path[i] = _path[i];
    }
    socklen_t addr_len = sizeof(server_addr);

    if (0 != bind(_server_fd, (sockaddr*)(&server_addr), addr_len)) {
        BOOST_LOG_TRIVIAL(error) << "bind failed. fd: " << _server_fd << " err: " << errno;
        return;
    }

    if (0 != listen(_server_fd, 5)) {
        BOOST_LOG_TRIVIAL(error) << "listen failed. fd: " << _server_fd << " err: " << errno;
        return;
    }
    
    //set nonblocking
    fcntl(_server_fd, F_SETFD, (fcntl(_server_fd, F_GETFL) | O_NONBLOCK));

    BOOST_LOG_TRIVIAL(error) << "uds fd: " << _server_fd << " listen >>>>> ";

    sockaddr_un client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    _client_fd = accept(_server_fd, (sockaddr*)(&client_addr), &client_addr_len);

    if (_client_fd < 0) {
        BOOST_LOG_TRIVIAL(error) << "listen failed. fd: " << _server_fd  << " err: " << errno;
        return;
    }

    if (_ev_connect) {
        _ev_connect->execute(_client_fd);
    }

    while (true) {
        //-----------------------//
        //1 recv header
        //-----------------------//
        IPCDataHeader header;
        char* header_buf = (char*)(&header);
        int header_rest = sizeof(header);
        //header is just 32 byte,use MSG_WAITALL to force client socket to return untill recv all header buffer(32byte) 
        int err = 0;
        while(true) {
            err = ::recv(_client_fd, header_buf, header_rest , 0);
            const int err_code = errno;
            if (err < 0 && err_code == EINTR) {
                continue;
            } else if (err < 0 && err_code == EAGAIN) {
                usleep(EAGAIN_WAIT);
                continue;
            } else if (err <= 0) {
                break;
            } else {
                header_buf += err;
                header_rest -= err;
                if (header_rest <= 0) {
                    break;
                } else {
                    continue;
                }
            }
        }

        //set client socket id/created_time to header to make operation know which socket to interact
        header.receiver = _client_fd;//client socket id
        //header.reserved1 = time;//client socket created time

        //client disconnect
        if (err == 0) {
            BOOST_LOG_TRIVIAL(info) << "client unix socket disconnect. " << "path: " << _path << ". " << _server_fd << "/" << _client_fd;
            //shutdown
            if (_ev_disconnect) {
                _ev_disconnect->execute(_client_fd);
            }
            break;    
        } 
        if (err < 0 && errno == ECONNRESET) {
            //disconnect
            BOOST_LOG_TRIVIAL(warning) << "client unix socket disconnect. " << "path: " << _path << ". " << _server_fd << "/" << _client_fd <<  ". because server close the connect(recv a err: Connection reset by peer).";
            //shutdown
            if (_ev_disconnect) {
                _ev_disconnect->execute(_client_fd);
            }
            break;
        }
        
        //recv error
        if (err < 0) {
            BOOST_LOG_TRIVIAL(error) << "client unix socket recv failed. errno: " << errno;
            //shutdown
            if (_ev_error) {
                _ev_error->execute(_client_fd);
            }
            break;  
        }

        //-----------------------//
        //2 recv data
        //-----------------------//
        char* buffer = nullptr;
        if (header.data_len > 0) {
            //loop to recv data
            buffer = new char[header.data_len];
            int cur_size = 0;
            int accum_size = 0;
            int try_size = (int)header.data_len;
            while (accum_size < (int)header.data_len) {
                cur_size = ::recv(_client_fd, buffer+accum_size, try_size, 0);
                const int err_code = errno;
                if (cur_size < 0 && err_code == EINTR) {
                    continue;
                } else if (cur_size < 0 && err_code == EAGAIN) {
                    usleep(EAGAIN_WAIT);
                    continue;
                }
                if (cur_size <= 0) {
                    BOOST_LOG_TRIVIAL(error) << "client receive data buffer failed. errno: " << errno;
                    break;
                }
                accum_size += cur_size;
                try_size -= cur_size;
                if (try_size <= 0) {
                    break;
                } else {
                    continue;
                }
            }

            //client disconnect
            if (cur_size == 0) {
                BOOST_LOG_TRIVIAL(info) << "client unix socket disconnect. " << "path: " << _path << ". " << _server_fd << "/" << _client_fd;
                delete [] buffer;
                buffer = nullptr;
                //shutdown
                if (_ev_disconnect) {
                    _ev_disconnect->execute(_client_fd);
                }
                break;
            }
            if (cur_size < 0 && errno == ECONNRESET) {
                //disconnect
                BOOST_LOG_TRIVIAL(warning) << "client unix socket disconnect. " << "path: " << _path << ". " << _server_fd << "/" << _client_fd <<  ". because server close the connect(recv a err: Connection reset by peer).";
                //shutdown
                if (_ev_disconnect) {
                    _ev_disconnect->execute(_client_fd);
                }
                break;
            }
            
            
            //recv error
            if (cur_size < 0) {
                BOOST_LOG_TRIVIAL(error) << "client unix socket recv failed. errno: " << errno;

                delete [] buffer;
                buffer = nullptr;
                //shutdown
                if (_ev_error) {
                    _ev_error->execute(_client_fd);
                }
                break;  
            }
        }

        //MI_UTIL_LOG(MI_DEBUG) << "server " << _server_info << " recv package: {MsgID: " << header.msg_id << "; OpID: " << header.op_id << "}";

        try {
            if (_handler) {
                const int err = _handler->handle(header, buffer);
                if (err == QUIT_SIGNAL) {
                    break;
                }
            } else {
                BOOST_LOG_TRIVIAL(error) << "server handler to process received data is null.";
            }
        } catch(const std::exception& e) {
            //Ignore error to keep connecting
            BOOST_LOG_TRIVIAL(error) << "server handle command error(skip and continue): " << e.what();
        } catch (const boost::exception& e) {
            //Ignore error to keep connecting
            BOOST_LOG_TRIVIAL(error) << "server handle command error(boost ex)(skip and continue): " << boost::diagnostic_information(e);
        }
    }

    //close socket
    if (_client_fd >= 0) {
        shutdown(_client_fd, SHUT_RDWR);
        close(_client_fd);   
        _client_fd = -1;
    }

    if (_server_fd >= 0) {
        shutdown(_server_fd, SHUT_RDWR);
        close(_server_fd);   
        _server_fd = -1;
    }

    if (!_path.empty()) {
        unlink(_path.c_str());
        BOOST_LOG_TRIVIAL(info) << "unlink UNIX path when destruction.";
    }
    
    BOOST_LOG_TRIVIAL(info) << "uds socket client stop.";
}

void UDSServer::stop() {

}

void UDSServer::on_connect(std::shared_ptr<IEvent> ev) {
    _ev_connect = ev;
}

void UDSServer::on_disconnect(std::shared_ptr<IEvent> ev) {
    _ev_disconnect = ev;
}

void UDSServer::on_error(std::shared_ptr<IEvent> ev) {
    _ev_error = ev;
}
