#ifndef UMIUSI_TRYLOCK_GUARD_HPP
#define UMIUSI_TRYLOCK_GUARD_HPP

template <typename Mutex>
class TrylockGuard {
private:
    Mutex& _mutex;
    bool         _locked;

public:
    TrylockGuard(Mutex& mutex) : _mutex(mutex) {
        this->_locked = this->_mutex.trylock();
    }

    ~TrylockGuard() {
        if (this->_locked) {
            this->_mutex.unlock();
        }
    }

    auto locked() const -> bool {
        return this->_locked;
    }
};

#endif // UMIUSI_TRYLOCK_GUARD_HPP
