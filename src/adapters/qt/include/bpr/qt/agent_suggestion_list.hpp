#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SUGGESTION_LIST_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SUGGESTION_LIST_HPP

#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <utility>
#include <vector>

class QAction;
class QLabel;
class QMenu;
class QPushButton;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief RFC-BPR-0125. One suggested prompt as a chip shows it: what it
	/// is called and what a press sends or puts in the question field.
	struct agent_suggestion
	{
		/// What a choice names: the macro's identity, or the owner's own key.
		QString Key;
		/// The chip's text: the macro's name, expanded.
		QString Label;
		/// What a press sends, or puts in the field; shown in the tooltip
		/// and the accessible description.
		QString Expansion;
		/// A plain press puts it in the question field (`{cursor}`).
		bool Finish = false;

		auto operator==(const agent_suggestion&) const -> bool = default;
	};

	/// \brief [[BPR-1877]], [[BPR-1934]], RFC-BPR-0125. Suggested prompts for
	/// an agent chat: a quiet lead line, then one quiet chip per prompt, left
	/// aligned and stacked, and past the chips a quiet last chip, More
	/// prompts (n)…, whose menu holds the rest in order. The Agent tab's
	/// thread and every tour chat show them with this one component.
	///
	/// \details A click chooses a prompt; Shift-click, Shift+Space and
	/// Shift+Return choose it to edit first ([[BPR-1872]]), and Return and
	/// Enter choose it as Space does; a More item reads Shift the same way.
	/// Each chip's tooltip is its name when shortened, then what it sends
	/// (its first 12 lines), then how to send or edit it; the accessible name
	/// is the chip's text and the accessible description holds the rest. The
	/// owner decides what a choice does, which prompts there are and when
	/// they show; this only presents them.
	class agent_suggestion_list final : public QWidget
	{
	public:
		/// \brief The prompt keyed `aKey` was chosen, to send (`aEditFirst`
		/// false) or to put in the composer first.
		using choose_dispatch = std::function<void(const QString& aKey, bool aEditFirst)>;

		/// \brief The longest expansion a tooltip shows, in lines.
		static constexpr int tooltip_lines = 12;

		explicit agent_suggestion_list(const QString& aLead, QWidget* aParent = nullptr);
		void set_choose_dispatch(choose_dispatch aChoose) { Choose = std::move(aChoose); }
		/// \brief The chips and More's items, in order. The chips are made
		/// again only when their keys or names change, so the keyboard's
		/// place survives a refresh that changes neither; what they send is
		/// updated in place.
		void present(std::vector<agent_suggestion> aChips, std::vector<agent_suggestion> aMore = { });
		/// \brief Whether a choice can be made now; the chips show disabled
		/// while it cannot. `aWhyNot`, when given, is what must happen first,
		/// in the reviewer's words: each greyed chip and More say it as their
		/// tooltip and to a screen reader, in place of what a press would
		/// send. Without it they keep their own.
		void set_usable(bool aUsable, const QString& aWhyNot = { });
		/// \brief The chips' texts, in order; More's items are not chips.
		[[nodiscard]] auto prompts() const -> QStringList;
		[[nodiscard]] auto chips() const noexcept -> const std::vector<agent_suggestion>& { return Chips; }
		[[nodiscard]] auto more() const noexcept -> const std::vector<agent_suggestion>& { return More; }
		[[nodiscard]] auto buttons() const -> std::vector<QPushButton*>;
		/// \brief More prompts (n)…, or none when every match is a chip.
		[[nodiscard]] auto more_button() const -> QPushButton*;
		[[nodiscard]] auto more_menu() const noexcept -> QMenu* { return MoreMenu; }
		[[nodiscard]] auto lead() const noexcept -> QLabel* { return Lead; }
		/// \brief The tooltip a prompt shows; `aShortened` when its chip
		/// shows less than its whole name.
		[[nodiscard]] static auto tooltip_of(const agent_suggestion& aPrompt, bool aShortened) -> QString;
		[[nodiscard]] static auto description_of(const agent_suggestion& aPrompt) -> QString;

	protected:
		bool eventFilter(QObject* aWatched, QEvent* aEvent) override;
		/// [[BPR-2000]]. The chips wear the agent accent of the ground.
		void changeEvent(QEvent* aEvent) override;

	private:
		void choose(const QString& aKey, bool aEditFirst);
		/// Says what each chip sends, without making it again.
		void dress(QPushButton& aButton, const agent_suggestion& aPrompt) const;
		void present_more();
		void open_more();

		choose_dispatch                    Choose;
		QVBoxLayout*                       Column = nullptr;
		QLabel*                            Lead   = nullptr;
		std::vector<agent_suggestion>      Chips;
		std::vector<agent_suggestion>      More;
		std::vector<QPointer<QPushButton>> Buttons;
		QPointer<QPushButton>              MoreButton;
		QMenu*                             MoreMenu = nullptr;
		bool                               Usable   = true;
		/// Why no choice can be made, said by each chip while it is greyed.
		QString WhyNot;
		bool    EditFirst     = false;
		bool    MoreEditFirst = false;
	};
}

#endif
