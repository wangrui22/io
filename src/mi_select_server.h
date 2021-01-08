#ifndef MI_SELECT_SERVER_H
#define MI_SELECT_SERVER_H

class SelectServer {
public:
    SelectServer();
    ~SelectServer();

    void listen(int port);
    void run();

private:
    int _port;
    int _sock_fd;
};



#endif