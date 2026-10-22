#ifndef BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_REACTIONS_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_REACTIONS_HPP

#include "conversation_open_card.hpp"

#include "bpr/core/conversation_reactions.hpp"

#include <QAbstractButton>
#include <QFrame>
#include <QRect>
#include <QString>
#include <QWidget>

#include <functional>
#include <optional>
#include <vector>

class QKeyEvent;
class QPaintEvent;
class QResizeEvent;

namespace bpr::qt {
	/// \brief [[BPR-PENDING-AR1]]. Who reacted with one reaction, in words, as
	/// GitHub says it: "You and octocat reacted with thumbs up", up to the
	/// names GitHub gave, then "and N others".
	[[nodiscard]] auto reaction_description(const core::reaction_group& aGroup, const QString& aViewer) -> QString;

	/// \brief [[BPR-PENDING-AR1]]. One reaction under a post: its emoji and
	/// count in a pill, tinted in the accent when it is mine and outlined in
	/// dashes while GitHub has not answered my change. Pressing it adds or
	/// removes mine; it is disabled when GitHub does not let me react.
	class reaction_chip final : public QAbstractButton
	{
	public:
		reaction_chip(const core::reaction_group& aGroup, const note_colours& aColours, bool aCanReact, const QString& aViewer, QWidget* aParent);
		[[nodiscard]] auto content() const noexcept -> core::reaction_content;
		[[nodiscard]] auto reaction() const noexcept -> const core::reaction_group&;
		[[nodiscard]] auto sizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		core::reaction_group Group;
		note_colours         Colours;
	};

	/// \brief [[BPR-PENDING-AR1]]. A post's reactions, under its body: the
	/// non-zero ones in GitHub's order, wrapping to the post's width. Empty,
	/// and no height, when the post has none or none were read.
	class reaction_bar final : public QWidget
	{
	public:
		reaction_bar(const note_colours& aColours, QWidget* aParent);
		void               set_reactions(const std::optional<core::reaction_summary>& aSummary, const QString& aViewer);
		[[nodiscard]] auto chips() const -> std::vector<reaction_chip*>;
		[[nodiscard]] auto hasHeightForWidth() const -> bool override;
		[[nodiscard]] auto heightForWidth(int aWidth) const -> int override;
		[[nodiscard]] auto sizeHint() const -> QSize override;

		/// \brief A chip was pressed: the reaction to add or remove.
		std::function<void(core::reaction_content)> Toggled;

	protected:
		void resizeEvent(QResizeEvent* aEvent) override;

	private:
		/// Where each chip goes at a width, wrapping to new rows.
		[[nodiscard]] auto arranged(int aWidth) const -> std::vector<QRect>;
		void               place();

		note_colours                Colours;
		std::vector<reaction_chip*> Chips;
	};

	/// \brief [[BPR-PENDING-AR1]]. The eight reactions GitHub offers, in its
	/// order, as a small popup: each a checkable button named for its
	/// reaction, checked when it is mine. The arrow keys move between them,
	/// Space or Return chooses and closes, Esc closes.
	class reaction_picker final : public QFrame
	{
	public:
		reaction_picker(const std::optional<core::reaction_summary>& aSummary, const note_colours& aColours, QWidget* aParent);
		/// \brief Shows the picker with its top left at \p aGlobal, kept on the
		/// screen, and focuses its first reaction.
		void               popup_at(QPoint aGlobal);
		[[nodiscard]] auto buttons() const -> const std::vector<QAbstractButton*>&;

		/// \brief A reaction was chosen; the picker has closed.
		std::function<void(core::reaction_content)> Chosen;

	protected:
		void keyPressEvent(QKeyEvent* aEvent) override;
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		void choose(core::reaction_content aContent);

		note_colours                  Colours;
		std::vector<QAbstractButton*> Buttons;
	};
} // namespace bpr::qt

#endif
