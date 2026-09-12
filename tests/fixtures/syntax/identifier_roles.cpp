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
