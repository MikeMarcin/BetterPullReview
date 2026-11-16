// Scratch harness stubs. The title row never reaches these: they belong to
// the review bar's progress track and Updates disclosure, which live in the
// same source file as the title row's strip and are not drawn here.
#include "bpr/qt/epoch_line.hpp"
#include "bpr/qt/progress_mark.hpp"

#include <cstdlib>

namespace bpr::qt {
	auto monochrome_progress_mark_colours(const QColor&) -> progress_mark_colours
	{
		std::abort();
	}

	void paint_progress_mark(QPainter&, const QRectF&, std::optional<core::file_progress_mark>, const progress_mark_colours&)
	{
		std::abort();
	}

	void epoch_line::paint_history_mark(QPainter&, const QRect&, const QPalette&)
	{
		std::abort();
	}
} // namespace bpr::qt
