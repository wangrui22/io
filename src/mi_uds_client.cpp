#include "mi_uds_client.h"

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

UDSClient::UDSClient():_fd(-1), _keep_connecting(false) {

}

UDSClient::~UDSClient() {

}

void UDSClient::set_path(const std::string& path) {
    _path = path;
}

void UDSClient::set_keep_connecting(bool flag) {
    _keep_connecting = flag;
}

void UDSClient::register_recv_handler(std::shared_ptr<IPCDataRecvHandler> handler) {
    _handler = handler;
}

int UDSClient::sync_send(const IPCDataHeader& header , char* buffer) {
    if (-1 == _fd) {
        BOOST_LOG_TRIVIAL(error) << "client sync send failed. invalid fd.";
        return -1;
    }

    if (header.data_len > 0 && !buffer) {
        BOOST_LOG_TRIVIAL(error) << "client send null data failed with header data len: " << header.data_len;
        return -1;
    }

    int err = 0;
    int offset = 0;
    int rest = (int)sizeof(header);
    while (true) {
        err = ::send(_fd , &header+offset , rest , MSG_NOSIGNAL);
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
        BOOST_LOG_TRIVIAL(error) << "client send data header failed with errno: " << errno;
        return -1;
    }

    if (buffer != nullptr && header.data_len > 0) {
        int offset = 0;
        int rest = (int)header.data_len;
        while (true) {
            err = ::send(_fd , buffer+offset , rest , MSG_NOSIGNAL);
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
            BOOST_LOG_TRIVIAL(error) << "client send data failed with errno: " << errno;
            return -1;
        }
    }
    return 0;
}

void UDSClient::run() {
    if (_path.empty()) {
        BOOST_LOG_TRIVIAL(error) << "empty path";
        return;
    }

    _fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_fd < 0) {
        BOOST_LOG_TRIVIAL(error) << "create sock failed." << " err: " << errno;
        return;
    }

    sockaddr_un server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    for (size_t i=0; i<_path.length(); ++i) {
        server_addr.sun_path[i] = _path[i];
    }
    socklen_t addr_len = sizeof(server_addr);

    if (_keep_connecting) {
        while (0 != connect(_fd, (sockaddr*)(&server_addr), addr_len)) {
            //BOOST_LOG_TRIVIAL(info) << "connecting " << _path;
            sleep(1);
        }
    } else {
        if (0 != connect(_fd, (sockaddr*)(&server_addr), addr_len)) {
            BOOST_LOG_TRIVIAL(error) << "connect failed. fd: " << _fd << " err: " << errno;
            return;
        }
    }
    

    if (_ev_connect) {
        _ev_connect->execute(_fd);
    }

    while (true) {
        IPCDataHeader header;
        int recv_size = 0;
        while(true) {
            recv_size = recv(_fd, &header, sizeof(header) , MSG_WAITALL);
            if (recv_size < 0 && errno == EINTR) {
                continue;
            } else {
                break;
            }
        }
        
        if (recv_size < 0 && errno == ECONNRESET) {
            //disconnect
            BOOST_LOG_TRIVIAL(error) << "server close connect(recv a err: Connection reset by peer).";
            recv_size = 0;
        }

        if (recv_size < 0) {
            //socket error 
            BOOST_LOG_TRIVIAL(error) << "receive data header failed. errno: " << errno;
            if (_ev_error) {
                _ev_error->execute(_fd);
            }
            break;
        }
        if (recv_size == 0 && _fd == -1) {
            //closed by other thread
            BOOST_LOG_TRIVIAL(info) << "client quit because socket is closed by other thread.";
            break;
        }
        if (recv_size == 0) {
            //server disconnected
            BOOST_LOG_TRIVIAL(warning) << "client quit because server close the connect.";
            if (_ev_disconnect) {
                _ev_disconnect->execute(_fd);
            }
            _fd = -1;

            //TODO reconnect logic
            break;
        }  

        //-----------------------//
        //2 recv data
        //-----------------------//
        char* buffer = nullptr;
        bool reconnect = false;
        if (header.data_len > 0) {
            buffer = new char[header.data_len];
            int cur_size = 0;
            int accum_size = 0;
            int try_size = (int)header.data_len;
            bool quit_signal = false;
            while (accum_size < (int)header.data_len) {
                cur_size = recv(_fd, buffer+accum_size, try_size, 0);
                if (cur_size < 0 && errno == EINTR) {
                    continue;
                }

                if (cur_size < 0 && errno == ECONNRESET) {
                    //disconnect
                    BOOST_LOG_TRIVIAL(warning) << "client quit because server close the connect(recv a err: Connection reset by peer).";
                    cur_size = 0;
                }

                if (cur_size < 0) {
                    //socket error 
                    BOOST_LOG_TRIVIAL(error) << "client recv data failed.";
                    delete [] buffer;
                    buffer = nullptr;
                    quit_signal = true;
                    
                    if (_ev_error) {
                        _ev_error->execute(_fd);
                    }
                    break;
                } else if (cur_size == 0 && _fd == -1) {
                    //closed by other thread
                    BOOST_LOG_TRIVIAL(info) << "client quit because socket is closed by other thread.";
                    delete [] buffer;
                    buffer = nullptr;
                    quit_signal = true;
                    break;
                } else if (cur_size == 0) {
                    //server disconnected
                    BOOST_LOG_TRIVIAL(warning) << "client quit because server close the connect.";
                    delete [] buffer;
                    buffer = nullptr;

                    //TODO reconnect logic

                    break;
                }
                accum_size += cur_size;
                try_size -= cur_size;
            }
            
            if (quit_signal) {
                break;
            }
        }

        if (reconnect) {
            continue;
        }

        try {
            if (_handler) {
                const int err = _handler->handle(header, buffer);
                if (err == QUIT_SIGNAL) {
                    break;
                }
            } else {
                BOOST_LOG_TRIVIAL(warning) << "client handler to process received data is null.";
            }

        } catch (const std::exception& e) {
            //Ignore error to keep connecting
            BOOST_LOG_TRIVIAL(error) << "client handle command error(std ex)(skip and continue): " << e.what();
        } catch (const boost::exception& e) {
            //Ignore error to keep connecting
            BOOST_LOG_TRIVIAL(error) << "client handle command error(boost ex)(skip and continue): " << boost::diagnostic_information(e);
        }
    }

    //close socket
    if (_fd >= 0) {
        shutdown(_fd, SHUT_RDWR);
        close(_fd);   
        _fd = -1;
    }

    if (!_path.empty()) {
        unlink(_path.c_str());
        BOOST_LOG_TRIVIAL(info) << "unlink UNIX path when destruction.";
    }
    
    BOOST_LOG_TRIVIAL(info) << "uds socket client stop.";
}

void UDSClient::stop() {

}

void UDSClient::on_connect(std::shared_ptr<IEvent> ev) {
    _ev_connect = ev;
}

void UDSClient::on_disconnect(std::shared_ptr<IEvent> ev) {
    _ev_disconnect = ev;
}

void UDSClient::on_error(std::shared_ptr<IEvent> ev) {
    _ev_error = ev;
}
