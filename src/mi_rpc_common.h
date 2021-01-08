#ifndef MI_RPC_COMMON_H
#define MI_RPC_COMMON_H


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


#endif