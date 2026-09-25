#pragma once
#include <cstddef>
struct Cache {
    bool contains(int key) const;
    void insert(int key);
    std::size_t size() const;
};
