#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SIDE_CHAT_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SIDE_CHAT_PANEL_HPP

#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/agent_session.hpp"

#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QListWidget;

namespace bpr::qt {
	/// \brief One supplemental context the reviewer explicitly added.
	///
	/// \details A chip is a label and the scope it was added at, and it
	/// arrives only in a projection. Nothing in this widget appends one:
	/// navigating, refreshing, and a comment arriving cannot produce a chip
	/// because there is no code path here that makes one.
	struct side_chat_context_chip
	{
		core::agent_chat_scope_kind Scope
			= core::agent_chat_scope_kind::selected_lines;
		std::string Label;

		auto operator<=>(const side_chat_context_chip&) const = default;
	};

	struct side_chat_reply
	{
		std::uint64_t Id = 0U;
		std::string   Text;
		auto          operator<=>(const side_chat_reply&) const = default;
	};
	/// \brief Everything the side chat is told to show, and nothing it computed.
	///
	/// \details **There is no field here the head now under review could
	/// occupy.** The panel is handed the session's own binding and the
	/// continuation offer core already decided; it is never handed the
	/// observed revision, so no branch below it can retarget a session to a
	/// head the reviewer happens to be looking at. That is BPR-0499's
	/// `agent_continuation_offer` property carried one layer outward.
	///
	/// The five parts of the RFC's visible session contract — provider, Git
	/// host, workspace, the selected epoch pair, and the exact head OID — are
	/// separate members here and separate fields on screen. A single
	/// pre-rendered summary string would let one of them go missing without
	/// anything failing.
	struct side_chat_projection
	{
		/// The session this chat is, pinned to what it was asked about.
		core::agent_session Session;
		/// The choices core offers against the revision now under review.
		core::agent_continuation_offer Continuations;
		/// Where this chat is, and where it was widened from.
		core::agent_chat_state Chat;
		/// Whether this session may carry the reviewer's private prose.
		core::agent_private_context_optin PrivateContext;
		/// The explicit action this chat is, named on every message it sends.
		core::agent_entry_point EntryPoint
			= core::agent_entry_point::ask_about_selection;
		std::string                         Provider;
		std::string                         GitHost;
		std::string                         Workspace;
		std::vector<side_chat_context_chip> Chips;
		std::vector<side_chat_reply>        Replies;

		auto operator<=>(const side_chat_projection&) const = default;
	};

	/// \brief Asks for one chat turn to be sent, as the reviewer typed it.
	///
	/// \details `Action` is a `core::agent_action_intent`, which is neither
	/// default constructible nor implicitly convertible from an entry point,
	/// so every message leaving this widget names the explicit action it is.
	struct send_chat_message_intent
	{
		core::agent_session_id    Session;
		core::agent_action_intent Action;
		/// The scope shown when the reviewer sent it, not one resolved later.
		core::agent_chat_scope Scope;
		std::string            Text;

		auto operator<=>(const send_chat_message_intent&) const = default;
	};

	/// \brief Asks for the chat's scope to widen, which is a context action.
	struct widen_chat_scope_intent
	{
		core::agent_session_id            Session;
		core::agent_scope_widening_intent Widening;

		auto operator<=>(const widen_chat_scope_intent&) const = default;
	};

	/// \brief Asks for this session's private-prose opt-in to change.
	///
	/// \details The core opt-in carries the session it is for, so there is no
	/// second session identity here that could disagree with it.
	struct chat_private_context_intent
	{
		core::agent_private_context_optin OptIn;

		auto operator<=>(const chat_private_context_intent&) const = default;
	};

	/// \brief Chooses how to continue a session that no longer matches.
	///
	/// \details `OfferedBinding` is copied from the offer the panel was
	/// presented, which is the session's own binding. The panel has no other
	/// binding to put here.
	struct choose_chat_continuation_intent
	{
		core::agent_session_id       Session;
		core::agent_continuation     Choice;
		core::agent_revision_binding OfferedBinding;

		auto operator<=>(const choose_chat_continuation_intent&) const = default;
	};

	struct promote_chat_reply_intent
	{
		core::agent_session_id Session;
		std::uint64_t          Reply = 0U;
	};

	using side_chat_intent = std::variant<
		send_chat_message_intent,
		widen_chat_scope_intent,
		chat_private_context_intent,
		choose_chat_continuation_intent,
		promote_chat_reply_intent>;

	using side_chat_intent_dispatch = std::function<void(side_chat_intent)>;

	/// \brief Records directly observable side-chat activity.
	struct side_chat_panel_statistics
	{
		std::uint64_t SendIntents           = 0U;
		std::uint64_t ScopeIntents          = 0U;
		std::uint64_t PrivateContextIntents = 0U;
		std::uint64_t ContinuationIntents   = 0U;
		std::uint64_t Presentations         = 0U;
		std::uint64_t Observations          = 0U;

		auto operator<=>(const side_chat_panel_statistics&) const = default;
	};

	/// \brief Every member the side-chat panel holds, as one readable value.
	///
	/// \details Declared as one aggregate, and held as the panel's only data
	/// member, so that "this widget holds no launcher, no process, no
	/// credential, and no network address" is a list a test reads rather than
	/// a claim a reviewer checks by eye. A guard in
	/// `tests/adapters_qt/side_chat_panel_tests.cpp` binds every member here
	/// by name and names its type, so adding one more member — of any type —
	/// fails to compile, and the same test pins `sizeof(side_chat_panel)` to
	/// this aggregate so a member added to the class outside it fails too.
	///
	/// What is here is a dispatch callback, the accepted projection, six
	/// counters, one observation record, and the widgets that show them.
	/// There is nothing to start, nothing to authenticate to, and nowhere to
	/// send anything: the only way out of this widget is an intent.
	struct side_chat_panel_members
	{
		/// True only while `present` is writing the accepted projection into
		/// the controls, so presenting never looks like a reviewer choosing.
		bool Applying = false;
		/// The last thing that happened while the review was open. Recording
		/// it is all an observation may become.
		core::agent_observation_record LastObservation;

		side_chat_panel_statistics          Statistics;
		side_chat_intent_dispatch           Dispatch;
		std::optional<side_chat_projection> Shown;

		QListWidget*    Replies            = nullptr;
		QPlainTextEdit* ReplyBody          = nullptr;
		QAction*        Promote            = nullptr;
		bool            PromotionAvailable = false;
		QString         PromotionReason;
		QLabel*         Provider            = nullptr;
		QLabel*         GitHost             = nullptr;
		QLabel*         Workspace           = nullptr;
		QLabel*         EpochPair           = nullptr;
		QLabel*         HeadOid             = nullptr;
		QLabel*         SessionState        = nullptr;
		QLabel*         Scope               = nullptr;
		QLabel*         Chips               = nullptr;
		QLabel*         PrivateContext      = nullptr;
		QLabel*         Continuations       = nullptr;
		QPlainTextEdit* Composer            = nullptr;
		QComboBox*      ScopeSelector       = nullptr;
		QCheckBox*      NoteOptIn           = nullptr;
		QCheckBox*      DraftOptIn          = nullptr;
		QAction*        Send                = nullptr;
		QAction*        AddContext          = nullptr;
		QAction*        NewSession          = nullptr;
		QAction*        ContinueOldRevision = nullptr;
	};

	/// \brief Shows what a reviewer needs to know before typing to an agent.
	///
	/// \details Nothing the reviewer does changes what this panel shows. Every
	/// gesture emits an intent and waits; the displayed state moves only when
	/// `present` is called with an accepted projection, exactly as
	/// `review_draft_editor` already works. A panel that added its own chip
	/// when the reviewer asked for one would show context as sent while the
	/// core had not yet agreed to send it — and would keep showing it if the
	/// core refused.
	///
	/// The three things that must not silently add context — refreshing,
	/// navigating, and a comment arriving — arrive through `observe`, which
	/// records that they happened and does nothing else. There is no dispatch
	/// call in it and no chip appended by it.
	class side_chat_panel final : public QWidget
	{
	public:
		explicit side_chat_panel(
			side_chat_intent_dispatch aDispatch = { },
			QWidget*                  aParent   = nullptr);

		void               set_dispatch(side_chat_intent_dispatch aDispatch);
		void               set_promotion_target(bool aAvailable, const QString& aReason);
		[[nodiscard]] auto replies_list() const noexcept -> QListWidget*;
		[[nodiscard]] auto promote_action() const noexcept -> QAction*;

		/// \brief Adopts one accepted projection, and starts nothing.
		void present(const side_chat_projection& aProjection);

		/// \brief Notes that something happened, and asks the agent nothing.
		void observe(core::agent_observation aObservation);

		[[nodiscard]] auto provider_text() const -> QString;
		[[nodiscard]] auto git_host_text() const -> QString;
		[[nodiscard]] auto workspace_text() const -> QString;
		[[nodiscard]] auto epoch_pair_text() const -> QString;
		[[nodiscard]] auto head_oid_text() const -> QString;
		[[nodiscard]] auto session_state_text() const -> QString;
		[[nodiscard]] auto scope_text() const -> QString;
		[[nodiscard]] auto chips_text() const -> QString;
		[[nodiscard]] auto private_context_text() const -> QString;
		[[nodiscard]] auto continuations_text() const -> QString;

		/// \brief The continuations the reviewer is being offered right now.
		///
		/// \returns Empty unless the presented offer requires an answer, and
		/// otherwise exactly what core offered, in core's order.
		[[nodiscard]] auto offered_continuations() const
			-> std::vector<core::agent_continuation>;
		[[nodiscard]] auto chip_count() const noexcept -> std::size_t;

		[[nodiscard]] auto composer() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto scope_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto note_optin() const noexcept -> QCheckBox*;
		[[nodiscard]] auto draft_optin() const noexcept -> QCheckBox*;
		[[nodiscard]] auto send_action() const noexcept -> QAction*;
		[[nodiscard]] auto add_context_action() const noexcept -> QAction*;
		[[nodiscard]] auto new_session_action() const noexcept -> QAction*;
		[[nodiscard]] auto continue_old_revision_action() const noexcept
			-> QAction*;

		[[nodiscard]] auto last_observation() const noexcept
			-> core::agent_observation_record;
		[[nodiscard]] auto statistics() const noexcept
			-> side_chat_panel_statistics;

	private:
		void emit_private_context();
		void emit_continuation(core::agent_continuation aChoice);
		void refresh_enablement();

		side_chat_panel_members Members;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SIDE_CHAT_PANEL_HPP
