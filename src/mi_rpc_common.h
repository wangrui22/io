#ifndef MI_RPC_COMMON_H
#define MI_RPC_COMMON_H

#include<boost/log/trivial.hpp>


struct BufferHeader {
    int src_socket;
    int dst_socket;
    int buffer_size;
    int ext0;

    BufferHeader() {
        src_socket = 0;
        dst_socket = 0;
        buffer_size = 0;
        ext0 = 0;
    }
};

struct RPCPackage {
    BufferHeader header;
    char* data_buffer;
    bool init;

    RPCPackage() {
        data_buffer = nullptr;
        init = false;
    }
};


//UDS
struct IPCDataHeader { //32 byte
    unsigned int sender;//sender pid or socket id ... 
    unsigned int receiver;//receiver pid or socket id ... (should be set when recv)
    unsigned int msg_id;//message ID : thus command ID
    unsigned int cell_id;//message info : client cell ID, client socket time 
    unsigned int op_id;//message info : client operation ID
    unsigned int reserved0;//message info : reserved sequenced msg end tag(EG: send n slice dicom series. 0~n-2:0 n-1:1)
    unsigned int reserved1;//message info : reserved
    unsigned int data_len;//data length

    IPCDataHeader():
        sender(0), receiver(0), msg_id(0), cell_id(0),
        op_id(0), reserved0(0), reserved1(0), data_len(0) {
    }
};

class IPCDataRecvHandler {
public:
    IPCDataRecvHandler() {};
    virtual ~IPCDataRecvHandler() {};
    virtual int handle(const IPCDataHeader& header , char* buffer) = 0;
protected:
private:
};

class IEvent {
public:
    IEvent() {};
    virtual ~IEvent() {};
    virtual void execute(int) = 0;
private:
};

#define QUIT_SIGNAL 0x990077

class HandlerPrint : public IPCDataRecvHandler {
public:
    HandlerPrint() {};
    virtual ~HandlerPrint() {};
    virtual int handle(const IPCDataHeader& header , char* buffer) {
        if (buffer && header.data_len > 0) {
            const std::string msg = std::string(buffer, header.data_len);
            BOOST_LOG_TRIVIAL(info) << "recv msg: " << msg;
            if (msg == "quit") {
                return QUIT_SIGNAL;
            }
        }
        return 0;
    }
};

#endif