#ifndef MI_UDS_SERVER_H
#define MI_UDS_SERVER_H

#include <string>
#include <memory>
#include <atomic>
#include "mi_rpc_common.h"

class UDSClient {
public:
    UDSClient();
    ~UDSClient();

    void set_path(const std::string& path);
    void set_keep_connecting(bool flag);
    
    void register_recv_handler(std::shared_ptr<IPCDataRecvHandler> handler);

    int sync_send(const IPCDataHeader& header , char* buffer);
    
    void run();
    void stop();

    void on_connect(std::shared_ptr<IEvent> ev);
    void on_disconnect(std::shared_ptr<IEvent> ev);
    void on_error(std::shared_ptr<IEvent> ev);

private:
    int _fd;
    std::string _path;
    std::shared_ptr<IPCDataRecvHandler> _handler;

    std::shared_ptr<IEvent> _ev_connect;
    std::shared_ptr<IEvent> _ev_disconnect;
    std::shared_ptr<IEvent> _ev_error;

    bool _keep_connecting;
};

#endif