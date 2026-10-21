#ifndef BPR_TESTS_SUPPORT_QUALIFICATION_BUILD_HPP
#define BPR_TESTS_SUPPORT_QUALIFICATION_BUILD_HPP

#include <iostream>

namespace bpr::tests {
	/// [[BPR-1674]] Report the binary's diagnostic mode without changing it.
	inline void report_qualification_build()
	{
		std::cout << "QUALIFICATION_BUILD:expect=always";
#if defined(NDEBUG)
		std::cout << ";NDEBUG=1";
#else
		std::cout << ";NDEBUG=0";
#endif
#if defined(_ITERATOR_DEBUG_LEVEL)
		std::cout << ";ITERATOR_DEBUG_LEVEL=" << _ITERATOR_DEBUG_LEVEL;
#endif
#if defined(_MSC_VER)
		std::cout << ";MSC_VER=" << _MSC_VER;
#endif
#if defined(_DLL)
		std::cout << ";CRT=dynamic";
#endif
#if defined(_DEBUG)
		std::cout << ";CRT_DEBUG=1";
#else
		std::cout << ";CRT_DEBUG=0";
#endif
#if defined(QT_VERSION_STR)
		std::cout << ";QT=" << QT_VERSION_STR;
	#if defined(QT_NO_DEBUG)
		std::cout << ";QT_NO_DEBUG=1";
	#else
		std::cout << ";QT_NO_DEBUG=0";
	#endif
#endif
		std::cout << '\n';
	}
	/// Report completed occurrences; the function and its expectations stay intact.
	template <typename Function>
	void run_qualification_case(const char* aName, Function aFunction)
	{
		std::cout << "QUALIFICATION_CASE_BEGIN:" << aName << '\n';
		aFunction();
		std::cout << "QUALIFICATION_CASE_END:" << aName << '\n';
	}
} // namespace bpr::tests

#endif
