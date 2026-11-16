/// \file
/// \brief [[BPR-2197]]. The shared fold the tray tabs, Overview's drawer tabs
/// and the mode switcher take their faces from.
///
/// \details The tray tabs' rule before the fold was shared is kept here as
/// the reference: over every room and a spread of tab widths, the shared
/// helper chooses the face it chose, shows the same text and glyphs, and
/// gives a glyph-only tab the same tooltip.

#include "bpr/qt/fold_faces.hpp"

#include <QString>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// The tray's three tabs with a width for each part of a face.
	struct tab_widths
	{
		std::array<int, 3> Name{ };
		std::array<int, 3> Glyph{ };
		std::array<int, 3> GlyphNamed{ };
	};

	/// \brief `fit_tray_tabs` before [[BPR-2197]], verbatim in its rule: the
	/// faces widest first, the first whose tabs fit the room, glyphs when
	/// none does, every name while there is no room yet.
	enum class old_face : std::uint8_t { names,
										 open_named,
										 glyphs };

	[[nodiscard]] auto old_text(const old_face aFace, const int aIndex, const int aCurrent, const QString& aName) -> QString
	{
		return aFace == old_face::names || (aFace == old_face::open_named && aIndex == aCurrent) ? aName : QString{ };
	}

	[[nodiscard]] auto face_width(const tab_widths& aTabs, const QString& aText, const bool aGlyph, const std::size_t aIndex) -> int
	{
		if (!aGlyph) {
			return aTabs.Name[aIndex];
		}
		return aText.isEmpty() ? aTabs.Glyph[aIndex] : aTabs.GlyphNamed[aIndex];
	}

	[[nodiscard]] auto old_choice(const tab_widths& aTabs, const std::array<QString, 3>& aNames, const int aCurrent, const int aRoom) -> old_face
	{
		auto chosen = old_face::names;
		if (aRoom > 0) {
			for (const auto candidate : { old_face::names, old_face::open_named, old_face::glyphs }) {
				chosen     = candidate;
				auto width = 0;
				for (std::size_t index = 0U; index < 3U; ++index) {
					width += face_width(aTabs, old_text(candidate, static_cast<int>(index), aCurrent, aNames[index]), candidate != old_face::names, index);
				}
				if (width <= aRoom) {
					break;
				}
			}
		}
		return chosen;
	}

	[[nodiscard]] auto new_choice(const tab_widths& aTabs, const std::array<QString, 3>& aNames, const int aCurrent, const int aRoom) -> qt::fold_face
	{
		if (aRoom <= 0) {
			return qt::fold_face::names;
		}
		return qt::choose_fold_face({ qt::fold_face::names, qt::fold_face::open_named, qt::fold_face::glyphs }, [&](const qt::fold_face aFace) {
			auto width = 0;
			for (std::size_t index = 0U; index < 3U; ++index) {
				width += face_width(aTabs, qt::fold_text(aFace, static_cast<int>(index) == aCurrent, aNames[index]), qt::fold_shows_glyph(aFace), index);
			}
			return width <= aRoom;
		});
	}

	void test_the_tray_tabs_fold_as_they_did()
	{
		const auto names  = std::array{ QStringLiteral("Comments"), QStringLiteral("Tour"), QStringLiteral("Agent") };
		const auto spread = std::vector<tab_widths>{
			{ .Name = { 92, 52, 60 }, .Glyph = { 36, 36, 36 }, .GlyphNamed = { 112, 72, 80 } },
			{ .Name = { 120, 70, 80 }, .Glyph = { 30, 30, 30 }, .GlyphNamed = { 146, 96, 106 } },
			{ .Name = { 40, 40, 40 }, .Glyph = { 40, 40, 40 }, .GlyphNamed = { 60, 60, 60 } },
		};
		auto compared = 0;
		auto seen     = std::array<bool, 3>{ };
		for (const auto& tabs : spread) {
			for (auto current = 0; current < 3; ++current) {
				for (auto room = -10; room <= 400; ++room) {
					const auto was = old_choice(tabs, names, current, room);
					const auto is  = new_choice(tabs, names, current, room);
					const auto map = std::array{ qt::fold_face::names, qt::fold_face::open_named, qt::fold_face::glyphs };
					expect(map[static_cast<std::size_t>(was)] == is, "the shared fold chooses the face the tray tabs chose, at every room");
					seen[static_cast<std::size_t>(was)] = true;
					for (std::size_t index = 0U; index < 3U; ++index) {
						const auto open = static_cast<int>(index) == current;
						expect(old_text(was, static_cast<int>(index), current, names[index]) == qt::fold_text(is, open, names[index])
								   && (was != old_face::names) == qt::fold_shows_glyph(is),
							   "each tab shows the text and glyph it showed");
					}
					++compared;
				}
			}
		}
		expect(compared > 3'000 && seen[0] && seen[1] && seen[2], "the comparison reaches all three of the tabs' faces");
		expect(qt::fold_tooltip(QString{ }, QStringLiteral("Tour"), QStringLiteral("Your review tour")) == QStringLiteral("Tour\nYour review tour")
				   && qt::fold_tooltip(QStringLiteral("Tour"), QStringLiteral("Tour"), QStringLiteral("Your review tour")) == QStringLiteral("Your review tour"),
			   "a tab showing only its glyph says its name first in its tooltip, as before");
	}

	void test_the_switchers_faces()
	{
		const auto review = QStringLiteral("Review");
		expect(qt::fold_text(qt::fold_face::names_keyed, false, review, QStringLiteral("Ctrl+2")) == QStringLiteral("Review  Ctrl+2")
				   && qt::fold_text(qt::fold_face::names_keyed, false, review, QStringLiteral("⌘2")) == QStringLiteral("Review  ⌘2")
				   && qt::fold_text(qt::fold_face::names_keyed, false, review) == review,
			   "the widest face writes the key the platform gives two spaces after the name");
		expect(qt::fold_text(qt::fold_face::names, true, review, QStringLiteral("Ctrl+2")) == review && qt::fold_text(qt::fold_face::open_named, true, review) == review
				   && qt::fold_text(qt::fold_face::open_named, false, review).isEmpty() && qt::fold_text(qt::fold_face::glyphs, true, review).isEmpty(),
			   "names, then the open mode named, then glyphs alone");
		expect(!qt::fold_shows_glyph(qt::fold_face::names_keyed) && !qt::fold_shows_glyph(qt::fold_face::names) && qt::fold_shows_glyph(qt::fold_face::open_named)
				   && qt::fold_shows_glyph(qt::fold_face::glyphs),
			   "no glyph shows while every name is showing");
		auto asked  = std::vector<qt::fold_face>{ };
		auto chosen = qt::choose_fold_face({ qt::fold_face::names_keyed, qt::fold_face::names, qt::fold_face::open_named, qt::fold_face::glyphs }, [&asked](const qt::fold_face aFace) {
			asked.push_back(aFace);
			return aFace == qt::fold_face::names;
		});
		expect(chosen == qt::fold_face::names && asked == std::vector{ qt::fold_face::names_keyed, qt::fold_face::names }, "the fold asks widest first and stops at the first that fits");
		asked.clear();
		chosen = qt::choose_fold_face({ qt::fold_face::names_keyed, qt::fold_face::names, qt::fold_face::open_named, qt::fold_face::glyphs }, [&asked](const qt::fold_face aFace) {
			asked.push_back(aFace);
			return false;
		});
		expect(chosen == qt::fold_face::glyphs && asked.size() == 3U, "when nothing wider fits, the last face shows without being asked");
	}
} // namespace

auto main() -> int
{
	test_the_tray_tabs_fold_as_they_did();
	test_the_switchers_faces();
	std::cout << "fold-faces-tests:pass\n";
	return EXIT_SUCCESS;
}
