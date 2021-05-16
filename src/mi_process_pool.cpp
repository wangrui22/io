#include "mi_process_pool.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>

#include <boost/thread/thread.hpp>

#include "uuid/uuid.h"

void sig_child(int signo) {
    pid_t pid(-1);
    int stat(0);
    pid = wait(&stat);
    BOOST_LOG_TRIVIAL(info) << "child process: " << pid << " exit."; 
}

std::string uuid() {
    uuid_t uu;
    char buf[1024];
    uuid_generate(uu);
    uuid_unparse(uu,buf);
    std::string ss;
    return std::string(buf);
}

int ProcessPool::add_worker(int num) {
    boost::mutex::scoped_lock locker(_mutex);
    const int num_per = (int)_worker.size();
    int num_cur = num_per;
    for (int i=0; i<num; ++i) {
        const std::string uid = uuid();
        mkdir("/tmp/medipc", S_IRWXU);
        const std::string unix_path = "/tmp/medipc/" + _worker_name + "-" + uid;

        pid_t pid = vfork();
        if (pid<0) {
            BOOST_LOG_TRIVIAL(error) << "failed to create worker container.";
            return -1;
        } else if (0 == pid) {
            BOOST_LOG_TRIVIAL(info) << "creating a new uds worker container.";    
            const char * const argv[ ] = {
                _worker_path.c_str(),
                unix_path.c_str(), 
                "keepconnect",
                NULL};
            const std::string path_env = std::string("PATH=") + std::string(getenv("PATH"));
            const char* envs[] = {path_env.c_str(), NULL};
            if (execvpe(_worker_path.c_str(), const_cast<char* const *>(argv), const_cast<char* const *>(envs)) < 0) {
                BOOST_LOG_TRIVIAL(error) << "creating a new worker failed.";
                //exit(1);
            }
        } else {
            BOOST_LOG_TRIVIAL(debug) << "creating a new uds worker success. {pid: " << pid << ", uid:" << uid << "}";
            for (auto it = _worker.begin(); it != _worker.end(); ++it) {
                if (it->first == pid) {
                    BOOST_LOG_TRIVIAL(error) << "pid: " << pid << " is already in pool. erase it.";
                    _worker.erase(it);
                    break;
                }
            }
            _worker.push_back(std::make_pair(pid, unix_path));

            ++num_cur;
        }
    }

    const int success = num_cur - num_per;
    if (success != num) {
        BOOST_LOG_TRIVIAL(error) << "worker pool: " << _worker_name << " add worker: " << num << " done. failed: " << num - success;
        return success;
    } else {
        BOOST_LOG_TRIVIAL(info) << "worker pool: " << _worker_name << " add worker: " << num << " success.";
        return num;
    }
}

int ProcessPool::acquire_worker(int& pid, std::string& unix_path) {
    bool pool_empty = false;
    {
        boost::mutex::scoped_lock locker(_mutex);
        pool_empty = _worker.empty();
    }
    if (pool_empty) {
        BOOST_LOG_TRIVIAL(warning) << "workder pool empty, create a worker before acquire.";
        add_worker(1);
    }

    BOOST_LOG_TRIVIAL(info) << "acquire workder: " << _worker_path;
    {
        boost::mutex::scoped_lock locker(_mutex);
        unix_path = "";
        pid = -1;
        
        if (!_worker.empty()) {
            auto it = _worker.begin();
            pid = it->first;
            unix_path = it->second;
            _worker.erase(it);
        } else {
            return -1;
        }
    }
    if (!pool_empty) {
        boost::thread th([this]() {
            this->add_worker(1);
        });
        th.detach();
    }

    if (kill(pid, 0) != 0) {
        return -1;
    } else {
        return 0;
    }
}

int ProcessPool::check_valid() {
    boost::mutex::scoped_lock locker(_mutex);
    int valid = 0;
    for (auto it = _worker.begin(); it != _worker.end(); ) {
        if (kill(it->first, 0) != 0) {
            ++valid;
            it = _worker.erase(it);
        } else {
            ++it;
        }
    }
    return valid;
}

void ProcessPool::print_status() {
    boost::mutex::scoped_lock locker(_mutex);
    for (auto it = _worker.begin(); it != _worker.end(); ++it) {
        BOOST_LOG_TRIVIAL(info) << "worker path: " << _worker_path << ", name: " << _worker_name << ", pid: " << it->first;
    }
}

int ProcessPool::clean() {
    for (auto it = _worker.begin(); it != _worker.end(); ++it) {
        kill(it->first, SIGKILL);
    }
}