#pragma once // BPR-PRAGMA-LINE

#include "fixture/detail/config.hpp" // BPR-INCLUDE-QUOTED
#include <string> // BPR-INCLUDE-ANGLE

#ifndef BPR_FIXTURE_GUARD // BPR-IFNDEF-LINE
	#define BPR_FIXTURE_GUARD 1 // BPR-DEFINE-GUARD
#endif

#if BPR_FIXTURE_USE_MODULES() && !defined(BPR_FIXTURE_LEGACY) // BPR-IF-LINE
	#error "BPR-ERROR-LINE MODULES are UNSUPPORTED"
#endif

#if defined(__GNUC__) && !defined(__clang__) && __cplusplus >= 202002L // BPR-STDMACRO-LINE
#endif

#ifdef mixedCase // BPR-LOWER-IFDEF
#endif
#ifndef mixedCase // BPR-LOWER-IFNDEF
#elif mixedCase // BPR-LOWER-ELIF
#endif
#undef mixedCase // BPR-LOWER-UNDEF
#if defined(lowercase) && true && !false && __has_include(<vector>) && __has_cpp_attribute(nodiscard) && __has_builtin(__builtin_expect) && other_name && defined bare_name and 'x' != 0 && L'y' != 0 && __has_private // BPR-CONDITION-OPERATORS
#endif
#if __has_cpp_attribute /* note */ (nodiscard) && after_test // BPR-COMMENT-OPERAND
#endif
// clang-format off
#if defined(__GNUC__) && \
    defined(__clang__) && __cplusplus // BPR-CONTINUED-CONDITION
#endif
#if __has_cpp_attribute /* ) */ \
    (nodiscard) && after_test // BPR-CONTINUED-TEST-OPEN
#endif
#if __has_builtin( \
    __builtin_expect) && after_test // BPR-CONTINUED-TEST-OPERAND
#endif
#if __has_cpp_attribute /* note \
    ) */ (nodiscard) && after_test // BPR-CONTINUED-TEST-COMMENT
#endif
#        // BPR-NULL-DIRECTIVE-COMMENT TARGET_OS_IOS is only available in newer SDKs,
#define // BPR-MALFORMED-DEFINE
#bogus BPR_FIXTURE_GUARD // BPR-UNKNOWN-DIRECTIVE
// clang-format on
#define replacement ordinary_name // BPR-REPLACEMENT

#define BEGIN_TEST_METHOD_PROPERTIES() /* BPR-MACRO-DEFINE ScreenBufferTests() */

// BPR-COMMENT-LINE ProcessString(TextAttribute) VERIFY_ARE_EQUAL Log::Comment
namespace fixture {
	class ScreenBufferTests // BPR-CLASS-LINE
	{
	public:
		TextAttribute& attribute(); // BPR-DECLARATION-LINE
	};

	void ProcessString(const std::wstring& text) // BPR-FREE-LINE
	{
		Log::Comment(L"BPR-STRING-LINE ProcessString(TextAttribute) MACRO_NAME");
		VERIFY_ARE_EQUAL(text.size(), ServiceLocator::instance().count()); // BPR-MACRO-LINE
	}
}
