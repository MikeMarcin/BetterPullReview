#ifndef BPR_SRC_ADAPTERS_QT_SRC_SETTINGS_CARD_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_SETTINGS_CARD_HPP

#include <QLabel>

class QFrame;
class QHBoxLayout;
class QVBoxLayout;

/// \file
/// \brief [[BPR-2302]]. The one Settings section: a card whose header names
/// it and carries its Reset, a hairline, then its rows. Appearance drew it
/// first; Review and Comments take it from here (owner, 2026-11-19: "take the
/// section header style from the appearance menu and port it to those two
/// menus").
namespace bpr::qt::detail {
	/// The card's side inset, and its header's height above and below the title.
	inline constexpr auto settings_card_inset = 16;
	inline constexpr auto settings_row_inset  = 10;
	/// The space between two cards on a page.
	inline constexpr auto settings_card_gap = 12;

	/// \brief A section's title: the header's words, announced as a heading.
	class settings_card_title final : public QLabel
	{
	public:
		settings_card_title(const QString& aText, QWidget* aParent);
	};

	/// \brief One card: a header that names it and carries its Reset, and a body.
	struct settings_card
	{
		QFrame*              Frame  = nullptr;
		QHBoxLayout*         Header = nullptr;
		QVBoxLayout*         Body   = nullptr;
		settings_card_title* Title  = nullptr;
	};

	/// \brief A card named `aTitle`: the frame is `aObjectName` (which ends in
	/// `Card`, so Settings search reads its rows as the section `aTitle`), its
	/// header `aObjectName` + `Header`, its title `aObjectName` + `Title`.
	[[nodiscard]] auto make_settings_card(QWidget* aParent, const QString& aTitle, const QString& aObjectName) -> settings_card;

	/// \brief The hairline under a card's header and between its rows.
	[[nodiscard]] auto make_settings_hairline(QWidget* aParent) -> QFrame*;
} // namespace bpr::qt::detail

#endif
