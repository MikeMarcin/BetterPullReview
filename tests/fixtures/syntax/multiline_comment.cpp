#include <string>

/*
 * BPR-COMMENT-OPEN starts a block comment that spans several lines.
 * BPR-COMMENT-BODY carries no comment marker of its own, so a pass that
 * BPR-COMMENT-TAIL began here would classify it as ordinary code.
 */
namespace bpr::fixture {
	constexpr int Answer = 42;

	inline auto banner() -> std::string
	{
		return std::string{ R"(
BPR-STRING-BODY sits inside a raw string literal opened on an earlier line.
)" };
	}
}
