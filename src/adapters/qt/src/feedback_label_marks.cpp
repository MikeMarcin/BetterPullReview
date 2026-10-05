#include "bpr/qt/feedback_label_marks.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		// The glyphs, exactly as BPR-1080's selected page writes them, one
		// element per path or circle, in the page's order. M, L, C and Z map
		// to moveTo, lineTo, cubicTo and closeSubpath; a disc is a circle.
		// `tests/adapters_qt/feedback_label_marks_tests.cpp` compares every
		// element with the checked-in transcription of that page. Changing a
		// glyph needs a new owner verdict.

		[[nodiscard]] auto stroke(QPainterPath aPath, const qreal aWidth) -> feedback_glyph_element
		{
			return { .Kind = feedback_glyph_element_kind::stroke, .PenWidth = aWidth, .Path = std::move(aPath) };
		}

		[[nodiscard]] auto fill(QPainterPath aPath) -> feedback_glyph_element
		{
			aPath.setFillRule(Qt::WindingFill);
			return { .Kind = feedback_glyph_element_kind::fill, .Path = std::move(aPath) };
		}

		[[nodiscard]] auto disc(const qreal aX, const qreal aY, const qreal aRadius) -> feedback_glyph_element
		{
			auto path = QPainterPath{ };
			path.addEllipse(QPointF{ aX, aY }, aRadius, aRadius);
			return { .Kind = feedback_glyph_element_kind::disc, .Path = std::move(path), .Centre = { aX, aY }, .Radius = aRadius };
		}

		// praise
		[[nodiscard]] auto glyph_praise() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			{
				auto path = QPainterPath{ };
				path.moveTo(12.0, 19.5);
				path.cubicTo(6.2, 15.6, 3.5, 12.6, 3.5, 9.2);
				path.cubicTo(3.5, 6.6, 5.4, 4.8, 7.7, 4.8);
				path.cubicTo(9.6, 4.8, 11.0, 5.9, 12.0, 7.6);
				path.cubicTo(13.0, 5.9, 14.4, 4.8, 16.3, 4.8);
				path.cubicTo(18.6, 4.8, 20.5, 6.6, 20.5, 9.2);
				path.cubicTo(20.5, 12.6, 17.8, 15.6, 12.0, 19.5);
				path.closeSubpath();
				elements.push_back(stroke(std::move(path), 2.6));
			}
			return elements;
		}

		// nitpick
		[[nodiscard]] auto glyph_nitpick() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			{
				auto path = QPainterPath{ };
				path.moveTo(15.5, 9.5);
				path.cubicTo(15.5, 12.81, 12.81, 15.5, 9.5, 15.5);
				path.cubicTo(6.19, 15.5, 3.5, 12.81, 3.5, 9.5);
				path.cubicTo(3.5, 6.19, 6.19, 3.5, 9.5, 3.5);
				path.cubicTo(12.81, 3.5, 15.5, 6.19, 15.5, 9.5);
				path.closeSubpath();
				elements.push_back(stroke(std::move(path), 2.6));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(12.43, 15.62);
				path.lineTo(15.62, 12.43);
				path.lineTo(21.63, 18.44);
				path.cubicTo(23.75, 20.57, 20.57, 23.75, 18.44, 21.63);
				path.closeSubpath();
				elements.push_back(fill(std::move(path)));
			}
			return elements;
		}

		// suggestion
		[[nodiscard]] auto glyph_suggestion() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			{
				auto path = QPainterPath{ };
				path.moveTo(9.2, 15.6);
				path.cubicTo(7.0, 14.3, 5.5, 12.2, 5.5, 9.6);
				path.cubicTo(5.5, 6.0, 8.4, 3.5, 12.0, 3.5);
				path.cubicTo(15.6, 3.5, 18.5, 6.0, 18.5, 9.6);
				path.cubicTo(18.5, 12.2, 17.0, 14.3, 14.8, 15.6);
				path.lineTo(14.8, 17.0);
				path.lineTo(9.2, 17.0);
				path.closeSubpath();
				elements.push_back(stroke(std::move(path), 2.6));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(9.8, 20.3);
				path.lineTo(14.2, 20.3);
				elements.push_back(stroke(std::move(path), 2.6));
			}
			return elements;
		}

		// issue
		[[nodiscard]] auto glyph_issue() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			{
				auto path = QPainterPath{ };
				path.moveTo(12.0, 4.0);
				path.lineTo(12.0, 14.0);
				elements.push_back(stroke(std::move(path), 2.6));
			}
			elements.push_back(disc(12.0, 19.0, 1.75));
			return elements;
		}

		// todo
		[[nodiscard]] auto glyph_todo() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			{
				auto path = QPainterPath{ };
				path.moveTo(15.5, 12.5);
				path.lineTo(15.5, 18.5);
				path.cubicTo(15.5, 19.05, 15.05, 19.5, 14.5, 19.5);
				path.lineTo(5.5, 19.5);
				path.cubicTo(4.95, 19.5, 4.5, 19.05, 4.5, 18.5);
				path.lineTo(4.5, 9.5);
				path.cubicTo(4.5, 8.95, 4.95, 8.5, 5.5, 8.5);
				path.lineTo(11.0, 8.5);
				elements.push_back(stroke(std::move(path), 2.6));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(7.8, 13.0);
				path.lineTo(10.6, 15.8);
				path.lineTo(19.8, 5.0);
				elements.push_back(stroke(std::move(path), 2.6));
			}
			return elements;
		}

		// question
		[[nodiscard]] auto glyph_question() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			{
				auto path = QPainterPath{ };
				path.moveTo(8.4, 8.6);
				path.cubicTo(8.4, 5.9, 10.0, 4.2, 12.2, 4.2);
				path.cubicTo(14.4, 4.2, 15.9, 5.7, 15.9, 7.7);
				path.cubicTo(15.9, 10.6, 12.2, 11.1, 12.2, 14.3);
				elements.push_back(stroke(std::move(path), 2.6));
			}
			elements.push_back(disc(12.2, 19.0, 1.75));
			return elements;
		}

		// thought
		[[nodiscard]] auto glyph_thought() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			{
				auto path = QPainterPath{ };
				path.moveTo(7.2, 14.0);
				path.cubicTo(5.0, 14.0, 3.6, 12.6, 3.6, 11.0);
				path.cubicTo(3.6, 9.4, 5.0, 8.2, 6.8, 8.4);
				path.cubicTo(7.2, 5.8, 9.3, 4.0, 12.0, 4.0);
				path.cubicTo(14.6, 4.0, 16.6, 5.7, 17.0, 8.0);
				path.cubicTo(19.0, 7.9, 20.5, 9.3, 20.5, 11.0);
				path.cubicTo(20.5, 12.7, 19.2, 14.0, 17.3, 14.0);
				path.closeSubpath();
				elements.push_back(stroke(std::move(path), 2.6));
			}
			elements.push_back(disc(7.6, 18.0, 1.8));
			elements.push_back(disc(4.4, 20.9, 1.3));
			return elements;
		}

		// chore
		[[nodiscard]] auto glyph_chore() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			{
				auto path = QPainterPath{ };
				path.moveTo(22.0, 3.0);
				path.lineTo(15.75, 9.25);
				elements.push_back(stroke(std::move(path), 2.2));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(14.0, 7.0);
				path.lineTo(18.0, 11.0);
				path.lineTo(16.5, 13.5);
				path.lineTo(11.5, 8.5);
				path.closeSubpath();
				elements.push_back(fill(std::move(path)));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(16.0, 15.0);
				path.lineTo(10.0, 9.0);
				path.lineTo(1.5, 12.5);
				path.lineTo(4.0, 15.0);
				path.lineTo(8.0, 11.0);
				path.lineTo(9.0, 12.0);
				path.lineTo(5.0, 16.0);
				path.lineTo(9.0, 20.0);
				path.lineTo(13.0, 16.0);
				path.lineTo(14.0, 17.0);
				path.lineTo(10.0, 21.0);
				path.lineTo(12.5, 23.5);
				path.closeSubpath();
				elements.push_back(fill(std::move(path)));
			}
			return elements;
		}

		// note
		[[nodiscard]] auto glyph_note() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			elements.push_back(disc(12.0, 5.6, 1.75));
			{
				auto path = QPainterPath{ };
				path.moveTo(12.0, 10.5);
				path.lineTo(12.0, 19.5);
				elements.push_back(stroke(std::move(path), 2.6));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(9.8, 10.5);
				path.lineTo(12.0, 10.5);
				elements.push_back(stroke(std::move(path), 2.6));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(9.8, 19.5);
				path.lineTo(14.2, 19.5);
				elements.push_back(stroke(std::move(path), 2.6));
			}
			return elements;
		}

		// typo
		[[nodiscard]] auto glyph_typo() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			{
				auto path = QPainterPath{ };
				path.moveTo(0.5, 14.0);
				path.lineTo(4.0, 2.0);
				path.lineTo(6.0, 2.0);
				path.lineTo(9.5, 14.0);
				path.lineTo(7.1, 14.0);
				path.lineTo(6.52, 12.0);
				path.lineTo(3.48, 12.0);
				path.lineTo(2.9, 14.0);
				path.closeSubpath();
				path.moveTo(4.07, 10.0);
				path.lineTo(5.93, 10.0);
				path.lineTo(5.0, 6.8);
				path.closeSubpath();
				elements.push_back(fill(std::move(path)));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(10.0, 2.0);
				path.lineTo(12.0, 2.0);
				path.lineTo(12.0, 6.0);
				path.lineTo(13.5, 6.0);
				path.cubicTo(14.88, 6.0, 16.0, 7.12, 16.0, 8.5);
				path.lineTo(16.0, 11.5);
				path.cubicTo(16.0, 12.88, 14.88, 14.0, 13.5, 14.0);
				path.lineTo(10.0, 14.0);
				path.closeSubpath();
				path.moveTo(12.0, 8.0);
				path.lineTo(12.0, 12.0);
				path.lineTo(13.5, 12.0);
				path.cubicTo(13.78, 12.0, 14.0, 11.78, 14.0, 11.5);
				path.lineTo(14.0, 8.5);
				path.cubicTo(14.0, 8.22, 13.78, 8.0, 13.5, 8.0);
				path.closeSubpath();
				elements.push_back(fill(std::move(path)));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(23.5, 6.0);
				path.lineTo(21.0, 6.0);
				path.cubicTo(19.34, 6.0, 18.0, 7.34, 18.0, 9.0);
				path.lineTo(18.0, 11.0);
				path.cubicTo(18.0, 12.66, 19.34, 14.0, 21.0, 14.0);
				path.lineTo(23.5, 14.0);
				path.lineTo(23.5, 12.0);
				path.lineTo(21.0, 12.0);
				path.cubicTo(20.45, 12.0, 20.0, 11.55, 20.0, 11.0);
				path.lineTo(20.0, 9.0);
				path.cubicTo(20.0, 8.45, 20.45, 8.0, 21.0, 8.0);
				path.lineTo(23.5, 8.0);
				path.closeSubpath();
				elements.push_back(fill(std::move(path)));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(1.5, 20.8);
				path.cubicTo(2.59, 20.8, 3.41, 18.2, 4.5, 18.2);
				path.cubicTo(5.59, 18.2, 6.41, 20.8, 7.5, 20.8);
				path.cubicTo(8.59, 20.8, 9.41, 18.2, 10.5, 18.2);
				path.cubicTo(11.59, 18.2, 12.41, 20.8, 13.5, 20.8);
				path.cubicTo(14.59, 20.8, 15.41, 18.2, 16.5, 18.2);
				path.cubicTo(17.59, 18.2, 18.41, 20.8, 19.5, 20.8);
				path.cubicTo(20.59, 20.8, 21.41, 18.2, 22.5, 18.2);
				elements.push_back(stroke(std::move(path), 2.0));
			}
			return elements;
		}

		// polish
		[[nodiscard]] auto glyph_polish() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			{
				auto path = QPainterPath{ };
				path.moveTo(10.0, 3.5);
				path.cubicTo(10.6, 7.8, 11.7, 8.9, 16.0, 9.5);
				path.cubicTo(11.7, 10.1, 10.6, 11.2, 10.0, 15.5);
				path.cubicTo(9.4, 11.2, 8.3, 10.1, 4.0, 9.5);
				path.cubicTo(8.3, 8.9, 9.4, 7.8, 10.0, 3.5);
				path.closeSubpath();
				elements.push_back(stroke(std::move(path), 2.6));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(17.5, 14.0);
				path.cubicTo(17.8, 16.1, 18.4, 16.7, 20.5, 17.0);
				path.cubicTo(18.4, 17.3, 17.8, 17.9, 17.5, 20.0);
				path.cubicTo(17.2, 17.9, 16.6, 17.3, 14.5, 17.0);
				path.cubicTo(16.6, 16.7, 17.2, 16.1, 17.5, 14.0);
				path.closeSubpath();
				elements.push_back(stroke(std::move(path), 2.6));
			}
			return elements;
		}

		// quibble
		[[nodiscard]] auto glyph_quibble() -> std::vector<feedback_glyph_element>
		{
			auto elements = std::vector<feedback_glyph_element>{ };
			{
				auto path = QPainterPath{ };
				path.moveTo(3.0, 17.2);
				path.cubicTo(3.5, 18.8, 4.4, 19.8, 5.7, 19.8);
				path.cubicTo(8.3, 19.8, 9.4, 13.8, 12.0, 13.8);
				path.cubicTo(14.6, 13.8, 15.7, 19.8, 18.3, 19.8);
				path.cubicTo(19.6, 19.8, 20.5, 18.8, 21.0, 17.2);
				elements.push_back(stroke(std::move(path), 2.6));
			}
			{
				auto path = QPainterPath{ };
				path.moveTo(12.0, 7.8);
				path.lineTo(12.0, 4.4);
				path.moveTo(7.72, 9.04);
				path.lineTo(5.71, 6.81);
				path.moveTo(16.28, 9.04);
				path.lineTo(18.29, 6.81);
				elements.push_back(stroke(std::move(path), 2.6));
			}
			return elements;
		}

		[[nodiscard]] auto build_glyphs() -> std::array<std::vector<feedback_glyph_element>, core::feedback_label_count>
		{
			return { glyph_praise(), glyph_nitpick(), glyph_suggestion(), glyph_issue(), glyph_todo(), glyph_question(),
					 glyph_thought(), glyph_chore(), glyph_note(), glyph_typo(), glyph_polish(), glyph_quibble() };
		}

		[[nodiscard]] auto index_of(const core::feedback_label aLabel) noexcept -> std::size_t
		{
			return static_cast<std::size_t>(aLabel);
		}

		constexpr auto shortcodes = std::array<std::string_view, core::feedback_label_count>{
			"pr",
			"ni",
			"su",
			"is",
			"td",
			"qu",
			"th",
			"ch",
			"no",
			"ty",
			"po",
			"qb",
		};

		[[nodiscard]] auto label_at(const std::size_t aIndex) noexcept -> core::feedback_label
		{
			return static_cast<core::feedback_label>(static_cast<std::uint8_t>(aIndex));
		}

		[[nodiscard]] auto text_of(const std::string_view aText) -> QString
		{
			return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
		}

		[[nodiscard]] auto decoration_text(const core::feedback_decoration_kind aKind) -> QString
		{
			switch (aKind) {
			case core::feedback_decoration_kind::blocking:
				return QStringLiteral("blocking");
			case core::feedback_decoration_kind::non_blocking:
				return QStringLiteral("non-blocking");
			case core::feedback_decoration_kind::if_minor:
				return QStringLiteral("if-minor");
			case core::feedback_decoration_kind::custom:
				return { };
			}
			return { };
		}

		[[nodiscard]] auto ratio(const QColor& aFirst, const QColor& aSecond) -> double
		{
			const auto to_core = [](const QColor& aColour) {
				return core::appearance_color{ .Red = static_cast<std::uint8_t>(aColour.red()), .Green = static_cast<std::uint8_t>(aColour.green()), .Blue = static_cast<std::uint8_t>(aColour.blue()), .Alpha = 255U };
			};
			return contrast_ratio(to_core(aFirst), to_core(aSecond));
		}
	} // namespace

	auto feedback_label_glyph(const core::feedback_label aLabel) -> std::span<const feedback_glyph_element>
	{
		static const auto glyphs = build_glyphs();
		const auto        index  = index_of(aLabel);
		if (index >= glyphs.size()) {
			return { };
		}
		return glyphs[index];
	}

	auto feedback_shortcode(const core::feedback_label aLabel) noexcept -> std::string_view
	{
		const auto index = index_of(aLabel);
		return index < shortcodes.size() ? shortcodes[index] : std::string_view{ };
	}

	auto find_feedback_shortcode(const std::string_view aCode) noexcept -> std::optional<core::feedback_label>
	{
		for (std::size_t index = 0U; index < shortcodes.size(); ++index) {
			if (shortcodes[index] == aCode) {
				return label_at(index);
			}
		}
		return std::nullopt;
	}

	auto feedback_shortcode_matches(const std::string_view aFragment) -> std::vector<core::feedback_label>
	{
		auto matches = std::vector<core::feedback_label>{ };
		if (aFragment.empty()) {
			return matches;
		}
		for (std::size_t index = 0U; index < shortcodes.size(); ++index) {
			if (shortcodes[index].starts_with(aFragment)) {
				matches.push_back(label_at(index));
			}
		}
		for (std::size_t index = 0U; index < shortcodes.size(); ++index) {
			const auto label = label_at(index);
			if (core::stable_name(label).starts_with(aFragment) && std::ranges::count(matches, label) == 0) {
				matches.push_back(label);
			}
		}
		return matches;
	}

	auto feedback_label_role(const core::feedback_label aLabel) noexcept -> core::appearance_role
	{
		return static_cast<core::appearance_role>(static_cast<std::uint8_t>(core::index_of(core::first_feedback_label_role) + index_of(aLabel)));
	}

	auto feedback_label_tints_of(const core::appearance_palette& aPalette) -> feedback_label_tints
	{
		auto tints = feedback_label_tints{ };
		for (std::size_t index = 0U; index < tints.size(); ++index) {
			tints[index] = to_presentation_color(aPalette[core::index_of(feedback_label_role(label_at(index)))]);
		}
		return tints;
	}

	auto feedback_mark_soft_fill(const QColor& aTint, const QColor& aPanel) -> QColor
	{
		// 22% over a light panel and 30% over a dark one (owner, BPR-1079).
		const auto share = aPanel.lightnessF() < 0.5 ? 0.30 : 0.22;
		const auto mix   = [share](const int aOver, const int aUnder) {
			return static_cast<int>(std::lround(aOver * share + aUnder * (1.0 - share)));
		};
		return QColor{ mix(aTint.red(), aPanel.red()), mix(aTint.green(), aPanel.green()), mix(aTint.blue(), aPanel.blue()) };
	}

	auto feedback_mark_blocking_ink(const QColor& aTint) -> QColor
	{
		const auto white = QColor{ 0xFF, 0xFF, 0xFF };
		const auto near  = QColor{ 0x11, 0x14, 0x18 };
		return ratio(white, aTint) >= ratio(near, aTint) ? white : near;
	}

	auto feedback_mark_colours_for(const QColor& aTint, const QPalette& aPalette) -> feedback_mark_colours
	{
		return { .Tint = aTint, .Panel = aPalette.color(QPalette::Base), .Ink = aPalette.color(QPalette::Text) };
	}

	void paint_feedback_glyph(QPainter& aPainter, const QRectF& aBox, const core::feedback_label aLabel, const QColor& aInk)
	{
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		aPainter.translate(aBox.topLeft());
		aPainter.scale(aBox.width() / feedback_glyph_grid, aBox.height() / feedback_glyph_grid);
		for (const auto& element : feedback_label_glyph(aLabel)) {
			if (element.Kind == feedback_glyph_element_kind::stroke) {
				auto pen = QPen{ aInk, element.PenWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin };
				aPainter.setPen(pen);
				aPainter.setBrush(Qt::NoBrush);
				aPainter.drawPath(element.Path);
				continue;
			}
			aPainter.fillPath(element.Path, aInk);
		}
		aPainter.restore();
	}

	void paint_feedback_mark(QPainter& aPainter, const QRectF& aMark, const core::feedback_label aLabel, const bool aBlocking, const feedback_mark_colours& aColours)
	{
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto inset = feedback_mark_border / 2.0;
		const auto frame = aMark.adjusted(inset, inset, -inset, -inset);
		aPainter.setPen(QPen{ aColours.Tint, feedback_mark_border });
		aPainter.setBrush(aBlocking ? aColours.Tint : feedback_mark_soft_fill(aColours.Tint, aColours.Panel));
		aPainter.drawRoundedRect(frame, feedback_mark_corner - inset, feedback_mark_corner - inset);
		const auto scale = aMark.width() / feedback_mark_size;
		const auto side  = feedback_mark_glyph_size * scale;
		const auto box   = QRectF{ aMark.center().x() - side / 2.0, aMark.center().y() - side / 2.0, side, side };
		paint_feedback_glyph(aPainter, box, aLabel, aBlocking ? feedback_mark_blocking_ink(aColours.Tint) : aColours.Ink);
		aPainter.restore();
	}

	auto feedback_mark_pixmap(const core::feedback_label aLabel, const bool aBlocking, const feedback_mark_colours& aColours, const qreal aDevicePixelRatio) -> QPixmap
	{
		const auto ratio  = std::max<qreal>(1.0, aDevicePixelRatio);
		const auto pixels = static_cast<int>(std::ceil(feedback_mark_size * ratio));
		auto       image  = QImage{ pixels, pixels, QImage::Format_ARGB32_Premultiplied };
		image.fill(Qt::transparent);
		image.setDevicePixelRatio(ratio);
		{
			auto painter = QPainter{ &image };
			paint_feedback_mark(painter, QRectF{ 0.0, 0.0, feedback_mark_size, feedback_mark_size }, aLabel, aBlocking, aColours);
		}
		return QPixmap::fromImage(std::move(image));
	}

	auto feedback_plain_mark_pixmap(const QPalette& aPalette, const qreal aDevicePixelRatio) -> QPixmap
	{
		const auto ratio  = std::max<qreal>(1.0, aDevicePixelRatio);
		const auto pixels = static_cast<int>(std::ceil(feedback_mark_size * ratio));
		auto       image  = QImage{ pixels, pixels, QImage::Format_ARGB32_Premultiplied };
		image.fill(Qt::transparent);
		image.setDevicePixelRatio(ratio);
		{
			auto painter = QPainter{ &image };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto inset = feedback_mark_border / 2.0;
			painter.setPen(QPen{ aPalette.color(QPalette::PlaceholderText), feedback_mark_border, Qt::DotLine });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(QRectF{ inset, inset, feedback_mark_size - 2.0 * inset, feedback_mark_size - 2.0 * inset }, feedback_mark_corner - inset, feedback_mark_corner - inset);
		}
		return QPixmap::fromImage(std::move(image));
	}

	void paint_feedback_rail(QPainter& aPainter, const qreal aRight, const qreal aTop, const qreal aBottom, const QColor& aColour, const bool aBlocking)
	{
		const auto width = aBlocking ? 4.0 : 2.0;
		aPainter.save();
		aPainter.setPen(Qt::NoPen);
		aPainter.setBrush(aColour);
		aPainter.drawRoundedRect(QRectF{ aRight - width, aTop, width, std::max(0.0, aBottom - aTop) }, width / 2.0, width / 2.0);
		aPainter.restore();
	}

	auto feedback_mark_name(const core::feedback_annotation& aAnnotation) -> QString
	{
		if (aAnnotation.Status != core::feedback_prefix_status::recognized || !aAnnotation.Label.has_value()) {
			return { };
		}
		auto name        = text_of(core::stable_name(*aAnnotation.Label));
		auto decorations = QStringList{ };
		for (const auto& decoration : aAnnotation.Decorations) {
			// Standard decorations only, in written order; a repeat is named once.
			const auto words = decoration_text(decoration.Kind);
			if (!words.isEmpty() && !decorations.contains(words)) {
				decorations.push_back(words);
			}
		}
		if (!decorations.isEmpty()) {
			name += QStringLiteral(" (%1)").arg(decorations.join(QStringLiteral(", ")));
		}
		return name;
	}

	auto feedback_body_mark_of(const std::string_view aBody) -> feedback_body_mark
	{
		const auto annotation = core::recognize_feedback_prefix(aBody);
		if (annotation.Status != core::feedback_prefix_status::recognized || !annotation.Label.has_value() || !annotation.Subject.has_value()) {
			return { };
		}
		return feedback_body_mark{
			.Label         = annotation.Label,
			.Blocking      = annotation.Expectation == core::feedback_expectation::blocking,
			.Name          = feedback_mark_name(annotation),
			.SubjectOffset = feedback_text_index(aBody, annotation.Subject->Begin),
		};
	}

	auto feedback_post_name(const QString& aAuthor, const feedback_body_mark& aMark) -> QString
	{
		if (aMark.Name.isEmpty()) {
			return aAuthor;
		}
		return QStringLiteral("%1, %2").arg(aAuthor, aMark.Name);
	}

	auto feedback_label_description(const core::feedback_label aLabel) -> QString
	{
		return text_of(core::definition_of(aLabel).Description);
	}

	auto feedback_label_text(const core::feedback_label aLabel) -> QString
	{
		return text_of(core::stable_name(aLabel));
	}

	auto feedback_text_index(const std::string_view aBody, const std::size_t aByteOffset) -> qsizetype
	{
		const auto bytes = std::min(aByteOffset, aBody.size());
		return QString::fromUtf8(aBody.data(), static_cast<qsizetype>(bytes)).size();
	}
} // namespace bpr::qt
