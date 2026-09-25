#include "cache.hpp"
void prepare(Cache& cache, int key) {
    if (!cache.contains(key)) {
        cache.insert(key);
    }
}
