#ifndef BPR_SRC_ADAPTERS_QT_SRC_COMMENT_STATE_TOGGLE_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_COMMENT_STATE_TOGGLE_HPP

#include <QPalette>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>

namespace bpr::qt {
	class quiet_tool_button;

	/// \brief RFC-BPR-0127, owner look v003 to v005. The one control that
	/// shows and changes where my unsent comment lives: Local (a computer)
	/// and Draft (a pencil), two icon segments joined, as the composer's
	/// foot has them.
	/// \details The owner, on the composer's foot: "This one seems much
	/// simpler and subtler than the big ass make draft button". Every
	/// surface that moves a comment between Local and Draft uses it: the
	/// composer, a Local comment's card, a Local reply, my draft's card, my
	/// draft reply, Finish your review's rows and the agent's keep actions.
	/// A move that is not allowed disables its segment and says why in the
	/// segment's tooltip alone; a refused Draft shows the pencil struck
	/// through. Where nothing is kept yet (an agent's answer), neither
	/// segment is on and each keeps the text as that state.
	class comment_state_toggle final : public QWidget
	{
	public:
		enum class state : std::uint8_t {
			/// Nothing kept yet: each segment is an action.
			none,
			local,
			draft,
		};

		comment_state_toggle(QPalette::ColorRole aSurface, QWidget* aParent);

		void               set_state(state aState);
		[[nodiscard]] auto current() const noexcept -> state { return State; }

		/// \brief Whether Local may be chosen, and its tooltip (why not, when
		/// it may not). The tooltip is at most 60 characters.
		void set_local(bool aAllowed, const QString& aTip);
		/// \brief Whether Draft may be chosen, and its tooltip. A refused
		/// Draft shows the pencil struck through.
		void set_draft(bool aAllowed, const QString& aTip);
		/// \brief What each segment is called to assistive technology.
		void set_names(const QString& aLocal, const QString& aDraft);

		[[nodiscard]] auto local_button() const noexcept -> quiet_tool_button* { return Local; }
		[[nodiscard]] auto draft_button() const noexcept -> quiet_tool_button* { return Draft; }
		[[nodiscard]] auto draft_refused() const noexcept -> bool { return DraftRefused; }

		/// \brief Local pressed while it is not the state.
		std::function<void()> LocalChosen;
		/// \brief Draft pressed while it is not the state.
		std::function<void()> DraftChosen;

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void refresh_icons();
		void press(bool aLocal);

		quiet_tool_button* Local        = nullptr;
		quiet_tool_button* Draft        = nullptr;
		state              State        = state::none;
		bool               DraftRefused = false;
	};
} // namespace bpr::qt

#endif
