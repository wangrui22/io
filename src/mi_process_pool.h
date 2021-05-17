#ifndef MI_PROCESS_POLL
#define MI_PROCESS_POLL


#include <string>
#include <map>
#include <deque>
#include <boost/thread/mutex.hpp>
#include <unistd.h>
#include <signal.h>
#include "mi_rpc_common.h"

class ProcessPool {
public: 
    ProcessPool(const std::string& worker_name, const std::string& worker_path)
    :_worker_name(worker_name), _worker_path(worker_path) {
        signal(SIGCHLD, SIG_IGN);
    }

    ~ProcessPool() {}

    int add_worker(int num);

    int acquire_worker(pid_t& pid, std::string& back);

    int check_valid();

    void clean();

    void print_status();

private:
    std::deque<std::pair<pid_t, std::string>> _worker;
    boost::mutex _mutex;
    std::string _worker_name;
    std::string _worker_path;
};

#endif