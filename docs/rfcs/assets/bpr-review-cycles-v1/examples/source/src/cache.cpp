#include "cache.hpp"

bool Cache::contains(int key) const {
	return find(key) != entries.end();
}

void Cache::insert(int key) {
	entries.push_back(key);
}
