#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DESTRUCTIVE_QUESTION_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_DESTRUCTIVE_QUESTION_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QToolButton>

#include <cstdint>
#include <optional>
#include <string>

class QAbstractButton;
class QLabel;
class QPalette;
class QScrollArea;
class QSpacerItem;

/// \file
/// \brief [[BPR-1345]] v003. The one question asked before something in the
/// compose flow is deleted, and the red its deleting button is drawn in.
/// \details The owner rejected v002's native message box (delete-comment
/// v002: "Render the comment better. Put it in a box and start the comment
/// below the location record. Render it basically like it would appear in the
/// drawer. Make sure the comment is clearly separated from the dialog
/// information. Color the delete box red ane ensure it looks dangerous sort of
/// like this one from codex"), and asked the same of Discard review. The
/// reference is a rounded panel in the theme's own surface: a bold question, a
/// close mark, muted words that end "This can't be undone.", and a neutral
/// Cancel beside a deleting button in red words on a red tint.
///
/// Everything is painted from the palette the owning window carries (the
/// themes are applied per window, so the question takes the window's palette
/// rather than the application's). Presentation only: the owner of the
/// question decides what a confirmation does.
namespace bpr::qt {
	class markdown_body_view;

	/// \brief The colours one destructive question is drawn in, derived from a
	/// palette and the surface it sits on.
	struct destructive_colours
	{
		/// \brief The question's own panel, raised from the window.
		QColor Surface;
		/// \brief The panel's hairline edge.
		QColor Edge;
		/// \brief The words of the title and the location's file.
		QColor Ink;
		/// \brief The quiet words, at least 4.5:1 on the surface.
		QColor Muted;
		/// \brief The recessed ground of the comment's box, and its edge.
		QColor Recess;
		QColor RecessEdge;
		/// \brief Cancel's fill and edge.
		QColor Neutral;
		QColor NeutralEdge;
		/// \brief The deleting button's words, on its tint at least 4.5:1
		/// (7:1 in a high-contrast theme).
		QColor Danger;
		/// \brief The deleting button's fill: the surface tinted red.
		QColor DangerTint;
		/// \brief The deleting button's edge.
		QColor DangerEdge;
		/// \brief The keyboard focus ring.
		QColor Focus;
		bool   Dark         = false;
		bool   HighContrast = false;
	};

	/// \brief The colours of a destructive question, or of a deleting control,
	/// on `aSurface`; with none, on the raised panel the question draws.
	[[nodiscard]] auto destructive_colours_of(const QPalette& aPalette, std::optional<QColor> aSurface = std::nullopt) -> destructive_colours;

	/// \brief The WCAG contrast ratio of two colours, 1 to 21.
	[[nodiscard]] auto destructive_contrast(const QColor& aFirst, const QColor& aSecond) -> double;

	/// \brief Which lines a comment is on, as the question says it: "line 5",
	/// "lines 3–4", "base line 5". `aLine` and `aStartLine` are zero-based.
	[[nodiscard]] auto comment_lines(core::diff_side aSide, std::uint32_t aLine, std::optional<std::uint32_t> aStartLine) -> QString;

	/// \brief How a question's button is drawn.
	enum class question_button_role : std::uint8_t {
		/// \brief Cancel: the surface lifted a little, with a hairline.
		neutral,
		/// \brief The deleting button: red words on a red tint.
		destructive,
	};

	/// \brief One of a destructive question's two buttons, painted from the
	/// question's colours; its role is also its `questionRole` property.
	class question_button final : public QPushButton
	{
	public:
		question_button(const QString& aText, question_button_role aRole, QWidget* aParent);
		[[nodiscard]] auto role() const noexcept -> question_button_role;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void enterEvent(QEnterEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;

	private:
		question_button_role Role;
	};

	/// \brief [[BPR-1345]] v003. A deleting control outside a question (Discard
	/// review on Finish your review): red words and no fill at rest, the red
	/// tint and edge under the pointer, the highlight's ring on focus. A
	/// secondary danger, never filled, so it does not compete with the
	/// verdicts.
	class danger_tool_button final : public QToolButton
	{
	public:
		explicit danger_tool_button(QWidget* aParent);
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;
		/// \brief The words' colour now, for a check of its contrast.
		[[nodiscard]] auto ink() const -> QColor;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void enterEvent(QEnterEvent* aEvent) override;
		void leaveEvent(QEvent* aEvent) override;
	};

	/// \brief [[BPR-1345]] v003. Asks before something is deleted.
	/// \details Window modal over its owner's window, deleted on close. Its
	/// children are named from `aName`: `<name>Confirm` (the deleting button),
	/// `<name>Cancel` (the default, focused when shown) and `<name>Close` (the
	/// close mark). Esc, Cancel and the close mark reject; the deleting button
	/// accepts. Connect to `accepted` for the deletion.
	class destructive_question final : public QDialog
	{
	public:
		/// \param aQuestion The bold title, which is also the window's title
		/// and the accessible name: "Delete this comment?".
		/// \param aDetail The muted words, ending "This can't be undone.".
		/// \param aConfirm The deleting button's words: "Delete comment".
		destructive_question(QWidget& aOwner, const QString& aName, const QString& aQuestion, const QString& aDetail, const QString& aConfirm);

		/// \brief Shows what is deleted between the title and the detail: its
		/// location on one line (`aFile`, the path's last part, and
		/// `aLines`), and below it the comment as the drawer reads it (its
		/// label's mark, the rendered Markdown, the code font) in a bordered
		/// box, apart from the question's own words.
		void set_subject(const QString& aFile, const QString& aLines, const std::string& aBody, const std::string& aIdentity, const feedback_mark_theme& aMarks, const QFont& aCodeFont);

		[[nodiscard]] auto question() const -> QString;
		[[nodiscard]] auto detail() const -> QString;
		/// \brief "renderer.cpp, lines 3–4", or empty with no subject.
		[[nodiscard]] auto location() const -> QString;
		[[nodiscard]] auto title_label() const noexcept -> QLabel*;
		[[nodiscard]] auto detail_label() const noexcept -> QLabel*;
		[[nodiscard]] auto location_label() const noexcept -> QLabel*;
		/// \brief The bordered box that holds the comment; hidden with none.
		[[nodiscard]] auto subject_box() const noexcept -> QWidget*;
		/// \brief The comment's reader, or null with no subject.
		[[nodiscard]] auto subject_view() const noexcept -> markdown_body_view*;
		/// \brief The comment's label mark in the box's left gutter, where
		/// the drawer draws it; hidden for a comment with no recognized label.
		[[nodiscard]] auto subject_mark() const noexcept -> QLabel*;
		[[nodiscard]] auto confirm_button() const noexcept -> question_button*;
		[[nodiscard]] auto cancel_button() const noexcept -> question_button*;
		[[nodiscard]] auto close_button() const noexcept -> QAbstractButton*;
		[[nodiscard]] auto colours() const -> destructive_colours;
		/// \brief The panel's corner radius, in pixels.
		static constexpr qreal corner_radius = 14.0;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		/// \brief Follows the owning window's theme and font while open.
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		void refresh_colours();
		void refresh_mark();
		void fit_subject();
		void fit_height();

		QLabel*                            Title     = nullptr;
		QLabel*                            Detail    = nullptr;
		QLabel*                            File      = nullptr;
		QLabel*                            Lines     = nullptr;
		QWidget*                           Location  = nullptr;
		QWidget*                           Box       = nullptr;
		QScrollArea*                       Scroll    = nullptr;
		markdown_body_view*                Reader    = nullptr;
		question_button*                   Confirm   = nullptr;
		question_button*                   Cancel    = nullptr;
		QAbstractButton*                   Close     = nullptr;
		QSpacerItem*                       GapAbove  = nullptr;
		QSpacerItem*                       GapBox    = nullptr;
		QSpacerItem*                       GapDetail = nullptr;
		QPointer<QWidget>                  Owner;
		QString                            Where;
		QLabel*                            Mark = nullptr;
		std::string                        Body;
		std::optional<feedback_mark_theme> MarkTheme;
	};
} // namespace bpr::qt

#endif
