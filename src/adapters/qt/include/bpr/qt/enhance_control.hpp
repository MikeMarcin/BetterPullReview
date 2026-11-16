#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ENHANCE_CONTROL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ENHANCE_CONTROL_HPP

#include <QFrame>
#include <QIcon>
#include <QPointer>
#include <QString>
#include <QToolButton>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

class QAbstractButton;
class QLabel;
class QPlainTextEdit;

/// \file
/// \brief [[BPR-2051]]. Enhance on a draft comment and a private note: the
/// control that asks, the panel that shows what came back, and the binding
/// that keeps the two to one editor. Nothing here runs an agent or writes to
/// GitHub: a press leaves as an `enhance_request`, and the answer arrives as
/// an `enhance_outcome`. The editor's text changes only when the person
/// presses Use suggestion, as one edit Undo takes back.
namespace bpr::qt {
	/// \brief Which editor asked.
	enum class enhance_surface : std::uint8_t {
		/// A new draft comment.
		comment,
		/// A reply in a conversation.
		reply,
		/// A private note.
		note,
	};
	/// \brief The control's short, context-aware tooltip (owner look,
	/// 2026-11-10: "much shorter and adapt to the context"): what it
	/// enhances, with the agent.
	[[nodiscard]] auto enhance_tooltip(enhance_surface aSurface, bool aSelection) -> QString;

	/// \brief The drawn options for the control's icon. The owner rejected
	/// pen, lines and wand on 2026-11-10 ("something more like nib and
	/// spark", with a picture of a solid nib whose body has a four-point spark
	/// cut out of it); these are three nibs.
	enum class enhance_glyph_style : std::uint8_t {
		/// A solid nib with the spark cut out of its body, as the owner's picture.
		nib_cut,
		/// A nib drawn in outline, its slit and breather hole, and the spark above it.
		nib_outline,
		/// A solid nib, and the spark beside its shoulder.
		nib_spark,
	};
	inline constexpr std::size_t enhance_glyph_style_count = 3U;
	/// \brief The option's name as the owner's look lists it.
	[[nodiscard]] auto enhance_glyph_style_name(enhance_glyph_style aStyle) -> QString;
	/// \brief The style every Enhance control draws. A look may change it to
	/// compare the options in place; the product keeps the default.
	void               set_enhance_glyph_style(enhance_glyph_style aStyle);
	[[nodiscard]] auto current_enhance_glyph_style() -> enhance_glyph_style;
	/// \brief The icon in `aPalette`'s button text, in the current style or
	/// the one named.
	[[nodiscard]] auto enhance_icon(const QPalette& aPalette, std::optional<enhance_glyph_style> aStyle = std::nullopt) -> QIcon;

	/// \brief One press of Enhance: the editor's text as UTF-8 and, with a
	/// selection, the half-open byte range it rewrites (otherwise the whole).
	struct enhance_request
	{
		std::uint64_t   Id      = 0U;
		enhance_surface Surface = enhance_surface::comment;
		std::string     Raw;
		std::size_t     Begin                                            = 0U;
		std::size_t     End                                              = 0U;
		bool            Selection                                        = false;
		auto            operator==(const enhance_request&) const -> bool = default;
	};

	/// \brief What one press came to: the replacement for the captured range,
	/// or why there is none.
	struct enhance_outcome
	{
		std::uint64_t              Id = 0U;
		std::optional<std::string> Replacement;
		QString                    Notice;
		auto                       operator==(const enhance_outcome&) const -> bool = default;
	};

	using enhance_dispatch = std::function<void(enhance_request)>;
	using enhance_cancel   = std::function<void(std::uint64_t)>;

	/// \brief The Enhance control: an icon-only tool in the editor's own
	/// toolbar. It starts a run, so it wears the Agent accent; while its run
	/// is going it stops it and wears the ordinary roles.
	class enhance_button final : public QToolButton
	{
	public:
		enhance_button(enhance_surface aSurface, QWidget* aParent);
		/// \brief Shows a run going (Stop) or not (Enhance).
		void set_running(bool aRunning);
		/// \brief Whether the editor has text selected, which the tooltip names.
		void               set_selection(bool aSelection);
		[[nodiscard]] auto running() const noexcept -> bool { return Running; }
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void changeEvent(QEvent* aEvent) override;
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		void restyle();

		enhance_surface Surface;
		bool            Running   = false;
		bool            Selection = false;
		bool            Restyling = false;
	};

	/// \brief Under the field: the run going, then the suggestion beside the
	/// person's own text with Use suggestion and Discard, or why there is
	/// none. The host makes the two buttons, so they are drawn as its own.
	class enhance_suggestion final : public QFrame
	{
	public:
		explicit enhance_suggestion(enhance_surface aSurface, QWidget* aParent);
		/// \brief Takes the host's Use and Discard buttons into the panel.
		void               set_buttons(QAbstractButton* aUse, QAbstractButton* aDiscard);
		void               show_running();
		void               show_suggestion(const QString& aText, bool aSelection);
		void               show_notice(const QString& aText);
		void               clear();
		[[nodiscard]] auto suggestion_text() const -> QString;
		[[nodiscard]] auto notice_text() const -> QString;
		[[nodiscard]] auto use_button() const noexcept -> QAbstractButton* { return Use; }
		[[nodiscard]] auto discard_button() const noexcept -> QAbstractButton* { return Discard; }

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		enhance_surface  Surface;
		QLabel*          Heading = nullptr;
		QLabel*          Text    = nullptr;
		QLabel*          Notice  = nullptr;
		QAbstractButton* Use     = nullptr;
		QAbstractButton* Discard = nullptr;
	};

	/// \brief Keeps one editor, its Enhance control and its suggestion panel
	/// together: captures the text a press asks about, shows the answer, and
	/// on Use writes it in place of what was asked about as one undoable edit,
	/// only while the text is still what was asked about.
	class enhance_binding final : public QObject
	{
	public:
		enhance_binding(QPlainTextEdit& aEditor, enhance_surface aSurface, enhance_button& aButton, enhance_suggestion& aPanel, QObject* aParent);
		void set_dispatch(enhance_dispatch aDispatch, enhance_cancel aCancel);
		/// \brief Takes the answer to this binding's own press; any other is ignored.
		auto               present(const enhance_outcome& aOutcome) -> bool;
		[[nodiscard]] auto pending() const noexcept -> std::optional<std::uint64_t> { return Running ? std::optional{ Asked.Id } : std::nullopt; }
		[[nodiscard]] auto suggested() const noexcept -> bool { return Suggestion.has_value(); }
		/// \brief The panel changed height.
		std::function<void()> Changed;
		void                  refresh();
		/// \brief The editor now holds another text (another note): a run
		/// going is stopped and a suggestion dropped, unused.
		void reset();
		/// \brief [[BPR-2192]]. What a press from now on tells the agent it
		/// rewrites: a composer switched to Local asks as a note (kept on this
		/// computer, read by nobody else). The button and panel keep their
		/// words.
		void ask_as(enhance_surface aSurface) noexcept { AskedSurface = aSurface; }

	private:
		void press();
		void use();
		void discard();

		QPointer<QPlainTextEdit>     Editor;
		enhance_surface              Surface;
		enhance_surface              AskedSurface;
		QPointer<enhance_button>     Button;
		QPointer<enhance_suggestion> Panel;
		enhance_dispatch             Dispatch;
		enhance_cancel               Cancel;
		enhance_request              Asked;
		QString                      AskedText;
		int                          AskedBegin = 0;
		int                          AskedEnd   = 0;
		bool                         Running    = false;
		std::optional<QString>       Suggestion;
	};
} // namespace bpr::qt

#endif
