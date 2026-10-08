#ifndef BPR_SRC_ADAPTERS_QT_SRC_DISCUSS_HEADER_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_DISCUSS_HEADER_HPP

#include "conversation_glyphs.hpp"

#include <QColor>
#include <QLabel>
#include <QLayout>
#include <QList>
#include <QString>
#include <QToolButton>
#include <QWidget>

#include <cstdint>
#include <optional>
#include <vector>

namespace bpr::qt {
	/// \brief [[BPR-1337]]. How a Discuss header button is drawn. Beside the
	/// code the filter buttons are the drawer's own icons (`native`); in
	/// Discuss they are chips in the language of Conversations' bar.
	enum class discuss_tool_shape : std::uint8_t {
		native,
		/// A rounded chip: a glyph, its words or both, and a count.
		chip,
		/// The first and last segments of a two-way choice.
		segment_first,
		segment_last,
		/// A glyph alone, ringed only on hover and focus: a pill's ×.
		bare,
	};

	/// \brief [[BPR-1337]]. A tool button that Discuss's header draws itself,
	/// from the palette on screen, so both themes follow without restyling.
	class discuss_tool final : public QToolButton
	{
	public:
		explicit discuss_tool(QWidget* aParent = nullptr);

		void set_shape(discuss_tool_shape aShape);
		void set_glyph(std::optional<conversation_glyph> aGlyph);
		/// \brief A number drawn after the words, in the colour of what is new
		/// when \p aNew.
		void set_count(const QString& aCount, bool aNew);
		/// \brief The colour of what is new: the notes' accent.
		void set_new_ink(const QColor& aInk);

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void focusInEvent(QFocusEvent* aEvent) override;

	private:
		/// \brief Whether focus came from the keyboard, so the ring shows only
		/// then: a click leaves no ring that reads as a choice.
		[[nodiscard]] auto ringed() const -> bool;

		discuss_tool_shape                Shape         = discuss_tool_shape::native;
		bool                              KeyboardFocus = false;
		std::optional<conversation_glyph> Glyph;
		QString                           Count;
		bool                              CountIsNew = false;
		QColor                            NewInk;
	};

	/// \brief [[BPR-1337]]. One part of the counts line: a number and its word.
	struct discuss_count_part
	{
		QString Number;
		QString Words;
	};

	/// \brief [[BPR-1337]]. The counts line: numbers in semibold ink, their
	/// words in the quieter ink, spaced rather than punctuated. Its text is
	/// the whole sentence, for accessibility and for the tests; what it
	/// draws are the parts.
	class discuss_counts final : public QLabel
	{
	public:
		explicit discuss_counts(QWidget* aParent = nullptr);

		void set_parts(std::vector<discuss_count_part> aParts);

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		std::vector<discuss_count_part> Parts;
	};

	/// \brief [[BPR-1337]]. A rounded band behind an active drill-down: the
	/// count's mark, its caption and its ×.
	class discuss_pill final : public QWidget
	{
	public:
		explicit discuss_pill(QWidget* aParent = nullptr);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};

	/// \brief [[BPR-1337]]. The header's band: the notes' panel colour with a
	/// hairline under it, as Conversations' bar sits over its list.
	class discuss_band final : public QWidget
	{
	public:
		explicit discuss_band(QWidget* aParent = nullptr);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};

	/// \brief [[BPR-1337]]. Lays its items left to right and wraps them onto
	/// further rows when the width runs out. A trailing item sits at the
	/// right end of the last row when it fits there, and starts a row of its
	/// own when it does not. Each row's items are centred on the row.
	class discuss_flow_layout final : public QLayout
	{
	public:
		explicit discuss_flow_layout(QWidget* aParent = nullptr);
		~discuss_flow_layout() override;

		discuss_flow_layout(const discuss_flow_layout&)                    = delete;
		auto operator=(const discuss_flow_layout&) -> discuss_flow_layout& = delete;
		discuss_flow_layout(discuss_flow_layout&&)                         = delete;
		auto operator=(discuss_flow_layout&&) -> discuss_flow_layout&      = delete;

		/// \brief Adds \p aWidget as the trailing item.
		void add_trailing(QWidget* aWidget);

		void               addItem(QLayoutItem* aItem) override;
		[[nodiscard]] auto count() const -> int override;
		[[nodiscard]] auto itemAt(int aIndex) const -> QLayoutItem* override;
		[[nodiscard]] auto takeAt(int aIndex) -> QLayoutItem* override;
		[[nodiscard]] auto expandingDirections() const -> Qt::Orientations override;
		[[nodiscard]] auto hasHeightForWidth() const -> bool override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSize() const -> QSize override;
		void               setGeometry(const QRect& aRect) override;

	private:
		/// \brief Places the items when \p aApply, and returns the height they take.
		auto arrange(const QRect& aRect, bool aApply) const -> int;

		QList<QLayoutItem*> Items;
		QLayoutItem*        Trailing = nullptr;
	};
} // namespace bpr::qt

#endif
