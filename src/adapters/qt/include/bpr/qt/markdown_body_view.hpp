#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MARKDOWN_BODY_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_MARKDOWN_BODY_VIEW_HPP

#include "bpr/qt/feedback_label_marks.hpp"

#include <QFont>
#include <QWidget>

#include <functional>
#include <string>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTextEdit;

#include <optional>
#include <utility>

namespace bpr::qt {
	class markdown_text_view;

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
		/// \brief Uses the composed code typography without overriding the native palette.
		void set_code_font(const QFont& aFont, qreal aTabStopDistance);
		/// \brief Switches modes, retaining independent positions and selections.
		void show_source(bool aSource);
		/// \brief Copies original bytes, without reconstructing rendered text.
		void copy_source() const;
		/// \brief [[BPR-1083]]. Reads a recognized Conventional Comments prefix
		/// as a mark: the rendered body starts at the subject and the mark,
		/// named "issue (blocking)", leads the controls. Source and Copy source
		/// keep the prefix byte for byte. Off by default, so a description or
		/// a tour preview reads as written.
		void               set_feedback_marks(const feedback_mark_theme& aTheme);
		[[nodiscard]] auto feedback_mark_label() const noexcept -> QLabel*;

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

	private:
		void prepare_rendered();
		void refresh_mark();
		void refresh_formats();

		std::string                             Body;
		std::string                             Identity;
		std::function<void(const std::string&)> LinkDispatch;
		markdown_text_view*                     Rendered    = nullptr;
		QPlainTextEdit*                         Source      = nullptr;
		QStackedWidget*                         Pages       = nullptr;
		QPushButton*                            Toggle      = nullptr;
		QLabel*                                 Notice      = nullptr;
		QLabel*                                 Destination = nullptr;
		QLabel*                                 Mark        = nullptr;
		std::optional<feedback_mark_theme>      MarkTheme;
		feedback_body_mark                      BodyMark;
		QFont                                   CodeFont;
		qreal                                   TabStopDistance   = 0;
		bool                                    RenderedAvailable = false;
		bool                                    VisitedSource     = false;
		bool                                    VisitedRendered   = false;
		QString                                 Fallback;
	};
} // namespace bpr::qt

#endif
