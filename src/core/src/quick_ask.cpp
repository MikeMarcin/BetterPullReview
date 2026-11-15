#include "bpr/core/quick_ask.hpp"

#include "bpr/core/prompt_surfaces.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto menu_surface(const prompt_surface aSurface) noexcept -> bool
		{
			return aSurface == prompt_surface::pull_request_menu || aSurface == prompt_surface::file_menu || aSurface == prompt_surface::thread_menu;
		}

		/// A review thread on a file: what Was this addressed? asks about.
		[[nodiscard]] auto addressable(const quick_ask_thread& aThread) -> bool { return aThread.Review && !aThread.Path.empty(); }

		/// [[BPR-2179]]. The facts the newer conditions read about a file.
		void add_file_facts(prompt_facts& aFacts, const quick_ask_facts& aFrom, const std::string& aPath)
		{
			aFacts.FileUnresolvedThreads = std::ranges::any_of(aFrom.Threads, [&aPath](const quick_ask_thread& aThread) { return aThread.Path == aPath && aThread.Review && aThread.Resolved == false; });
			aFacts.FileUnreviewed.reset();
			if (aFrom.ReviewedFiles) {
				aFacts.FileUnreviewed = std::ranges::find(*aFrom.ReviewedFiles, aPath) == aFrom.ReviewedFiles->end();
			}
		}

		/// [[BPR-2179]]. The facts the pull request's newer conditions read.
		[[nodiscard]] auto with_review_facts(prompt_facts aFacts, const quick_ask_facts& aFrom) -> prompt_facts
		{
			// [[BPR-2181]] (AC13). Commits counted after the earlier head, or a
			// newer revision the latest upload offers when that head is no
			// longer listed (a force push): either is a push since my review.
			if (!aFacts.PushesSinceReview) {
				aFacts.PushesSinceReview = aFrom.Reviewed && (aFrom.CommitsSinceReview > 0U || aFrom.LatestUpload != latest_upload_state::hidden);
			}
			return aFacts;
		}

		/// The facts a thread's own question is decided against.
		[[nodiscard]] auto thread_facts(const quick_ask_facts& aFacts, const quick_ask_thread& aThread) -> prompt_facts
		{
			auto facts         = with_review_facts(aFacts.Base, aFacts);
			facts.Place        = prompt_place::conversation;
			facts.TakesReplies = aThread.TakesReplies;
			facts.ThreadUnresolved.reset();
			if (aThread.Resolved) {
				facts.ThreadUnresolved = !*aThread.Resolved;
			}
			facts.ThreadReplies    = aThread.Replied;
			facts.ThreadPushes     = aThread.Changed;
			facts.RepliesSinceMine = aThread.RepliesSinceMine;
			if (!aThread.Path.empty()) {
				add_file_facts(facts, aFacts, aThread.Path);
			}
			facts.ConversationFile.reset();
			if (!aThread.Path.empty()) {
				facts.ConversationFile = aThread.Path;
			}
			facts.Lines.reset();
			return facts;
		}

		/// The facts a file's own question is decided against.
		[[nodiscard]] auto file_facts(const quick_ask_facts& aFacts, const std::string& aPath) -> prompt_facts
		{
			auto facts     = with_review_facts(aFacts.Base, aFacts);
			facts.Place    = prompt_place::agent_tab;
			facts.OpenFile = aPath;
			facts.Lines.reset();
			add_file_facts(facts, aFacts, aPath);
			return facts;
		}

		/// The facts the pull request's own question is decided against: the
		/// open file stays, as the Agent tab's chips read it.
		[[nodiscard]] auto pull_request_facts(const quick_ask_facts& aFacts) -> prompt_facts
		{
			auto facts  = with_review_facts(aFacts.Base, aFacts);
			facts.Place = prompt_place::agent_tab;
			facts.OpenFile.reset();
			if (aFacts.File) {
				facts.OpenFile = aFacts.File;
				add_file_facts(facts, aFacts, *aFacts.File);
			}
			facts.Lines.reset();
			return facts;
		}

		/// The threads a scope reaches, in the pull request's order.
		[[nodiscard]] auto threads_at(const quick_ask_facts& aFacts, const prompt_scope aScope) -> std::vector<const quick_ask_thread*>
		{
			auto reached = std::vector<const quick_ask_thread*>{ };
			for (const auto& thread : aFacts.Threads) {
				const auto here = aScope == prompt_scope::pull_request || (aScope == prompt_scope::file && aFacts.File && thread.Path == *aFacts.File)
					|| (aScope == prompt_scope::thread && aFacts.Thread && thread.Identity == *aFacts.Thread);
				if (here) {
					reached.push_back(&thread);
				}
			}
			return reached;
		}

		/// Whether the scope's own subject is there at all.
		[[nodiscard]] auto scope_present(const quick_ask_facts& aFacts, const prompt_scope aScope) -> bool
		{
			switch (aScope) {
			case prompt_scope::thread:       return aFacts.Thread && std::ranges::any_of(aFacts.Threads, [&aFacts](const quick_ask_thread& aThread) { return aThread.Identity == *aFacts.Thread; });
			case prompt_scope::file:         return aFacts.File.has_value();
			case prompt_scope::pull_request: return aFacts.Base.PullRequest.has_value();
			}
			return false;
		}

		/// A wider scope's label: the name with each value the scope holds,
		/// and "each file" or "each thread" for one it asks once for each.
		[[nodiscard]] auto each_label(const prompt_macro& aMacro, const prompt_facts& aFacts, const std::string_view aEach) -> std::string
		{
			auto       label = std::string{ };
			const auto scan  = scan_prompt_template(aMacro.Body.Name, aMacro.Place);
			for (const auto& piece : scan.Pieces) {
				if (!piece.Variable) {
					label += piece.Literal;
					continue;
				}
				const auto alone = prompt_template_scan{ .Pieces = { piece } };
				if (const auto value = expand_prompt_template(alone, aFacts); value && !value->Text.empty()) {
					label += value->Text;
				} else {
					label += aEach;
				}
			}
			return label;
		}

		[[nodiscard]] auto row_of(const prompt_macro& aMacro, const prompt_scope aScope) -> quick_ask_row
		{
			return { .Id = aMacro.Id, .Label = aMacro.Body.Name, .Scope = aScope, .Action = aMacro.Action, .Subject = aMacro.Body.Subject, .Kind = aMacro.Body.Kind };
		}

		[[nodiscard]] auto greyed(quick_ask_row aRow, const quick_ask_reason aReason) -> quick_ask_row
		{
			aRow.Enabled = false;
			aRow.Reason  = aReason;
			return aRow;
		}

		/// What changed since my review, at the pull request.
		[[nodiscard]] auto what_changed(quick_ask_row aRow, const quick_ask_facts& aFacts) -> quick_ask_row
		{
			aRow.Count = aFacts.CommitsSinceReview;
			aRow.Unit  = aFacts.CommitsSinceReview > 0U ? quick_ask_unit::commits : quick_ask_unit::none;
			if (!aFacts.Reviewed) {
				return greyed(std::move(aRow), quick_ask_reason::not_reviewed);
			}
			switch (aFacts.LatestUpload) {
			case latest_upload_state::hidden:      return greyed(std::move(aRow), quick_ask_reason::no_commits_since_review);
			case latest_upload_state::reading:     return greyed(std::move(aRow), quick_ask_reason::reading_commits);
			case latest_upload_state::unavailable: return greyed(std::move(aRow), quick_ask_reason::earlier_unreadable);
			case latest_upload_state::offered:     return aRow;
			}
			return aRow;
		}

		/// Was this addressed?, on one thread or each thread of a scope.
		/// \returns Nothing when the scope holds no thread it asks about.
		[[nodiscard]] auto was_addressed(quick_ask_row aRow, const quick_ask_facts& aFacts) -> std::optional<quick_ask_row>
		{
			auto threads = threads_at(aFacts, aRow.Scope);
			std::erase_if(threads, [](const quick_ask_thread* aThread) { return !addressable(*aThread); });
			if (aRow.Scope == prompt_scope::thread) {
				if (threads.empty()) {
					return std::nullopt;
				}
				const auto& thread = *threads.front();
				aRow.Each          = { thread.Identity };
				if (!thread.Changed) {
					return greyed(std::move(aRow), quick_ask_reason::thread_commit_unknown);
				}
				if (thread.CommitsSince && *thread.CommitsSince > 0U) {
					aRow.Count = *thread.CommitsSince;
					aRow.Unit  = quick_ask_unit::commits;
				}
				return !*thread.Changed ? greyed(std::move(aRow), quick_ask_reason::no_commits_since_thread) : aRow;
			}
			if (threads.empty()) {
				return greyed(std::move(aRow), quick_ask_reason::no_threads);
			}
			for (const auto* const thread : threads) {
				if (thread->Changed.value_or(false)) {
					aRow.Each.push_back(thread->Identity);
				}
			}
			// [[BPR-2184]] (review B1). One ask carries at most
			// `maximum_addressed_threads`, the first in the pull request's
			// order; the row names exactly those, and says how many more.
			if (aRow.Each.size() > maximum_addressed_threads) {
				aRow.Omitted = static_cast<std::uint32_t>(aRow.Each.size() - maximum_addressed_threads);
				aRow.Each.resize(maximum_addressed_threads);
			}
			aRow.Count = static_cast<std::uint32_t>(aRow.Each.empty() ? threads.size() : aRow.Each.size());
			aRow.Unit  = quick_ask_unit::threads;
			return aRow.Each.empty() ? greyed(std::move(aRow), quick_ask_reason::no_commits_since_threads) : aRow;
		}

		/// An ordinary macro at `aRow.Scope`.
		[[nodiscard]] auto ordinary(quick_ask_row aRow, const prompt_macro& aMacro, const quick_ask_facts& aFacts, prompt_withheld& aWithheld) -> std::optional<quick_ask_row>
		{
			const auto own = subject_scope(aMacro.Body.Subject);
			if (!own) {
				return std::nullopt;
			}
			if (*own == aRow.Scope) {
				auto facts = prompt_facts{ };
				if (aRow.Scope == prompt_scope::thread) {
					const auto threads = threads_at(aFacts, prompt_scope::thread);
					if (threads.empty()) {
						return std::nullopt;
					}
					facts     = thread_facts(aFacts, *threads.front());
					aRow.Each = { threads.front()->Identity };
				} else if (aRow.Scope == prompt_scope::file) {
					facts = file_facts(aFacts, *aFacts.File);
				} else {
					facts = pull_request_facts(aFacts);
				}
				auto evaluation = evaluate_prompt_macro(aMacro, facts);
				if (!evaluation.Offer) {
					// RFC-BPR-0128 Q1: an untrue condition of the greys kind
					// lists the row greyed, with that condition's reason.
					if (evaluation.Unmet == prompt_unmet::condition) {
						if (const auto decided = decide_prompt_conditions(aMacro.Body, facts); !decided.Hides) {
							aRow.Label     = each_label(aMacro, facts, "");
							aRow.Condition = decided.Reason;
							return greyed(std::move(aRow), quick_ask_reason::condition);
						}
					}
					aWithheld = { .Id = aMacro.Id, .Unmet = evaluation.Unmet, .Condition = evaluation.Condition };
					return std::nullopt;
				}
				aRow.Label = std::move(evaluation.Offer->Label);
				return aRow;
			}
			// A wider scope asks once for each thread, or each file.
			auto label = std::optional<std::string>{ };
			auto seen  = std::uint32_t{ 0 };
			// Review N5: whether every one it was decided for failed on its
			// conditions alone, and the first such failure.
			auto       onlyConditions = true;
			auto       firstUnmet     = std::optional<prompt_evaluation>{ };
			const auto note           = [&onlyConditions, &firstUnmet](const prompt_evaluation& aEvaluation) {
				onlyConditions = onlyConditions && aEvaluation.Unmet == prompt_unmet::condition;
				if (!firstUnmet) {
					firstUnmet = aEvaluation;
				}
			};
			if (*own == prompt_scope::thread) {
				for (const auto* const thread : threads_at(aFacts, aRow.Scope)) {
					if (thread->Path.empty() && aRow.Scope == prompt_scope::file) {
						continue;
					}
					++seen;
					if (auto evaluation = evaluate_prompt_macro(aMacro, thread_facts(aFacts, *thread)); evaluation.Offer) {
						aRow.Each.push_back(thread->Identity);
						if (!label) {
							label = std::move(evaluation.Offer->Label);
						}
					} else {
						note(evaluation);
					}
				}
				aRow.Unit = quick_ask_unit::threads;
			} else {
				for (const auto& path : aFacts.Files) {
					++seen;
					if (auto evaluation = evaluate_prompt_macro(aMacro, file_facts(aFacts, path)); evaluation.Offer) {
						aRow.Each.push_back(path);
						if (!label) {
							label = std::move(evaluation.Offer->Label);
						}
					} else {
						note(evaluation);
					}
				}
				aRow.Unit = quick_ask_unit::files;
			}
			if (label && aRow.Each.size() == 1U) {
				aRow.Label = std::move(*label);
			} else {
				auto facts = aRow.Scope == prompt_scope::file ? file_facts(aFacts, *aFacts.File) : pull_request_facts(aFacts);
				if (*own == prompt_scope::file) {
					facts.OpenFile.reset();
				}
				facts.Place = aMacro.Place;
				aRow.Label  = each_label(aMacro, facts, *own == prompt_scope::thread ? "each thread" : "each file");
			}
			aRow.Count = static_cast<std::uint32_t>(aRow.Each.empty() ? seen : aRow.Each.size());
			if (seen == 0U) {
				return greyed(std::move(aRow), *own == prompt_scope::thread ? quick_ask_reason::no_threads : quick_ask_reason::no_files_match);
			}
			// RFC-BPR-0128 Q1, review N5: a prompt that hides while its
			// conditions are unmet hides here too, when its conditions are
			// all that held it back from every one.
			if (aRow.Each.empty() && onlyConditions && firstUnmet && aMacro.Body.WhenUnmet == prompt_when_unmet::hide) {
				aWithheld = { .Id = aMacro.Id, .Unmet = firstUnmet->Unmet, .Condition = firstUnmet->Condition };
				return std::nullopt;
			}
			if (aRow.Each.empty()) {
				return greyed(std::move(aRow), *own == prompt_scope::thread ? quick_ask_reason::no_threads_match : quick_ask_reason::no_files_match);
			}
			return aRow;
		}
	} // namespace

	auto stable_name(const quick_ask_anchor aAnchor) noexcept -> std::string_view
	{
		switch (aAnchor) {
		case quick_ask_anchor::review_bar: return "review-bar";
		case quick_ask_anchor::file:       return "file";
		case quick_ask_anchor::thread:     return "thread";
		}
		return "unknown";
	}

	auto quick_ask_scopes(const quick_ask_anchor aAnchor) -> std::vector<prompt_scope>
	{
		switch (aAnchor) {
		case quick_ask_anchor::review_bar: return { prompt_scope::pull_request, prompt_scope::file, prompt_scope::thread };
		case quick_ask_anchor::file:       return { prompt_scope::file };
		case quick_ask_anchor::thread:     return { prompt_scope::thread };
		}
		return { };
	}

	auto stable_name(const quick_ask_reason aReason) noexcept -> std::string_view
	{
		switch (aReason) {
		case quick_ask_reason::none:                     return "none";
		case quick_ask_reason::agent_not_set_up:         return "agent-not-set-up";
		case quick_ask_reason::agent_busy:               return "agent-busy";
		case quick_ask_reason::not_reviewed:             return "not-reviewed";
		case quick_ask_reason::no_commits_since_review:  return "no-commits-since-review";
		case quick_ask_reason::reading_commits:          return "reading-commits";
		case quick_ask_reason::earlier_unreadable:       return "earlier-unreadable";
		case quick_ask_reason::no_threads:               return "no-threads";
		case quick_ask_reason::no_commits_since_threads: return "no-commits-since-threads";
		case quick_ask_reason::no_commits_since_thread:  return "no-commits-since-thread";
		case quick_ask_reason::thread_commit_unknown:    return "thread-commit-unknown";
		case quick_ask_reason::no_threads_match:         return "no-threads-match";
		case quick_ask_reason::no_files_match:           return "no-files-match";
		case quick_ask_reason::condition:                return "condition";
		}
		return "unknown";
	}

	auto decide_quick_asks(const std::span<const prompt_macro> aOrdered, const quick_ask_facts& aFacts, const quick_ask_anchor aAnchor) -> quick_ask_menu
	{
		auto menu = quick_ask_menu{ };
		for (const auto scope : quick_ask_scopes(aAnchor)) {
			if (!scope_present(aFacts, scope)) {
				continue;
			}
			auto actions = std::vector<quick_ask_row>{ };
			auto others  = std::vector<quick_ask_row>{ };
			for (const auto& macro : aOrdered) {
				if (!macro.Body.QuickAsk || !macro.Body.Scopes.has(scope) || !prompt_scope_allowed(macro.Place, macro.Body.Subject, scope)
					|| (macro.Surface && !menu_surface(*macro.Surface))) {
					continue;
				}
				if (!macro.On) {
					menu.Hidden.push_back({ .Id = macro.Id, .Unmet = prompt_unmet::off });
					continue;
				}
				if (!prompt_macro_valid(macro)) {
					menu.Hidden.push_back({ .Id = macro.Id, .Unmet = prompt_unmet::invalid });
					continue;
				}
				auto row      = row_of(macro, scope);
				auto decided  = std::optional<quick_ask_row>{ };
				auto withheld = prompt_withheld{ .Id = macro.Id, .Unmet = prompt_unmet::condition };
				switch (macro.Action) {
				case prompt_action::what_changed:  decided = what_changed(std::move(row), aFacts); break;
				case prompt_action::was_addressed: decided = was_addressed(std::move(row), aFacts); break;
				case prompt_action::explain_file:
					if (aFacts.File) {
						decided = std::move(row);
					} else {
						withheld.Unmet = prompt_unmet::needs_file;
					}
					break;
				case prompt_action::none: decided = ordinary(std::move(row), macro, aFacts, withheld); break;
				}
				if (!decided) {
					menu.Hidden.push_back(std::move(withheld));
					continue;
				}
				// The agent's own state greys nothing: a press queues it, and the
				// row says what it will wait for.
				if (decided->Enabled && aFacts.Agent != quick_ask_agent::ready) {
					decided->Waits = aFacts.Agent == quick_ask_agent::busy ? quick_ask_reason::agent_busy : quick_ask_reason::agent_not_set_up;
				}
				// [[BPR-2179]]. A placed copy keeps its surface's order; the
				// library's own fields put the action rows first.
				(macro.Action != prompt_action::none && !macro.Surface ? actions : others).push_back(std::move(*decided));
			}
			if (actions.empty() && others.empty()) {
				continue;
			}
			auto section = quick_ask_section{ .Scope = scope, .Rows = std::move(actions) };
			section.Rows.insert(section.Rows.end(), std::make_move_iterator(others.begin()), std::make_move_iterator(others.end()));
			menu.Sections.push_back(std::move(section));
		}
		return menu;
	}

	auto find_quick_ask(const quick_ask_menu& aMenu, const std::string_view aId, const prompt_scope aScope) -> const quick_ask_row*
	{
		for (const auto& section : aMenu.Sections) {
			if (section.Scope != aScope) {
				continue;
			}
			const auto found = std::ranges::find(section.Rows, aId, &quick_ask_row::Id);
			if (found != section.Rows.end()) {
				return &*found;
			}
		}
		return nullptr;
	}

	auto chat_prompt_facts(const quick_ask_facts& aFacts, const prompt_chat_view& aView) -> prompt_facts
	{
		auto facts = with_review_facts(aFacts.Base, aFacts);
		if (aView.Thread) {
			if (const auto found = std::ranges::find(aFacts.Threads, *aView.Thread, &quick_ask_thread::Identity); found != aFacts.Threads.end()) {
				facts = thread_facts(aFacts, *found);
			}
		}
		facts.Place     = aView.Place;
		facts.ChatEmpty = aView.ChatEmpty;
		facts.OpenFile  = aView.OpenFile;
		facts.Lines     = aView.Lines;
		if (aView.OpenFile && !aView.Thread) {
			add_file_facts(facts, aFacts, *aView.OpenFile);
		}
		if (aView.TakesReplies) {
			facts.TakesReplies = aView.TakesReplies;
		}
		facts.Thread    = aView.TourPart;
		facts.Selection = aView.Selection;
		return facts;
	}

	auto decide_chat_prompts(const std::span<const prompt_macro> aPlaced, const prompt_facts& aFacts) -> chat_prompts
	{
		auto offers       = decide_prompt_offers(aPlaced, aFacts);
		auto decided      = chat_prompts{ .Chips = std::move(offers.Chips), .More = std::move(offers.More), .Pressable = { } };
		decided.Pressable = decided.Chips;
		decided.Pressable.insert(decided.Pressable.end(), decided.More.begin(), decided.More.end());
		for (auto& offer : menu_prompt_offers(aPlaced, aFacts)) {
			if (std::ranges::find(decided.Pressable, offer.Id, &prompt_offer::Id) == decided.Pressable.end()) {
				decided.Pressable.push_back(std::move(offer));
			}
		}
		return decided;
	}

	auto decide_quick_ask_prompt(const prompt_macro& aMacro, const quick_ask_facts& aFacts, const std::string_view aItem) -> std::optional<prompt_offer>
	{
		if (aMacro.Action != prompt_action::none) {
			return std::nullopt;
		}
		auto       facts = pull_request_facts(aFacts);
		const auto own   = subject_scope(aMacro.Body.Subject);
		if (own == prompt_scope::thread) {
			const auto identity = !aItem.empty() ? std::string{ aItem } : aFacts.Thread.value_or(std::string{ });
			const auto found    = std::ranges::find(aFacts.Threads, identity, &quick_ask_thread::Identity);
			if (found == aFacts.Threads.end()) {
				return std::nullopt;
			}
			facts = thread_facts(aFacts, *found);
		} else if (own == prompt_scope::file) {
			if (!aItem.empty()) {
				facts = file_facts(aFacts, std::string{ aItem });
			} else if (aFacts.File) {
				facts = file_facts(aFacts, *aFacts.File);
			} else {
				return std::nullopt;
			}
		}
		return evaluate_prompt_macro(aMacro, facts).Offer;
	}
} // namespace bpr::core
