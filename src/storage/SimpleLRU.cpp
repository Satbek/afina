#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    bool result = true;
    if (!is_valid_key_value(key, value)) {
            result = false;
    } else {
        if (_lru_index.count(key)) {
            result = SimpleLRU::Set(key, value);
        } else {
            result = SimpleLRU::PutIfAbsent(key, value);
        }
    }
    return result;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    bool result = true;
    if (_lru_index.count(key) || !is_valid_key_value(key, value)) {
        result = false;
    } else {
        while (!size_manager.PutIfAbsent(key, value)) {
            free_head();
        }
        if (lru_len() == 0) {
            _lru_head = std::unique_ptr<lru_node>(
                new lru_node(key, value)
            );
            _lru_tail = std::move(
                std::unique_ptr<lru_node>(
                    new lru_node("", "",
                        _lru_head.get()
                    )
                )
            );
            _lru_index.insert(make_pair(std::ref(_lru_head->key), std::ref(*_lru_head)));
        } else {
            _lru_tail->key = key;
            _lru_tail->value = value;
            auto ref = std::ref(_lru_tail->prev->next);
            ref.get() = std::move(_lru_tail);
            _lru_index.insert(make_pair(std::ref(ref.get()->key), std::ref(*ref.get())));
            _lru_tail = std::move(
                std::unique_ptr<lru_node>(
                    new lru_node("", "",
                        ref.get().get()
                    )
                )
            );
        }
    }
    return result;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    bool result = true;
    if (_lru_index.count(key) == 0 || !is_valid_key_value(key, value)) {
        result = false;
    } else {
        while (!size_manager.Set(value, _lru_index.at(key).get().value)) {
            free_head();
        }
        _lru_index.at(key).get().value = value;
    }
    return result;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    bool result = true;
    if (_lru_index.count(key) == 0) {
        result = false;
    } else {
        auto node = _lru_index.at(key);
        size_manager.Delete(node.get());
        //если вниз поставить - то сломается, WTF?!
        _lru_index.erase(key);
        if (&node.get() == _lru_head.get()) {
            _lru_head = std::move(_lru_head->next);
        } else if (&node.get() == _lru_tail->prev) {
            _lru_tail->prev = node.get().prev;
            node.get().prev->next.reset();
        } else {
            node.get().next->prev = node.get().prev;
            node.get().prev->next = std::move(node.get().next);
        }
    }
    return result;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    bool result = true;
    if (_lru_index.count(key) == 0) {
        result = false;
    } else {
        value = _lru_index.at(key).get().value;
        SimpleLRU::Delete(key);
        SimpleLRU::PutIfAbsent(key, value);
    }
    return result;
}

size_t SimpleLRU::lru_len() const {
    return _lru_index.size();
}

bool SimpleLRU::free_head() {
    if (_lru_head == nullptr) return false;
    SimpleLRU::Delete(_lru_head->key);
}

void SimpleLRU::LRU_size_manager::check_sizes() const {
    if (lru_len != lru_list_ptr->lru_len()) {
        throw std::logic_error("sizes not match!");
    }
}

bool SimpleLRU::LRU_size_manager::Set(const std::string& new_value, const std::string& old_value) {
    if (current_size - old_value.size() + new_value.size() > max_size) {
        return false;
    }
    current_size = current_size - old_value.size() + new_value.size();
    return true;
}

bool SimpleLRU::LRU_size_manager::Delete(size_t size) {
    current_size -= size;
    check_sizes();
    lru_len--;
    return true;
}

bool SimpleLRU::LRU_size_manager::Delete(const lru_node& node) {
    return LRU_size_manager::Delete(node.size());
}

bool SimpleLRU::LRU_size_manager::PutIfAbsent(size_t size) {
    if (current_size + size > max_size) {
        return false;
    } else {
        current_size += size;
        check_sizes();
        lru_len++;
        return true;
    }
}

bool SimpleLRU::LRU_size_manager::PutIfAbsent(const lru_node& node) {
    return LRU_size_manager::PutIfAbsent(node.size());
}

bool SimpleLRU::LRU_size_manager::PutIfAbsent(const std::string& key, const std::string& value) {
    return LRU_size_manager::PutIfAbsent(key.size() + value.size());
}

} // namespace Backend
} // namespace Afina
