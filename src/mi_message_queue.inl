

template<class T>
FIFOQueue<T>::~FIFOQueue() {
    std::deque<T>().swap(_container);
}

template<class T>
FIFOQueue<T>::FIFOQueue() {
}

template<class T>
void FIFOQueue<T>::clear() {
    _container.clear();
}

template<class T>
void FIFOQueue<T>::pop(T* elem) {
    *elem = std::move(_container.front());
    _container.pop_front();
}

template<class T>
T* FIFOQueue<T>::top() {
    return &_container[_container.size() - 1];
}

template<class T>
void FIFOQueue<T>::push(const T& elem) {
    _container.push_back(elem);
}

template<class T>
bool FIFOQueue<T>::is_empty() const {
    return _container.empty();
}

template<class T>
size_t FIFOQueue<T>::size() const {
    return _container.size();
}

template<class T>
template<typename Function>
T* FIFOQueue<T>::find(Function selector) {
    for (auto it = _container.begin(); it != _container.end(); ++it) {
        if (selector(*it)) {
            return &(*it);
        }
    }
    return nullptr;
}

template<class T>
void FIFOQueue<T>::refresh() {}

template<class T>
template<typename Function>
int FIFOQueue<T>::remove(Function selector) {
    int counter = 0;
    for (auto it = _container.begin(); it != _container.end(); ) {
        if (selector(*it)) {
            it =_container.erase(it);
            ++counter;
        } else {
            ++it;
        }
    }
    return counter;
}