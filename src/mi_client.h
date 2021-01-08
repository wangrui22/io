#ifndef MI_CLIENT_H
#define MI_CLIENT_H

#include <string>

class Client {
public:
    Client();
    ~Client();
    void set_host(const std::string& ip, const int port);
    void run();

private:
    std::string _ip;
    int _port;
    int _sock_fd;
};

#endif