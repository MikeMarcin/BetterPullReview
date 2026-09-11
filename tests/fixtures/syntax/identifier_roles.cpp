#include <string>

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
