#ifndef BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_CODE_BLOCK_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_CODE_BLOCK_HPP

#include "bpr/qt/code_sample_painter.hpp"

#include <QColor>
#include <QFont>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSizeF>
#include <QString>
#include <QTextLayout>
#include <QTextObjectInterface>
#include <QWidget>

#include <set>

#include <functional>
#include <map>
#include <string>
#include <vector>

class QTextEdit;
class QTimer;
class QToolButton;

/// \file
/// \brief [[BPR-1944]]. A fenced block of an agent's answer as its own box:
/// a header with the language, Wrap and Copy, over the code, which is as
/// wide as the column and scrolls sideways by itself.
/// \details The reader keeps the block's lines in its document, hidden, and
/// holds its place with one object character this unit sizes; the box is a
/// child of the reader's viewport laid over that place. Copy writes the
/// clipboard, which is presentation; nothing here performs another effect.
namespace bpr::qt::detail {
	class markdown_code_store;
	/// The object type of a framed block's place in the reader.
	inline constexpr int markdown_code_block_object = QTextFormat::UserObject + 85;
	/// Character property of that object: the fenced block's ordinal.
	inline constexpr int markdown_code_block_ordinal = QTextFormat::UserProperty + 0x0B66;

	/// \brief The colours a box is drawn in: the code background, its rule,
	/// the header's ink and the code's own.
	struct markdown_code_block_colours
	{
		QColor Back;
		QColor Rule;
		QColor Muted;
		QColor Normal;
	};

	/// \brief A selectable fenced block with independent overflow and header actions.
	class markdown_code_block final : public QWidget
	{
	public:
		explicit markdown_code_block(QWidget* aParent);

		/// \brief The block's language as shown ("C++", "Plain text") and its
		/// text, its lines joined by LF.
		void set_code(const QString& aLanguage, const std::string& aText);
		void set_fonts(const QFont& aFont, const QFont& aCode, qreal aTabStopDistance);
		void set_colours(const markdown_code_block_colours& aColours);
		/// \brief Each line's syntax formats, as the reader's lines carry them.
		void set_line_formats(const std::vector<QList<QTextLayout::FormatRange>>& aFormats);

		void set_wrapped(bool aWrapped);
		/// \brief The box's height at a width: header, code and, unwrapped
		/// and wider than the box, a scroll bar that takes room.
		[[nodiscard]] auto height_for(int aWidth) const -> int;
		/// \brief Puts exactly the block's text on the clipboard.
		void copy_code();

		/// Called when Wrap changes the height the box needs.
		std::function<void(bool)> WrapChanged;

		[[nodiscard]] auto wrap_button() const noexcept -> QToolButton* { return Wrap; }
		[[nodiscard]] auto copy_button() const noexcept -> QToolButton* { return Copy; }
		/// \brief The header's height, which the code sits under.
		[[nodiscard]] auto header_height() const -> int;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void               arrange();
		void               refresh_icons();
		void               refresh_wrap_names();
		[[nodiscard]] auto header_font() const -> QFont;

		QString                     Language;
		std::string                 Text;
		markdown_code_block_colours Colours;
		bool                        Wrapped   = false;
		bool                        Copied    = false;
		QTextEdit*                  Editor    = nullptr;
		QToolButton*                Wrap      = nullptr;
		QToolButton*                Copy      = nullptr;
		QTimer*                     CopiedFor = nullptr;
	};

	/// \brief Sizes each framed block's place in the reader to its box at the
	/// width the document is laid out at, so the reader's height counts it.
	class markdown_code_block_objects final : public QObject, public QTextObjectInterface
	{
	public:
		explicit markdown_code_block_objects(QTextEdit& aView);

		/// \brief Rebuilds the boxes for placed fences, retaining per-block Wrap.
		void create(const markdown_code_store& aCode);
		void clear();
		void reset_wrap() { Wrapped.clear(); }
		void style(const markdown_code_store& aCode, const code_role_colors& aRoles, const QFont& aFont, const QFont& aCodeFont, qreal aTabStopDistance);
		void place();
		/// The containing message may need a new height after Wrap changes.
		std::function<void()> HeightChanged;

		[[nodiscard]] auto qt_metacast(const char* aName) -> void* override;
		[[nodiscard]] auto intrinsicSize(QTextDocument* aDocument, int aPosition, const QTextFormat& aFormat) -> QSizeF override;
		void               drawObject(QPainter* aPainter, const QRectF& aRect, QTextDocument* aDocument, int aPosition, const QTextFormat& aFormat) override;

		/// \brief The width a block's place has at the document's layout width.
		[[nodiscard]] static auto available_width(const QTextDocument& aDocument, int aPosition) -> int;

	protected:
		bool eventFilter(QObject* aWatched, QEvent* aEvent) override;

	private:
		void                                         schedule();
		void                                         reflow(int aOrdinal);
		QTextEdit&                                   View;
		std::map<int, QPointer<markdown_code_block>> Boxes;
		std::set<int>                                Wrapped;
		bool                                         PlacementQueued = false;
	};
} // namespace bpr::qt::detail

#endif
