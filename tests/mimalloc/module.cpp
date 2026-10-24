#include <QByteArray>

#include <cstdlib>
#include <new>

// Deliberately links Qt, never mimalloc: these are loaded-library calls.
extern "C" void* bpr_module_allocate(std::size_t aSize)
{
	return std::calloc(aSize, 1);
}

extern "C" void bpr_module_release(void* aPointer)
{
	std::free(aPointer);
}

extern "C" void* bpr_module_new(std::size_t aSize)
{
	return ::operator new(aSize);
}

extern "C" void bpr_module_delete(void* aPointer)
{
	::operator delete(aPointer);
}

extern "C" void* bpr_module_qt_data()
{
	static QByteArray bytes(32768, 'q');
	return bytes.data();
}
