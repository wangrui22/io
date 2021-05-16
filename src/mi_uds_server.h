#ifndef MI_UDS_SERVER_H
#define MI_UDS_SERVER_H

#include <string>
#include <memory>
#include "mi_rpc_common.h"

class UDSServer {
public:
    UDSServer();
    ~UDSServer();

    void set_path(const std::string& path);
    
    void register_recv_handler(std::shared_ptr<IPCDataRecvHandler> handler);

    int sync_send(const IPCDataHeader& header , char* buffer);
    
    void run();
    void stop();

    void on_connect(std::shared_ptr<IEvent> ev);
    void on_disconnect(std::shared_ptr<IEvent> ev);
    void on_error(std::shared_ptr<IEvent> ev);

private:
    int _server_fd;
    int _client_fd;
    std::string _path;
    std::shared_ptr<IPCDataRecvHandler> _handler;

    std::shared_ptr<IEvent> _ev_connect;
    std::shared_ptr<IEvent> _ev_disconnect;
    std::shared_ptr<IEvent> _ev_error;
};

#endif