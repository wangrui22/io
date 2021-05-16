#ifndef MI_MESSAGE_QUEUE_H
#define MI_MESSAGE_QUEUE_H

#include <vector>
#include <deque>
#include <queue>
#include <limits>

#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/noncopyable.hpp>
#include <boost/log/trivial.hpp>

template<class T>
class Queue {
public:
    virtual ~Queue() {};
    virtual size_t size() const = 0;
    virtual bool is_empty() const = 0;
    virtual void push(const T&) = 0;
    virtual void pop(T*) = 0;
    virtual T* top() = 0;
    virtual void clear() = 0;
};

template<class T>
class FIFOQueue : public Queue<T> {
public:
    FIFOQueue();
    virtual ~FIFOQueue();

    virtual size_t size() const;
    virtual bool is_empty() const;
    virtual void push(const T&);
    virtual void pop(T*);
    virtual T* top();
    virtual void clear();

    template<typename Function>
    T* find(Function selector);
    
    void refresh();
    
    template<typename Function>
    int remove(Function selector);

private:
    std::deque<T> _container;
};

template<class T , class TQueue = FIFOQueue<T>>
class MessageQueue : public boost::noncopyable {
public:
    MessageQueue(): _is_activated(false) {

    }

    virtual ~MessageQueue() {
    }

    void wait_to_push(boost::mutex::scoped_lock& locker , int time_wait_limit) {
        while (is_full()) {
            auto time = boost::get_system_time() + boost::posix_time::milliseconds(time_wait_limit);
            if (!_condition_write.timed_wait(locker , time)) {
                std::cout  << "message queue time out to push.";
            }
        }
    }

    void wait_to_pop(boost::mutex::scoped_lock& locker , int time_wait_limit) {
        while (is_empty()) {
            auto time = boost::get_system_time() + boost::posix_time::milliseconds(time_wait_limit);
            if (!_condition_read.timed_wait(locker , time)) {
                std::cout  << "message queue time out to pop.";
            }
        }
    }

    size_t capacity() const {
        return _DEFAULT_CAPACITY;
    }

    size_t size() const {
        return _container.size();
    }

    bool is_full() const {
        return _DEFAULT_CAPACITY <= _container.size();
    }

    bool is_empty() const {
        return _container.is_empty();
    }

    void activate() {
        _is_activated = true;
    }

    void deactivate() {
        if (_is_activated) {
            boost::mutex::scoped_lock locker(_mutex);

            _is_activated = false;
            _condition_read.notify_all();
            _condition_write.notify_all();
        }
    }

    bool is_activated() const {
        return _is_activated;
    }

    void push(const T& msg) {
        boost::mutex::scoped_lock locker(_mutex);

        wait_to_push(locker , _DEFAULT_TIME_WAIT_LIMIT);

        if (!is_activated()) {
            BOOST_LOG_TRIVIAL(error) <<"message queue is not activated.";
            abort();
        }

        _container.push(msg);

        _condition_read.notify_one();
    }

    void pop(T* msg) {
        boost::mutex::scoped_lock locker(_mutex);

        wait_to_pop(locker , _DEFAULT_TIME_WAIT_LIMIT);

        if (!is_activated()) {
            std::cerr  << "message queue is not activated.";
            abort();
        }

        if (!_container.is_empty()) {
            _container.pop(msg);
        }

        _condition_write.notify_one();
    }

    T* top() {
        boost::mutex::scoped_lock locker(_mutex);

        if (!is_activated()) {
            std::cerr  << "message queue is not activated.";
            abort();
        }

        if (!_container.is_empty()) {
            return _container.top();
        } else {
            return nullptr;
        }
    }

    void clear() {
        boost::mutex::scoped_lock locker(_mutex);

        if (!is_activated()) {
            std::cerr  << "message queue is not activated.";
            abort();
        }
        
        _container.clear();
    }

    template<class Selector, class Updater>
    void update_element(Selector selector, Updater updater) {
        boost::mutex::scoped_lock locker(_mutex);

        auto arr = _container.find(selector);
        if (arr.empty()) {
            return;
        }

        bool any_updated = false;
        for (auto it = arr.begin(); it != arr.end(); ++it) {
            if (*it == nullptr) continue;
            any_updated |= updater(**it);
        }

        if (any_updated) {
            _container.refresh();
        }
    }

    template<class Selector>
    int remove_element(Selector selector) {
        boost::mutex::scoped_lock locker(_mutex);

        int removed_nums = _container.remove(selector);
        if (removed_nums > 0) {
            _container.refresh();
        }
        return removed_nums;
    }

protected:
private:
    const static size_t _DEFAULT_CAPACITY = 2000;
    const static int _DEFAULT_TIME_WAIT_LIMIT = 0X7FFFFFFF;

    TQueue _container;
    bool _is_activated;

    mutable boost::mutex _mutex;
    boost::condition _condition_read;
    boost::condition _condition_write;
};

#include "mi_message_queue.inl"


#endif
