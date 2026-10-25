#ifdef _WIN32
	#define NOMINMAX
	#include <Windows.h>

	#include <Psapi.h>
	#include <malloc.h>
#else
	#include <dlfcn.h>
	#ifdef __APPLE__
		#include <mach-o/dyld.h>
	#else
		#include <link.h>
	#endif
#endif

#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <thread>

namespace {
	using owns_pointer = bool (*)(const void*);

	void require(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::fprintf(stderr, "MIMALLOC_PROBE_FAIL %s\n", aMessage);
			std::exit(1);
		}
	}

	template <class T>
	T symbol(void* aModule, const char* aName)
	{
#ifdef _WIN32
		auto result = std::bit_cast<T>(GetProcAddress(static_cast<HMODULE>(aModule), aName));
#else
		auto result = reinterpret_cast<T>(dlsym(aModule, aName));
#endif
		require(result != nullptr, aName);
		return result;
	}

	void check_allocation(owns_pointer aOwns, void* aPointer, const char* aMessage)
	{
		require(aPointer != nullptr && aOwns(aPointer), aMessage);
		std::memset(aPointer, 0x5a, 73);
	}

	void exercise_boundaries(owns_pointer aOwns, void* aModule)
	{
		auto allocate    = symbol<void* (*)(std::size_t)>(aModule, "bpr_module_allocate");
		auto release     = symbol<void (*)(void*)>(aModule, "bpr_module_release");
		auto allocateNew = symbol<void* (*)(std::size_t)>(aModule, "bpr_module_new");
		auto releaseNew  = symbol<void (*)(void*)>(aModule, "bpr_module_delete");
		auto qtData      = symbol<void* (*)()>(aModule, "bpr_module_qt_data");
		require(aOwns(qtData()), "loaded Qt QByteArray storage is not owned");
		for (std::size_t index = 0; index < 4096; ++index) {
			const auto size       = 73 + (index % 256) * 17;
			auto*      fromModule = allocate(size);
			check_allocation(aOwns, fromModule, "module calloc");
			std::free(fromModule);
			auto* fromExecutable = std::malloc(size);
			check_allocation(aOwns, fromExecutable, "executable malloc");
			release(fromExecutable);
			auto* moduleNew = allocateNew(size);
			check_allocation(aOwns, moduleNew, "module new");
			::operator delete(moduleNew);
			auto* executableNew = ::operator new(size);
			check_allocation(aOwns, executableNew, "executable new");
			releaseNew(executableNew);
		}
		auto* crossThread = allocate(16384);
		check_allocation(aOwns, crossThread, "cross-thread allocation");
		std::thread worker([crossThread] { std::free(crossThread); });
		worker.join();
	}

	void exercise_standard_family(owns_pointer aOwns)
	{
		auto* pointer = std::malloc(73);
		check_allocation(aOwns, pointer, "malloc");
		auto* resized = std::realloc(pointer, 16384);
		if (resized == nullptr) {
			std::free(pointer);
			require(false, "realloc failed");
		}
		pointer = resized;
		check_allocation(aOwns, pointer, "realloc");
		std::free(pointer);
		pointer = std::calloc(73, 2);
		check_allocation(aOwns, pointer, "calloc");
		std::free(pointer);
#ifdef _WIN32
		pointer = _aligned_malloc(512, 256);
#else
		require(posix_memalign(&pointer, 256, 512) == 0, "posix_memalign");
#endif
		check_allocation(aOwns, pointer, "aligned ownership");
		require(reinterpret_cast<std::size_t>(pointer) % 256 == 0, "aligned address");
#ifdef _WIN32
		_aligned_free(pointer);
		auto* text = _strdup("allocator-boundary");
#else
		std::free(pointer);
		auto* text = strdup("allocator-boundary");
#endif
		require(text != nullptr && aOwns(text), "strdup");
		std::free(text);
		auto* array = ::operator new[](73);
		check_allocation(aOwns, array, "array new");
		::operator delete[](array);
		auto* aligned = ::operator new(512, std::align_val_t(256));
		check_allocation(aOwns, aligned, "aligned new");
		::operator delete(aligned, std::align_val_t(256));
		// Volatile prevents folding the overflow request into a compiler warning.
		volatile auto impossibleSize = std::numeric_limits<std::size_t>::max();
		// Observe the actual call. An optimized AppleClang malloc builtin can
		// fold away this deliberately impossible request and its errno read.
		auto* volatile allocate = &std::malloc;
		errno                   = 0;
		require(allocate(impossibleSize) == nullptr && errno == ENOMEM, "malloc failure semantics");
		require(::operator new(impossibleSize, std::nothrow) == nullptr, "nothrow failure semantics");
		bool threwBadAlloc = false;
		try {
			auto* unexpected = ::operator new(impossibleSize);
			::operator delete(unexpected);
		} catch (const std::bad_alloc&) {
			threwBadAlloc = true;
		}
		require(threwBadAlloc, "throwing new failure semantics");
	}

	std::size_t allocator_image_count()
	{
		std::size_t count = 0;
#ifdef _WIN32
		HMODULE modules[1024]{ };
		DWORD   bytes = 0;
		require(K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &bytes) != 0
					&& bytes <= sizeof(modules),
				"enumerate loaded images");
		for (std::size_t index = 0; index < bytes / sizeof(HMODULE); ++index) {
			char name[MAX_PATH]{ };
			require(K32GetModuleBaseNameA(GetCurrentProcess(), modules[index], name, MAX_PATH) != 0, "image name");
			if (_stricmp(name, "mimalloc.dll") == 0 || _stricmp(name, "mimalloc-debug.dll") == 0) {
				++count;
			}
		}
#elif defined(__APPLE__)
		for (std::uint32_t index = 0; index < _dyld_image_count(); ++index) {
			if (std::strstr(_dyld_get_image_name(index), "/libmimalloc") != nullptr) {
				++count;
			}
		}
#else
		dl_iterate_phdr([](dl_phdr_info* aInfo, std::size_t, void* aContext) {
			if (std::strstr(aInfo->dlpi_name, "/libmimalloc") != nullptr) {
				++*static_cast<std::size_t*>(aContext);
			}
			return 0;
		},
						&count);
#endif
		return count;
	}

	std::string environment(const char* aName)
	{
#ifdef _WIN32
		const auto length = GetEnvironmentVariableA(aName, nullptr, 0);
		if (length == 0) {
			return { };
		}
		std::string result(length, '\0');
		require(GetEnvironmentVariableA(aName, result.data(), length) == length - 1, "probe environment");
		result.pop_back();
		return result;
#else
		const auto* value = std::getenv(aName);
		return value == nullptr ? std::string{ } : std::string(value);
#endif
	}

	// Compiled only into tests-enabled product images. Each course enters the
	// actual executable's bindings before main; ordinary startup is checked
	// separately. No allocator hook or diagnostic seam ships in distribution.
	void allocator_probe()
	{
		if (environment("BPR_MIMALLOC_PROBE").empty()) {
			return;
		}
#ifdef _WIN32
		auto* allocator = GetModuleHandleW(L"mimalloc.dll");
		if (allocator == nullptr) {
			allocator = GetModuleHandleW(L"mimalloc-debug.dll");
		}
		require(allocator != nullptr, "mi_is_in_heap_region");
		require(GetModuleHandleW(L"mimalloc-redirect.dll") != nullptr, "redirect DLL not loaded");
#else
		auto* allocator = RTLD_DEFAULT;
#endif
		auto owns    = symbol<owns_pointer>(allocator, "mi_is_in_heap_region");
		auto version = symbol<int (*)()>(allocator, "mi_version");
		require(version() == 30503, "pinned runtime version");
		require(allocator_image_count() == 1, "one allocator image");
		exercise_standard_family(owns);
		const auto modulePath = environment("BPR_MIMALLOC_PROBE_MODULE");
		require(!modulePath.empty(), "missing module path");
#ifdef _WIN32
		auto* module = LoadLibraryA(modulePath.c_str());
		if (module == nullptr) {
			std::fprintf(stderr, "MIMALLOC_PROBE_LOAD_ERROR %lu\n", GetLastError());
		}
#else
		auto* module = dlopen(modulePath.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
		require(module != nullptr, "loaded module");
		exercise_boundaries(owns, module);
#ifdef _WIN32
		require(FreeLibrary(module) != 0, "module shutdown");
#else
		require(dlclose(module) == 0, "module shutdown");
#endif
		std::fprintf(stderr, "MIMALLOC_PROBE_PASS C C++ Qt cross-module cross-thread OOM\n");
		std::exit(0);
	}

	struct probe_startup
	{
		probe_startup() noexcept { allocator_probe(); }
	};
	probe_startup gProbeStartup;
}
