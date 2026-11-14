/// \file
/// \brief RFC-BPR-0128 ([[BPR-2163]], [[BPR-2164]]). The Ask menus in the
/// window: the facts each menu is decided on, read from what the window
/// already holds (no request, no agent call), and where a press goes. Every
/// press goes through the existing launch paths of RFC-BPR-0101: a quick ask
/// is the suggested prompt's own send, and an action row is the action's own.

#include "github_application/window_composition.hpp"

#include "bpr/core/quick_ask.hpp"

#include <algorithm>

namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	namespace {
		/// Commits listed after `aCommit`, or none when it is not listed.
		[[nodiscard]] auto commits_after(const std::vector<application::github_commit_record>& aCommits, const std::string& aCommit) -> std::optional<std::uint32_t>
		{
			if (aCommit.empty()) {
				return std::nullopt;
			}
			const auto found = std::ranges::find_if(aCommits, [&aCommit](const application::github_commit_record& aRecord) { return aRecord.Oid.value() == aCommit; });
			if (found == aCommits.end()) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(std::distance(found, aCommits.end()) - 1);
		}
	} // namespace

	void github_application::implementation::bind_quick_ask()
	{
		if (Window == nullptr || Window->agent_tab() == nullptr) {
			return;
		}
		Window->set_quick_ask([this](const qt::ask_menu_request& aRequest) { return quick_ask_menu(aRequest); },
							  qt::ask_menu_dispatch{
								  .Ask    = [this](const qt::ask_menu_request& aRequest, const core::quick_ask_row& aRow) { press_quick_ask(aRequest, aRow); },
								  .Custom = [this](const qt::ask_menu_request& aRequest) { ask_custom_question(aRequest); },
								  .Edit   = [this] { Window->open_settings(qt::settings_page::agents_and_context); Window->reveal_prompt_macros(); },
							  });
	}

	auto github_application::implementation::quick_ask_facts(const qt::ask_menu_request& aRequest) const -> core::quick_ask_facts
	{
		auto  facts = core::quick_ask_facts{ };
		auto* tab   = Window != nullptr ? Window->agent_tab() : nullptr;
		if (tab != nullptr) {
			const auto context = tab->suggestion_context_now();
			facts.Base.PullRequest = context.PullRequest;
			facts.Base.Repository  = context.Repository;
			facts.Base.Draft       = context.Draft;
			facts.Base.AuthorIsMe  = context.AuthorIsMe;
			facts.Base.ChecksFailing = context.ChecksFailing;
			facts.Base.ChatEmpty   = tab->turn_count() == 0U;
			facts.Agent            = tab->setup_readiness() != qt::agent_setup_readiness::ready ? core::quick_ask_agent::not_set_up
				: tab->state() == qt::agent_tab_state::working                                    ? core::quick_ask_agent::busy
																									: core::quick_ask_agent::ready;
		}
		if (!aRequest.File.isEmpty()) {
			facts.File = aRequest.File.toStdString();
		}
		if (!aRequest.Thread.isEmpty()) {
			facts.Thread = aRequest.Thread.toStdString();
		}
		facts.Reviewed     = ReturningReview.Earlier.has_value();
		facts.LatestUpload = ReturningReview.Offer.State;
		const auto& snapshot = WindowRead != nullptr ? WindowRead->accepted_snapshot() : std::nullopt;
		if (!snapshot) {
			return facts;
		}
		if (ReturningReview.Earlier) {
			facts.CommitsSinceReview = commits_after(snapshot->Commits, ReturningReview.Earlier->Head).value_or(0U);
		}
		for (const auto& file : snapshot->Files) {
			facts.Files.push_back(file.Path);
		}
		const auto head = std::string{ snapshot->PullRequest.HeadOid.value() };
		for (const auto& thread : snapshot->ReviewThreads) {
			auto entry     = core::quick_ask_thread{ .Identity = thread_identity(thread), .Path = thread.Path };
			entry.TakesReplies = thread.ViewerReplyPermission != application::github_permission_state::denied;
			// Q6: the baseline is the commit the thread started on.
			if (!thread.Comments.empty()) {
				const auto& first    = thread.Comments.front().Observation;
				const auto  baseline = !first.OriginalCommit.empty() ? first.OriginalCommit : first.Commit;
				if (!baseline.empty()) {
					entry.Changed      = baseline != head;
					entry.CommitsSince = commits_after(snapshot->Commits, baseline);
				}
			}
			facts.Threads.push_back(std::move(entry));
		}
		return facts;
	}

	auto github_application::implementation::quick_ask_menu(const qt::ask_menu_request& aRequest) const -> std::optional<core::quick_ask_menu>
	{
		if (Closing || Window == nullptr || Window->agent_tab() == nullptr) {
			return std::nullopt;
		}
		return core::decide_quick_asks(PromptMacroSet.macros(), quick_ask_facts(aRequest), aRequest.Anchor);
	}

	/// A press decides the menu again under the facts it is made under, and
	/// acts only on a row that is still listed and enabled.
	void github_application::implementation::press_quick_ask(const qt::ask_menu_request& aRequest, const core::quick_ask_row& aRow)
	{
		const auto menu = quick_ask_menu(aRequest);
		const auto* row = menu ? core::find_quick_ask(*menu, aRow.Id, aRow.Scope) : nullptr;
		if (row == nullptr || !row->Enabled || Window == nullptr) {
			return;
		}
		auto* const tab = Window->agent_tab();
		switch (row->Action) {
		case core::prompt_action::what_changed: dismiss_asked_conversation(); ask_what_changed(); return;
		case core::prompt_action::explain_file: ask_explain_file(aRequest.File.toStdString()); return;
		case core::prompt_action::was_addressed:
			if (!row->Each.empty()) {
				ask_was_addressed(row->Each.front());
			}
			return;
		case core::prompt_action::none: break;
		}
		const auto id = QString::fromStdString(row->Id);
		if (row->Subject == core::prompt_subject::conversation) {
			// The thread is attached as Ask agent attaches it, and its
			// suggested prompt is sent from the Agent tab.
			if (!row->Each.empty()) {
				ask_agent_about_thread(row->Each.front());
				tab->send_prompt(id);
			}
			return;
		}
		dismiss_asked_conversation();
		Window->open_agent_question();
		tab->send_prompt(id);
	}

	/// Custom question…: the Agent tab on the narrowest subject the menu
	/// names, with nothing sent.
	void github_application::implementation::ask_custom_question(const qt::ask_menu_request& aRequest)
	{
		if (Window == nullptr) {
			return;
		}
		if (aRequest.Anchor != core::quick_ask_anchor::file && !aRequest.Thread.isEmpty()) {
			ask_agent_about_thread(aRequest.Thread.toStdString());
			return;
		}
		dismiss_asked_conversation();
		Window->open_agent_question();
	}

	/// RFC-BPR-0125 milestone 3 ([[BPR-2146]]). The profile's suggested
	/// prompts reach the chips, the tour chats, the Ask menus and Settings.
	void github_application::implementation::present_prompt_macros(const application::prompt_macro_projection& aProjection)
	{
		PromptMacroSet = aProjection.Set;
		if (Window != nullptr) {
			Window->present_prompt_macros(aProjection);
		}
	}
#endif
} // namespace bpr::app
