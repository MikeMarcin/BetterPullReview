#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SUGGESTION_LIST_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_SUGGESTION_LIST_HPP

#include <QPointer>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <utility>
#include <vector>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief [[BPR-1877]], [[BPR-1934]]. Suggested prompts for an agent chat:
	/// a quiet lead line, then one quiet chip per prompt, left aligned and
	/// stacked. The Agent tab's thread and every tour chat show them with
	/// this one component.
	///
	/// \details A click chooses a prompt; Shift-click, Shift+Space and
	/// Shift+Return choose it to edit first ([[BPR-1872]]), and Return and
	/// Enter choose it as Space does. Each chip carries the whole prompt as
	/// its `agentPrompt` property, its tooltip and its accessible name; a
	/// long prompt is shortened in its middle on the chip only. The owner
	/// decides what a choice does, which prompts there are and when they
	/// show; this only presents them.
	class agent_suggestion_list final : public QWidget
	{
	public:
		/// \brief `aPrompt` was chosen, to send (`aEditFirst` false) or to put
		/// in the composer first.
		using choose_dispatch = std::function<void(const QString& aPrompt, bool aEditFirst)>;

		explicit agent_suggestion_list(const QString& aLead, QWidget* aParent = nullptr);
		void set_choose_dispatch(choose_dispatch aChoose) { Choose = std::move(aChoose); }
		/// \brief The prompts, in order. The chips are made again only when
		/// the prompts differ, so the keyboard's place survives a refresh.
		void present(const QStringList& aPrompts);
		/// \brief Whether a choice can be made now; the chips show disabled
		/// while it cannot.
		void               set_usable(bool aUsable);
		[[nodiscard]] auto prompts() const -> const QStringList& { return Prompts; }
		[[nodiscard]] auto buttons() const -> std::vector<QPushButton*>;
		[[nodiscard]] auto lead() const noexcept -> QLabel* { return Lead; }

	protected:
		bool eventFilter(QObject* aWatched, QEvent* aEvent) override;

	private:
		void choose(const QString& aPrompt, bool aEditFirst);

		choose_dispatch                    Choose;
		QVBoxLayout*                       Column = nullptr;
		QLabel*                            Lead   = nullptr;
		QStringList                        Prompts;
		std::vector<QPointer<QPushButton>> Buttons;
		bool                               Usable    = true;
		bool                               EditFirst = false;
	};
}

#endif
