// [[BPR-0970]]. Markdown, rendered half: the body view's own budgets, fence
// and tab repair and image placeholders around Qt's Markdown reader. Built
// only in a Qt graph. Runs offscreen; the application object is made once.
//
// Input: the whole buffer is one pull request or comment body.

#include "bpr/qt/markdown_body_view.hpp"
#include "fuzz_support.hpp"

#include <QApplication>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextEdit>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>

namespace {
	auto fuzz_application() -> QApplication&
	{
		static int   argc     = 1;
		static char  name[]   = "fuzz_markdown_view";
		static char* argv[]   = { name, nullptr };
		static auto  instance = [] {
			::setenv("QT_QPA_PLATFORM", "offscreen", 1);
			return std::make_unique<QApplication>(argc, argv);
		}();
		return *instance;
	}
}

// libFuzzer calls the entry point by this exact name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, const std::size_t aSize) -> int
{
	using namespace bpr;
	if (aSize > 70000U) {
		return 0;
	}
	static_cast<void>(fuzz_application());
	const std::string      body{ reinterpret_cast<const char*>(aData), aSize };
	qt::markdown_body_view view;
	view.set_body(body, "fuzz");
	// The original bytes are kept exactly, and a body the view does not
	// render says why.
	fuzz::require(view.original_body() == body);
	fuzz::require(view.rendered_available() || !view.fallback_reason().isEmpty());
	// [[BPR-1543]]. The GitHub-parity marks never reach the rendered document
	// unless the body itself holds them.
	const auto marked = [](const QString& aText) {
		return std::ranges::any_of(aText, [](const QChar aChar) { return aChar.unicode() == 0x2008 || (aChar.unicode() >= 0xFDE0 && aChar.unicode() <= 0xFDEF); });
	};
	fuzz::require(marked(QString::fromUtf8(body.data(), static_cast<qsizetype>(body.size()))) || !marked(view.rendered_view()->document()->toRawText()));
	view.show_source(true);
	fuzz::require(view.source_visible());
	static_cast<void>(view.selected_source_bytes());
	view.show_source(false);
	view.clear();
	return 0;
}
