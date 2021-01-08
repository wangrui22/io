#ifndef MI_SERVER_H
#define MI_SERVER_H

class Server {
public:
    Server();
    ~Server();

    void listen(int port);
    void run();

private:
    int _port;
    int _sock_fd;
};



#endif