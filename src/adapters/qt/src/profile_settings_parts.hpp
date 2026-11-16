#ifndef BPR_SRC_ADAPTERS_QT_SRC_PROFILE_SETTINGS_PARTS_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_PROFILE_SETTINGS_PARTS_HPP

#include "bpr/core/appearance_model.hpp"

#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QList>
#include <QStyledItemDelegate>

#include <cstdint>

class QGridLayout;
class QPainter;

/// \file
/// \brief [[BPR-2204]]. The parts Import and Export Profile Settings share,
/// drawn after design workflow-import-v1, option B (the owner's choice): the
/// Prompt library's ticked list, small tinted tags, the agent sparkle and the
/// refused mark, a rounded well with one border, and the comparison table.
/// Every colour comes from the inherited palette or from a theme role; none
/// is fixed here.
namespace bpr::qt::detail {
	/// \brief What a tag says, which chooses its tint.
	enum class tag_tone : std::uint8_t {
		/// Unchanged, Required, Built-in: the secondary text on a faint wash.
		quiet,
		/// New: the unread blue (`state_unread`).
		fresh,
		/// Updated, New action, Edited: the verified teal (`state_verified`).
		updated,
		/// Has edits not saved, an added effect: the attention amber (`state_attention`).
		caution,
	};

	/// \brief One theme role's colour for the theme `aPalette` was derived
	/// from: the built-in theme whose window is `aPalette`'s, else the light
	/// or dark one by the window's lightness.
	[[nodiscard]] auto role_colour(const QPalette& aPalette, core::appearance_role aRole) -> QColor;
	/// \brief A tone's words, readable on `aGround` and on its own wash there.
	[[nodiscard]] auto tag_ink(const QPalette& aPalette, tag_tone aTone, const QColor& aGround) -> QColor;
	/// \brief A tone's wash: its colour, faint, over whatever ground it sits on.
	[[nodiscard]] auto tag_wash(const QPalette& aPalette, tag_tone aTone) -> QColor;
	/// \brief The refused mark's red (`state_defect`), visible on `aGround`.
	[[nodiscard]] auto refused_ink(const QPalette& aPalette, const QColor& aGround) -> QColor;
	/// \brief The secondary text's face: the Settings secondary scale of `aFont`.
	[[nodiscard]] auto secondary_font(const QFont& aFont) -> QFont;
	/// \brief The quiet grey a selected row is filled with.
	[[nodiscard]] auto quiet_selection(const QPalette& aPalette) -> QColor;
	/// \brief The hairline between a table's rows and columns.
	[[nodiscard]] auto hairline(const QPalette& aPalette) -> QColor;
	/// \brief A well's one border.
	[[nodiscard]] auto well_edge(const QPalette& aPalette) -> QColor;

	/// \brief A tag's size for `aText` in `aFont` (the body face; the tag sets it smaller).
	[[nodiscard]] auto tag_size(const QFont& aFont, const QString& aText) -> QSizeF;
	/// \brief Paints a tag at `aTopLeft` on `aGround`; answers its width.
	auto paint_tag(QPainter& aPainter, const QPointF& aTopLeft, const QString& aText, tag_tone aTone, const QPalette& aPalette, const QFont& aFont, const QColor& aGround) -> qreal;
	/// \brief The agent sparkle, a four-pointed star with a small one beside
	/// it, `aSide` square at `aTopLeft`, in `aInk`.
	void paint_sparkle(QPainter& aPainter, const QPointF& aTopLeft, qreal aSide, const QColor& aInk);
	/// \brief The refused mark, a slashed ring, filling `aBox`, in `aInk`.
	void paint_refused(QPainter& aPainter, const QRectF& aBox, const QColor& aInk);

	/// \brief A tag as a widget: its words are the label's text, so they are
	/// its accessible name; the tint is never the only sign.
	class tag_label final : public QLabel
	{
	public:
		tag_label(const QString& aText, tag_tone aTone, QWidget* aParent);
		[[nodiscard]] auto tone() const noexcept -> tag_tone { return Tone; }
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		tag_tone Tone;
	};

	/// \brief Holds a wrapping label's least height to what its text takes at
	/// the width it was given, so a grid or a nested row neither crops it nor
	/// leaves the room of its narrowest wrapping under it.
	void follow_wrapped_height(QLabel& aLabel);

	/// \brief A wrapping label that asks for its whole text on one line, so
	/// a tag after it sits beside the name until the name must wrap.
	class line_label final : public QLabel
	{
	public:
		line_label(const QString& aText, QWidget* aParent);
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		/// The height its text took at the width it was last given.
		int Lines = 0;
	};

	/// \brief The agent sparkle as a widget, one line of `aFont` high.
	class sparkle_mark final : public QWidget
	{
	public:
		explicit sparkle_mark(QWidget* aParent);
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};

	/// \brief A rounded well on the panel colour (`AlternateBase`, the Prompts
	/// page's panel) with one hairline border: the list, the file's text.
	/// Its child views draw on it with no frame of their own.
	class rounded_well : public QFrame
	{
	public:
		explicit rounded_well(QWidget* aParent);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};

	/// \brief A filled group, as Settings' grouped rows are drawn: a faint
	/// wash of the text over the ground, rounded, with a fainter edge.
	class filled_group final : public QFrame
	{
	public:
		explicit filled_group(QWidget* aParent);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};

	/// \brief Yours beside the file's: a well with a header row, a hairline
	/// between rows and one between the two columns. A cell marked
	/// `set_tinted` is washed whole in the updated tone.
	class comparison_table final : public QWidget
	{
	public:
		explicit comparison_table(QWidget* aParent);
		/// \brief The grid the cells go in: row 0 is the header.
		[[nodiscard]] auto grid() const noexcept -> QGridLayout* { return Grid; }
		/// \brief Washes `aCell` whole in the updated tone.
		static void set_tinted(QWidget& aCell);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		QGridLayout* Grid = nullptr;
	};

	/// The data roles `settings_row_delegate` reads, beside the display text.
	/// A section heading with no tick (not selectable).
	inline constexpr int row_heading_role = Qt::UserRole + 20;
	/// The name, when the display text says more for a screen reader.
	inline constexpr int row_name_role = Qt::UserRole + 21;
	/// The quieter second line.
	inline constexpr int row_second_role = Qt::UserRole + 22;
	/// The tag's words, and its `tag_tone`.
	inline constexpr int row_tag_role  = Qt::UserRole + 23;
	inline constexpr int row_tone_role = Qt::UserRole + 24;
	/// A `row_mark` before the name.
	inline constexpr int row_mark_role = Qt::UserRole + 25;
	/// The name set semibold: a section's own row.
	inline constexpr int row_strong_role = Qt::UserRole + 26;

	/// \brief What a row shows before its name.
	enum class row_mark : std::uint8_t {
		none,
		/// The agent sparkle, in the agent accent.
		agent,
		/// The refused mark, in the defect red, where the tick would be.
		refused,
	};

	/// \brief A row of the Prompt library's ticked list: a tick (or the
	/// refused mark, or the tick's room left empty so every name lines up),
	/// the sparkle for an agent, the name with its tag, and a quieter second
	/// line. A heading is body text, semibold, with even room above it. A
	/// selected row is a quiet grey; focus keeps the style's ring. Names and
	/// lines wrap; nothing is elided. Colours and fonts come from the row's
	/// own style option.
	class settings_row_delegate final : public QStyledItemDelegate
	{
	public:
		using QStyledItemDelegate::QStyledItemDelegate;
		void               paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override;
		[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override;

	protected:
		auto editorEvent(QEvent* aEvent, QAbstractItemModel* aModel, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> bool override;
	};
} // namespace bpr::qt::detail

#endif // BPR_SRC_ADAPTERS_QT_SRC_PROFILE_SETTINGS_PARTS_HPP
