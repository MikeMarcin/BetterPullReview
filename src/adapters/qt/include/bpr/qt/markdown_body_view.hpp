#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MARKDOWN_BODY_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MARKDOWN_BODY_VIEW_HPP

#include "bpr/core/markdown_image.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QFont>
#include <QWidget>

#include <functional>
#include <string>

class QLabel;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTextEdit;
class QTextCursor;
class QTimer;

namespace bpr::application {
	struct markdown_image_projection;
}

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace bpr::qt {
	class markdown_text_view;
	namespace detail {
		class markdown_image_store;
	}

	/// \brief A native, resource-free Markdown reader with original source and copy.
	/// \details Call only on the GUI lane with an already admitted provider body.
	/// Identity is supplied by the owning surface; replace it on authority/body
	/// identity changes. An identical identity and body retains both documents.
	/// Link dispatch reports untrusted destination text from an explicit gesture;
	/// the application must validate it against current context before any effect.
	class markdown_body_view final : public QWidget
	{
	public:
		explicit markdown_body_view(QWidget* aParent = nullptr);

		/// \brief Replaces one admitted body; identical identity and bytes are a no-op.
		void set_body(std::string aBody, std::string aIdentity);
		/// \brief Discards both documents and all local reading/link state.
		void clear();
		/// \brief Binds untrusted link gestures to the application admission lane.
		void set_link_dispatch(std::function<void(const std::string&)> aDispatch);
		/// \brief RFC-BPR-0084. Shows this body's images inline, asking the
		/// application for each one's pixels through `aDispatch`.
		/// \details Without a dispatch every image stays the accessible text
		/// placeholder and nothing is asked for. The references are untrusted
		/// source text in document order; answers come back by `apply_image`.
		void set_image_dispatch(std::function<void(std::vector<core::markdown_image_reference>)> aDispatch);
		/// \brief Asks again for every image still loading, after the holder's
		/// accepted context changed.
		void request_images();
		/// \brief Shows one image's answer; ignored unless it names an image of
		/// the current body by ordinal and the source this body sent.
		void apply_image(const application::markdown_image_projection& aImage);
		/// \brief The current body's images, for tests and accessibility.
		[[nodiscard]] auto image_store() const noexcept -> const detail::markdown_image_store*;
		/// \brief Uses the composed code typography without overriding the native palette.
		void set_code_font(const QFont& aFont, qreal aTabStopDistance);
		/// \brief Switches modes, retaining independent positions and selections.
		void show_source(bool aSource);
		/// \brief Moves source switching into the native reader context menu and hides action rows.
		/// \details Keeps scrolling and both documents; selection copy supplies Markdown text only.
		void set_context_controls(bool aContextControls);
		/// \brief Lets the owning surface append semantic gestures to either reader menu.
		void set_context_menu_builder(std::function<void(QMenu&)> aBuilder);
		/// \brief Copies original bytes, without reconstructing rendered text.
		void copy_source() const;
		/// \brief [[BPR-1083]]. Shows a recognized Conventional Comments prefix's
		/// mark, named "issue (non-blocking, if-minor)", leading the controls.
		/// The rendered body still reads as written, its header and
		/// decorations included (owner, `decorations-v003`). Off by default,
		/// so a description or a tour preview shows no mark.
		void               set_feedback_marks(const feedback_mark_theme& aTheme);
		[[nodiscard]] auto feedback_mark_label() const noexcept -> QLabel*;
		/// \brief [[BPR-1290]]. Reads one post in place, in an open thread of
		/// the conversations drawer: as tall as its content at a width
		/// (`heightForWidth`), with no vertical scrolling of its own, so a
		/// vertical wheel goes to what holds it; wide code and tables still
		/// scroll sideways. Its own source controls and destination line are
		/// hidden: the holder offers View source and Copy source through
		/// `show_source` and `copy_source`, and a link's destination is its
		/// tooltip. Headings keep to the body's scale, and a recognized
		/// Conventional Comments header is set in semibold, as the notes set it
		/// ([[BPR-1083]]). Call before `set_body`.
		void               set_in_place(bool aInPlace);
		[[nodiscard]] auto in_place() const noexcept -> bool;
		/// \brief Called when the height for a width changes in place: a new
		/// body, a source switch, a font or palette change.
		void set_height_observer(std::function<void()> aObserver);
		/// \brief In place, how many characters at the start of the rendered
		/// text are the recognized header set in semibold; zero otherwise.
		[[nodiscard]] auto header_length() const noexcept -> qsizetype;
		[[nodiscard]] auto hasHeightForWidth() const -> bool override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;

		[[nodiscard]] auto original_body() const noexcept -> const std::string&;
		[[nodiscard]] auto source_visible() const noexcept -> bool;
		[[nodiscard]] auto rendered_available() const noexcept -> bool;
		[[nodiscard]] auto fallback_reason() const -> QString;
		[[nodiscard]] auto rendered_view() const noexcept -> QTextEdit*;
		[[nodiscard]] auto source_view() const noexcept -> QPlainTextEdit*;
		/// Exact raw UTF-8 byte span for the native source selection; refuses a changed display.
		[[nodiscard]] auto selected_source_bytes() const -> std::optional<std::pair<std::size_t, std::size_t>>;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void changeEvent(QEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;
		void hideEvent(QHideEvent* aEvent) override;

	private:
		void               prepare_rendered();
		void               refresh_mark();
		void               refresh_formats();
		void               height_changed();
		void               refresh_controls();
		void               refresh_control_row();
		void               show_context_menu(bool aSource, const QPoint& aPosition);
		void               dispatch_images();
		[[nodiscard]] auto selected_markdown(const QTextCursor& aCursor) const -> QString;
		void               schedule_animation();
		[[nodiscard]] auto dark_palette() const -> bool;

		std::string                                                      Body;
		std::string                                                      Identity;
		std::function<void(const std::string&)>                          LinkDispatch;
		std::function<void(QMenu&)>                                      ContextMenuBuilder;
		std::function<void(std::vector<core::markdown_image_reference>)> ImageDispatch;
		detail::markdown_image_store*                                    Images           = nullptr;
		QTimer*                                                          Animation        = nullptr;
		std::int64_t                                                     AnimationStarted = 0;
		markdown_text_view*                                              Rendered         = nullptr;
		QPlainTextEdit*                                                  Source           = nullptr;
		QStackedWidget*                                                  Pages            = nullptr;
		QWidget*                                                         Controls         = nullptr;
		QPushButton*                                                     Toggle           = nullptr;
		QPushButton*                                                     Copy             = nullptr;
		QLabel*                                                          Notice           = nullptr;
		QLabel*                                                          Destination      = nullptr;
		QLabel*                                                          Mark             = nullptr;
		std::optional<feedback_mark_theme>                               MarkTheme;
		feedback_body_mark                                               BodyMark;
		QFont                                                            CodeFont;
		qreal                                                            TabStopDistance   = 0;
		bool                                                             RenderedAvailable = false;
		bool                                                             VisitedSource     = false;
		bool                                                             VisitedRendered   = false;
		QString                                                          Fallback;
		bool                                                             InPlace         = false;
		bool                                                             ContextControls = false;
		std::function<void()>                                            HeightObserver;
		qsizetype                                                        HeaderChars   = 0;
		qsizetype                                                        HeaderStart   = 0;
		qsizetype                                                        SourceColumns = 0;
		mutable int                                                      CachedWidth   = -1;
		mutable int                                                      CachedHeight  = 0;
	};
} // namespace bpr::qt

#endif
