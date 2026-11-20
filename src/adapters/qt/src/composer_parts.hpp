#ifndef BPR_SRC_ADAPTERS_QT_SRC_COMPOSER_PARTS_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_COMPOSER_PARTS_HPP

#include <QString>
#include <QWidget>

class QBoxLayout;
class QHBoxLayout;
class QPushButton;

/// \file
/// \brief [[BPR-2337]] (owner, attended review b64bbc23: "The send button on
/// both the side chats (report and tour) should be positioned more like the
/// agent tab"). The parts of the Agent tab's composer every composer that
/// talks to an agent shares: the field's insets, the thin rule under the
/// words, the row under it with the tools on the left and Send at its right
/// end, and Send itself, an arrow in the agent's accent. The Agent tab, the
/// Tour tab's side chat and the side chat about one review item build their
/// composers from these, so Send stands in the same place in each.
namespace bpr::qt::composer {
	/// \brief The field's insets: left, top, right, bottom.
	inline constexpr int field_left   = 8;
	inline constexpr int field_top    = 6;
	inline constexpr int field_right  = 6;
	inline constexpr int field_bottom = 6;

	/// \brief Gives `aColumn`, the field's own layout, the field's insets.
	void set_field_margins(QBoxLayout& aColumn);

	/// \brief A thin rule inside the field, in its outline's colour.
	class rule_line final : public QWidget
	{
	public:
		explicit rule_line(QWidget* aParent);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};

	/// \brief Adds the rule and, under it, the send row to `aColumn` in
	/// `aField`; returns the row. The caller adds its tools, a stretch, and
	/// Send last, so Send ends the row at the field's right inset.
	[[nodiscard]] auto add_send_row(QWidget& aField, QBoxLayout& aColumn) -> QHBoxLayout*;

	/// \brief Send: the default button, the send arrow and no words, named
	/// "Send". The caller wears the agent accent on it as it restyles.
	[[nodiscard]] auto make_send_button(QWidget& aField, const QString& aObjectName) -> QPushButton*;
} // namespace bpr::qt::composer

#endif
