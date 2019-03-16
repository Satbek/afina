#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <exception>
#include <afina/Storage.h>

#include<iostream>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */

class SimpleLRU : public Afina::Storage {
public:
    SimpleLRU(size_t max_size = 1024) : _max_size(max_size),
        size_manager(max_size, this) {}

    ~SimpleLRU() {
        _lru_index.clear();
        //attach tail
        _lru_tail->prev->next = std::move(_lru_tail);
        for (auto ptr = std::move(_lru_head); ptr; ptr = std::move(ptr->next)) {}
    }

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override;

    size_t lru_len () const;
private:
    bool is_valid_key_value(const std::string& key, const std::string& value) const {
        return key.size() + value.size() <= _max_size;
    }

    using lru_node = struct lru_node {
        std::string key;
        std::string value;
        lru_node* prev;
        std::unique_ptr<lru_node> next;
        lru_node(const std::string& key_, const std::string& value_,
            lru_node* prev_ = nullptr, lru_node* next_ = nullptr)
            : key(key_), value(value_), prev(prev_), next(std::unique_ptr<lru_node> (next_)) {}

        size_t size() const {
            return key.size() + value.size();
        }
        // ~lru_node() {
        //     std::cout << "~lru_node()" << " " << "key = " << key << "| value = " << value << std::endl;
        // }
    };

    class LRU_size_manager {
    public:
        LRU_size_manager(const size_t max_size_, SimpleLRU* lru_list)
            : max_size(max_size_), lru_list_ptr(lru_list), current_size(0), lru_len(0) {}
        bool Set(const std::string& new_value, const std::string& old_value);
        bool Delete(const lru_node& node);
        bool Delete(size_t size);
        bool PutIfAbsent(const lru_node& node);
        bool PutIfAbsent(const std::string& key, const std::string& value);
        bool PutIfAbsent(size_t size);
    private:
        void check_sizes() const;
        size_t max_size;
        size_t current_size;
        size_t lru_len;
        SimpleLRU* lru_list_ptr;
    };

    LRU_size_manager size_manager;
    bool free_head();

    // Maximum number of bytes could be stored in this cache.
    // i.e all (keys+values) must be less the _max_size
    std::size_t _max_size;

    // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
    // element that wasn't used for longest time.
    //
    // List owns all nodes
    std::unique_ptr<lru_node> _lru_head;
    std::unique_ptr<lru_node> _lru_tail;

    // Index of nodes from list above, allows fast random access to elements by lru_node#key
    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>> _lru_index;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
