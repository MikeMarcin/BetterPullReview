#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DRAWER_PARTS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DRAWER_PARTS_HPP

#include <QColor>
#include <QFrame>
#include <QIcon>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class QAction;
class QEnterEvent;
class QGridLayout;
class QLabel;
class QPainter;
class QPaintEvent;
class QResizeEvent;
class QTabBar;

/// \file
/// \brief [[BPR-1587]]. RFC-BPR-0085's shared drawer parts.
///
/// \details The Comments drawer was accepted ([[BPR-1290]] v005) for a small,
/// consistent vocabulary: its header is its tabs, its tools are icons at the
/// tab row's right, every thread is one card anatomy, and each colour means
/// one thing. These are those parts, taken out of the Comments drawer so the
/// Overview's Status, Notes, the Tour tab, the Agent tab and Compose review
/// draw from them rather than from stock widgets. Everything is painted from
/// palette roles and the two colours the composition supplies; nothing here
/// uses a style sheet.
namespace bpr::qt {
	/// \brief A count in an accent pill, painted from the palette: the
	/// Comments tab's count of threads with new comments ([[BPR-1134]]).
	class drawer_count_pill final : public QWidget
	{
	public:
		explicit drawer_count_pill(QWidget* aParent = nullptr);

		/// \brief Shows the count; zero hides the pill.
		void               set_count(std::size_t aCount);
		[[nodiscard]] auto text() const -> const QString&;
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		QString Text;
	};

	/// \brief A drawer's header: its tabs at the left and the selected tab's
	/// tools at the right, or on a second row when the drawer is too narrow
	/// for both, so no tab scrolls out of reach ([[BPR-1290]]).
	class drawer_header final : public QWidget
	{
	public:
		explicit drawer_header(QWidget* aParent = nullptr);

		/// \brief Places the tab row and the tools; both are reparented here.
		void set_parts(QWidget* aTabs, QWidget* aTools);
		/// \brief Whether the tools sit on their own row (the drawer is narrow).
		[[nodiscard]] auto wrapped() const noexcept -> bool;

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;
		auto event(QEvent* aEvent) -> bool override;

	private:
		void arrange();

		QGridLayout* Grid    = nullptr;
		QWidget*     Tabs    = nullptr;
		QWidget*     Tools   = nullptr;
		bool         Wrapped = false;
	};

	/// \brief Gives a tab bar the accepted drawer-tab look: document mode,
	/// content-sized tabs on a drawn base, one keyboard stop ([[BPR-1134]],
	/// [[BPR-1509]]).
	void configure_drawer_tabs(QTabBar& aTabs);

	/// \brief One icon tool for a drawer's tab row: borderless, its name as
	/// tooltip and accessible name, focusable by keyboard.
	[[nodiscard]] auto make_drawer_tool(QWidget* aParent, const QIcon& aIcon, const QString& aName, const QString& aObjectName)
		-> QToolButton*;

	/// \brief The glyphs drawer tools use that the strip does not have.
	enum class drawer_glyph : std::uint8_t {
		/// A route between two stops: Prepare review tour.
		tour,
		/// An edge with a chevron leaving it: hide this drawer.
		hide_right,
		/// A page with lines: an empty notes or status state.
		page,
	};
	[[nodiscard]] auto drawer_icon(drawer_glyph aGlyph, const QPalette& aPalette) -> QIcon;

	/// \brief What a card shows about itself (RFC-BPR-0085 principle 2).
	enum class drawer_card_state : std::uint8_t {
		/// A flat note in the drawer's colour; the hover frame is drawn only
		/// while hovered.
		closed,
		/// Open where you read: the panel, a shadow and the amber edge.
		reading,
		/// Open where you write: the same in the writing blue.
		writing,
	};

	/// \brief Who owns a card (RFC-BPR-0085 principle 3).
	enum class drawer_ownership : std::uint8_t {
		/// Public: solid.
		published,
		/// Yours, not yet published: a dashed edge.
		unpublished,
		/// Private and never published: a dashed edge; the card draws the lock.
		private_note,
		/// An agent's, until a person promotes it: a dashed edge.
		agent,
	};

	/// \brief The two colours of meaning the composition supplies: the amber
	/// of where you read and the blue of what is new and where you write.
	struct drawer_card_colours
	{
		QColor Reading = QColor{ 0xD2, 0x99, 0x22 };
		QColor Writing;
	};

	/// \brief Paints a card's frame into `aBox` (RFC-BPR-0085 principle 2).
	///
	/// \details A closed card paints nothing but its ownership edge (dashed for
	/// anything not published) and, when hovered, the panel and a hairline. An
	/// open card is the palette's Base with a soft shadow, a hairline in the
	/// state's colour, a three-pixel edge at its left and, when `aNotchY` is
	/// given, a notch pointing left at that height.
	void paint_drawer_card(QPainter& aPainter, const QRectF& aBox, const QPalette& aPalette, drawer_card_state aState,
						   drawer_ownership aOwnership, const drawer_card_colours& aColours, bool aHovered,
						   std::optional<qreal> aNotchY = std::nullopt);

	/// \brief A frame that paints itself with `paint_drawer_card`; its layout
	/// holds the card's content.
	class drawer_card final : public QFrame
	{
	public:
		explicit drawer_card(QWidget* aParent = nullptr);

		void               set_state(drawer_card_state aState);
		[[nodiscard]] auto state() const noexcept -> drawer_card_state;
		void               set_ownership(drawer_ownership aOwnership);
		[[nodiscard]] auto ownership() const noexcept -> drawer_ownership;
		void               set_colours(const drawer_card_colours& aColours);
		/// \brief The notch's height inside the card, or none.
		void set_notch(std::optional<int> aY);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void enterEvent(QEnterEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;

	private:
		drawer_card_state   State     = drawer_card_state::closed;
		drawer_ownership    Ownership = drawer_ownership::published;
		drawer_card_colours Colours;
		std::optional<int>  Notch;
		bool                Hovered = false;
	};

	/// \brief An empty tab: a glyph, one sentence and at most one action
	/// (RFC-BPR-0085 principle 6).
	class drawer_empty_state final : public QWidget
	{
	public:
		drawer_empty_state(const QIcon& aGlyph, const QString& aSentence, QAction* aAction, QWidget* aParent = nullptr);

		[[nodiscard]] auto sentence() const -> QString;
		[[nodiscard]] auto action_button() const noexcept -> QToolButton*;

	private:
		QLabel*      Sentence = nullptr;
		QToolButton* Action   = nullptr;
	};

	/// \brief [[BPR-1589]]. A section of a drawer's tab that opens in place to
	/// its details (RFC-BPR-0085's progressive disclosure).
	///
	/// \details The section's title in the drawer's ink at semibold weight, a
	/// faint count before a chevron at the right edge, and no fill until the
	/// pointer is over it: a quiet header, not a grey bar (principle 5). It is
	/// one keyboard stop; Space or Return opens and closes it, the chevron and
	/// `arrowType()` say which, and focus draws the window's focus ring.
	class drawer_section_header final : public QToolButton
	{
	public:
		explicit drawer_section_header(QWidget* aParent = nullptr);

		/// \brief The faint count at the right; empty draws none.
		void               set_count(const QString& aCount);
		[[nodiscard]] auto count() const -> const QString&;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		void describe();

		QString Count;
	};

	/// \brief [[BPR-1589]]. A thin bar of proportions: files read of all, or
	/// checks passed, failed and skipped, each segment in a colour of meaning
	/// over a neutral track.
	class drawer_meter final : public QWidget
	{
	public:
		explicit drawer_meter(QWidget* aParent = nullptr);

		/// \brief The segments, left to right, as weights and colours; the
		/// track shows where the weights fall short of `aTotal`.
		void               set_segments(std::vector<std::pair<double, QColor>> aSegments, double aTotal);
		[[nodiscard]] auto segments() const -> const std::vector<std::pair<double, QColor>>&;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		std::vector<std::pair<double, QColor>> Segments;
		double                                 Total = 0.0;
	};

	/// \brief Every place a widget tree breaks RFC-BPR-0085's principles that a
	/// widget tree can show, one sentence each; empty when none.
	///
	/// \details It checks what the eye would: a tab bar or tab widget inside
	/// another's page (principle 1); three or more push or tool buttons stacked
	/// in one vertical layout (principle 5); a visible label saying only "0",
	/// "—" or "not available" (principle 6); and any style sheet, the one way a
	/// colour outside the palette roles gets in (principle 4). A course runs it
	/// over each rebuilt surface, with a negative control for each rule.
	/// \brief Which rules `drawer_principle_violations` checks.
	struct drawer_principles
	{
		bool NestedTabs     = true;
		bool StyleSheets    = true;
		bool StackedButtons = true;
		bool EmptyValueRows = true;
	};
	[[nodiscard]] auto drawer_principle_violations(const QWidget& aRoot, const drawer_principles& aRules = { }) -> QStringList;
} // namespace bpr::qt

#endif
