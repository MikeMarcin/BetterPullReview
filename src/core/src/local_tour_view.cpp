#include "bpr/core/local_tour_view.hpp"

#include "bpr/core/sha256.hpp"
#include "bpr/core/tour_stop_acceptance.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	namespace {
		constexpr auto view_domain = std::string_view{ "BPR-local-tour-view-v1" };

		/// SHA-256 of the domain, a role and length-prefixed parts.
		[[nodiscard]] auto digest(const std::string_view aRole, std::initializer_list<std::string_view> aParts) -> std::array<std::uint8_t, 32>
		{
			auto       bytes  = std::vector<std::uint8_t>(view_domain.begin(), view_domain.end());
			const auto append = [&bytes](const std::string_view aText) {
				for (std::size_t shift = 0U; shift < 64U; shift += 8U) {
					bytes.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(aText.size()) >> shift) & 0xFFU));
				}
				bytes.insert(bytes.end(), aText.begin(), aText.end());
			};
			bytes.push_back(0U);
			append(aRole);
			for (const auto part : aParts) {
				append(part);
			}
			return sha256(bytes);
		}

		[[nodiscard]] auto nonzero(const std::array<std::uint8_t, 32>& aDigest, const std::size_t aOffset) -> std::uint64_t
		{
			auto value = std::uint64_t{ 0U };
			for (std::size_t index = 0U; index < 8U; ++index) {
				value = (value << 8U) | aDigest[(aOffset + index) % aDigest.size()];
			}
			value &= 0x7FFF'FFFF'FFFF'FFFFULL;
			return value == 0U ? 1U : value;
		}

		/// A binding's lines on its candidate, as the panel's zero-based destination.
		[[nodiscard]] auto destination(std::string_view aDraft, const std::uint64_t aCandidate, const std::string& aPath, const diff_side aSide, const std::uint32_t aFirst,
									   const std::uint32_t aLast) -> tour_code_destination
		{
			return {
				.Commit    = local_tour_commit(aDraft, aCandidate),
				.Path      = aPath,
				.Side      = aSide,
				.FirstLine = aFirst == 0U ? 0U : aFirst - 1U,
				.LastLine  = aLast == 0U ? 0U : aLast - 1U,
			};
		}

		[[nodiscard]] auto project_stop(std::string_view aDraft, const local_tour_stop& aStop, const git_commit_oid& aHead, const std::uint64_t aLatest) -> tour_stop
		{
			auto       stop  = tour_stop{ .Id = tour_stop_id{ aStop.Id }, .Title = aStop.Title, .Body = aStop.Text };
			const auto exact = aStop.Current.Correspondence == local_correspondence::exact && aStop.Current.Candidate == aLatest;
			// The stop's lines: where they are on the latest candidate when
			// they are exactly there, else where the author bound them.
			stop.Destination = exact ? destination(aDraft, aLatest, aStop.Source.Path, aStop.Source.Side, aStop.Current.FirstLine, aStop.Current.LastLine)
									 : destination(aDraft, aStop.Source.Candidate, aStop.Source.Path, aStop.Source.Side, aStop.Source.FirstLine, aStop.Source.LastLine);
			// The same lines as a range, so the route says them whole.
			const auto& lines      = *stop.Destination;
			const auto  comparison = file_comparison_digest{ digest("comparison", { aDraft, lines.Path }) };
			stop.Original          = tour_source_range{ .First = { comparison, lines.Side, lines.FirstLine }, .Last = { comparison, lines.Side, lines.LastLine }, .Commit = lines.Commit, .Path = lines.Path };
			stop.Mapping           = exact ? tour_mapping::exact_original : tour_mapping::changed_material;
			stop.Preparation       = tour_preparation::editing;
			const auto decision    = decide_local_stop(aStop);
			if (decision.State == tour_acceptance_state::not_accepted) {
				return stop;
			}
			// The record the panel decides from, written to reach the local
			// decision: code at the head holds; code checked changed there
			// does not.
			auto acceptance = tour_stop_acceptance{ .Title = stop.Title, .Body = stop.Body, .Destination = stop.Destination, .Head = aHead };
			if (decision.State == tour_acceptance_state::accepted) {
				acceptance.Material.push_back({ .Range = *stop.Destination });
			} else {
				auto range      = *stop.Destination;
				range.Commit    = range.Commit == aHead ? local_tour_commit(aDraft, 0U) : range.Commit;
				const auto gone = decision.Reason == local_stop_reason::missing || decision.Reason == local_stop_reason::unavailable;
				acceptance.Head = range.Commit;
				acceptance.Material.push_back({ .Range = range, .CheckedAt = aHead, .Verdict = gone ? tour_material_verdict::missing : tour_material_verdict::changed });
			}
			stop.Acceptance = std::move(acceptance);
			return stop;
		}

		[[nodiscard]] auto stop_of(const std::string_view aThread) -> std::optional<std::uint64_t>
		{
			constexpr auto prefix = std::string_view{ "stop:" };
			if (!aThread.starts_with(prefix)) {
				return std::nullopt;
			}
			auto       value        = std::uint64_t{ 0U };
			const auto digits       = aThread.substr(prefix.size());
			const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
			if (error != std::errc{ } || end != digits.data() + digits.size() || value == 0U) {
				return std::nullopt;
			}
			return value;
		}

		[[nodiscard]] auto cost_of(const std::optional<std::uint64_t>& aMicros) -> std::optional<agent_cost>
		{
			if (!aMicros || *aMicros > maximum_agent_cost / 1000U) {
				return std::nullopt;
			}
			return *aMicros * 1000U;
		}
	}

	auto local_tour_commit(const std::string_view aDraft, const std::uint64_t aCandidate) -> git_commit_oid
	{
		auto       bytes   = std::array<std::uint8_t, 32>{ };
		const auto ordinal = std::to_string(aCandidate);
		const auto value   = digest("candidate", { aDraft, ordinal });
		std::copy_n(value.begin(), 20U, bytes.begin());
		bytes[0] = bytes[0] == 0U ? 1U : bytes[0];
		return git_commit_oid{ git_hash_algorithm::sha1, bytes };
	}

	auto local_tour_owner(const std::string_view aDraft) -> tour_owner
	{
		const auto subject = digest("owner", { aDraft });
		return {
			.Profile       = bpr_profile_id{ nonzero(subject, 0U) },
			.Host          = git_host_id{ nonzero(subject, 8U) },
			.HostAuthority = host_authority_fingerprint{ digest("host", { aDraft }) },
			.Account       = github_account_id{ nonzero(subject, 16U) },
			.Repository    = repository_id{ nonzero(subject, 4U) },
			.PullRequest   = pull_request_id{ nonzero(subject, 12U) },
		};
	}

	auto local_tour_thread(const std::string_view aThread) -> tour_thread
	{
		if (const auto stop = stop_of(aThread)) {
			return { .Kind = tour_thread_kind::stop, .Stop = tour_stop_id{ *stop } };
		}
		if (aThread == "problem") {
			return { .Kind = tour_thread_kind::problem };
		}
		if (aThread == "solution") {
			return { .Kind = tour_thread_kind::solution };
		}
		return { .Kind = tour_thread_kind::tour };
	}

	auto local_thread_of(const tour_thread& aThread) -> std::string
	{
		switch (aThread.Kind) {
		case tour_thread_kind::stop:     return local_stop_thread(aThread.Stop.value());
		case tour_thread_kind::problem:  return "problem";
		case tour_thread_kind::solution: return "solution";
		case tour_thread_kind::tour:     break;
		}
		return std::string{ local_change_thread };
	}

	auto local_subject_of(const tour_thread& aThread) -> local_subject
	{
		switch (aThread.Kind) {
		case tour_thread_kind::stop:     return local_subject::stop;
		case tour_thread_kind::problem:  return local_subject::problem;
		case tour_thread_kind::solution: return local_subject::solution;
		case tour_thread_kind::tour:     break;
		}
		return local_subject::whole_change;
	}

	auto view_local_tour(const local_draft& aDraft, const std::optional<local_tour_asking>& aAsking) -> local_tour_view
	{
		auto        view    = local_tour_view{ };
		const auto  id      = std::string_view{ aDraft.Id.Value };
		const auto* latest  = latest_candidate(aDraft);
		const auto  ordinal = latest != nullptr ? latest->Ordinal : 0U;
		const auto  head    = local_tour_commit(id, ordinal);
		const auto& local   = aDraft.Preparation.Tour;
		auto&       tour    = view.Tour;
		tour.Id             = tour_draft_id{ 1U };
		tour.Owner          = local_tour_owner(id);
		tour.Base           = local_tour_commit(id, std::numeric_limits<std::uint64_t>::max());
		tour.Head           = head;
		tour.Problem        = local.Problem;
		tour.Solution       = local.Solution;
		tour.Revision       = std::max<std::uint64_t>(aDraft.Version, 1U);
		tour.LastStopId     = local.NextStop == 0U ? 0U : local.NextStop - 1U;
		for (const auto& stop : local.Stops) {
			tour.Stops.push_back(project_stop(id, stop, head, ordinal));
			if (const auto decision = decide_local_stop(stop); decision.State != tour_acceptance_state::accepted) {
				view.Reasons.emplace_back(stop.Id, decision.Reason);
			}
		}
		if (latest != nullptr) {
			for (const auto& change : latest->Changes) {
				view.Files.push_back(change.Path);
			}
		}

		// The conversations, turn by turn, as the side chat reads runs: an
		// author's question and the answer that followed it. Earlier
		// candidates' conversations stay readable before the open ones.
		auto runs    = std::vector<tour_agent_run>{ };
		auto runOf   = std::map<std::uint64_t, tour_agent_run_id>{ };
		auto ordered = std::vector<const local_conversation*>{ };
		for (const auto& conversation : aDraft.Preparation.Conversations) {
			ordered.push_back(&conversation);
		}
		std::ranges::stable_sort(ordered, [](const local_conversation* aLeft, const local_conversation* aRight) { return aLeft->Ended && !aRight->Ended; });
		for (const auto* conversation : ordered) {
			auto       total = agent_cost{ 0U };
			auto       known = false;
			const auto scope = local_session_scope(id, conversation->Candidate, conversation->Thread, conversation->Agent);
			for (std::size_t index = 0U; index < conversation->Turns.size(); ++index) {
				const auto& turn = conversation->Turns[index];
				if (!turn.Author) {
					continue;
				}
				auto run = tour_agent_run{
					.Id                = tour_agent_run_id{ conversation->Id * 1024U + index + 1U },
					.ConversationScope = scope,
					.Conversation      = conversation->AgentName.empty() ? conversation->Agent : conversation->AgentName,
					.Request           = tour_agent_request::ask,
					.Tools             = tour_agent_tools::none,
					.State             = tour_agent_run_state::failed,
					.Question          = turn.Text,
					.Thread            = local_tour_thread(conversation->Thread),
				};
				if (index + 1U < conversation->Turns.size() && !conversation->Turns[index + 1U].Author) {
					const auto& answer = conversation->Turns[index + 1U];
					run.State          = tour_agent_run_state::answered;
					run.Reply          = answer.Text;
					if (const auto cost = cost_of(answer.CostMicros)) {
						total += *cost;
						known    = true;
						run.Cost = cost;
					}
					if (known) {
						run.SessionCost = total;
					}
					for (const auto proposal : answer.Proposals) {
						runOf[proposal] = run.Id;
					}
				}
				runs.push_back(std::move(run));
			}
		}
		if (aAsking) {
			runs.push_back({
				.Id       = tour_agent_run_id{ std::numeric_limits<std::uint32_t>::max() },
				.Request  = tour_agent_request::ask,
				.State    = tour_agent_run_state::running,
				.Question = aAsking->Text,
				.Thread   = local_tour_thread(aAsking->Thread),
			});
		}
		if (runs.size() > maximum_tour_agent_runs) {
			runs.erase(runs.begin(), runs.end() - static_cast<std::ptrdiff_t>(maximum_tour_agent_runs));
		}
		view.Session.Runs    = std::move(runs);
		view.Session.LastRun = view.Session.Runs.empty() ? 0U : view.Session.Runs.back().Id.value();

		// Proposals on the tour's parts. A stale one stays readable, refused.
		auto proposal = tour_proposal{ };
		for (const auto& entry : aDraft.Preparation.Proposals) {
			if (entry.Target == local_proposal_target::title || entry.Target == local_proposal_target::description) {
				continue;
			}
			if (entry.State == local_proposal_state::accepted || entry.State == local_proposal_state::rejected) {
				continue;
			}
			const auto found = runOf.find(entry.Id);
			auto       item  = tour_proposal_item{ .Run = found == runOf.end() ? tour_agent_run_id{ } : found->second };
			switch (entry.Target) {
			case local_proposal_target::problem:  item.Part = tour_proposal_part::problem; break;
			case local_proposal_target::solution: item.Part = tour_proposal_part::solution; break;
			case local_proposal_target::stop:
				item.Part       = tour_proposal_part::stop_text;
				item.StopId     = tour_stop_id{ entry.Stop };
				item.Stop.Title = entry.Title;
				break;
			default:
				item.Part = tour_proposal_part::stop;
				if (entry.Source) {
					item.Stop = { .Path = entry.Source->Path, .FirstLine = entry.Source->FirstLine, .LastLine = entry.Source->LastLine };
				}
				item.Stop.Title       = entry.Title;
				item.Stop.Explanation = entry.Text;
				break;
			}
			item.Text = entry.Text;
			if (entry.State == local_proposal_state::outside_candidate) {
				proposal.LeftOut.push_back({ .Stop = item.Stop, .Verdict = tour_anchor_verdict::not_in_pull_request });
				continue;
			}
			if (proposal_stale(aDraft, entry)) {
				item.Decision = tour_proposal_decision::discarded;
				item.Refusal  = "stale";
			}
			proposal.Run = item.Run;
			proposal.Items.push_back(std::move(item));
			view.ProposalItems.push_back(entry.Id);
		}
		if (!proposal.Items.empty() || !proposal.LeftOut.empty()) {
			view.Proposal = std::move(proposal);
		}
		return view;
	}

	auto local_tour_carry_refusal_words(const tour_refusal aRefusal) -> std::string
	{
		const auto why = [aRefusal]() -> std::string_view {
			switch (aRefusal) {
			case tour_refusal::none:            return { };
			case tour_refusal::identity:        return "it belongs to another pull request or account";
			case tour_refusal::conflict:        return "the pull request's tour changed meanwhile";
			case tour_refusal::invalid_source:  return "a stop's lines could not be placed in it";
			case tour_refusal::invalid_command: return "the tour could not be read";
			case tour_refusal::bounds:          return "it is larger than a tour may be";
			case tour_refusal::incomplete:      return "a stop is missing its title, explanation or lines";
			case tour_refusal::exhausted:       return "the tour has no room for more stops";
			}
			return "it was refused";
		}();
		if (why.empty()) {
			return { };
		}
		return "The local draft's tour could not start this pull request's tour: " + std::string{ why } + ". The local draft keeps its tour.";
	}

	auto carry_local_tour_to_pull_request(const local_draft& aDraft, const local_publication_result& aResult, const review_tour& aTarget) -> local_tour_carry
	{
		auto        carry = local_tour_carry{ .Tour = aTarget, .Mapping = map_local_publication(aDraft, aResult) };
		auto&       tour  = carry.Tour;
		const auto& local = aDraft.Preparation.Tour;
		tour.Problem      = local.Problem;
		tour.Solution     = local.Solution;
		tour.Stops.clear();
		auto files = std::vector<std::string>{ };
		for (const auto& file : aResult.Files) {
			files.push_back(file.Path);
		}
		for (std::size_t index = 0U; index < local.Stops.size() && index < maximum_tour_stops; ++index) {
			const auto& from    = local.Stops[index];
			const auto& mapping = carry.Mapping.Stops[index];
			auto        stop    = tour_stop{ .Id = tour_stop_id{ index + 1U }, .Title = from.Title, .Body = from.Text, .Preparation = tour_preparation::editing };
			if (mapping.Anchor) {
				// A stop at a destination with no local source range, as an
				// agent's proposed stop is: `exact_original` would claim an
				// `Original` range it does not have, which `apply` refuses.
				stop.Destination    = mapping.Anchor;
				stop.Mapping        = tour_mapping::unavailable;
				stop.ReaffirmedHead = mapping.Anchor->Commit;
				if (decide_local_stop(from).State == tour_acceptance_state::accepted) {
					stop.Acceptance = accept_tour_stop(tour, stop, files);
				}
			}
			tour.Stops.push_back(std::move(stop));
		}
		tour.LastStopId = tour.Stops.size();
		return carry;
	}

	auto translate_local_tour_command(const local_draft& aDraft, const tour_command& aCommand) -> std::vector<local_tour_edit>
	{
		const auto& tour  = aDraft.Preparation.Tour;
		auto        edits = std::vector<local_tour_edit>{ };
		const auto  stops = aCommand.Selection.empty() ? std::vector<tour_stop_id>{ aCommand.Stop } : aCommand.Selection;
		switch (aCommand.Kind) {
		case tour_command_kind::problem:  edits.push_back({ .Kind = local_tour_edit_kind::brief, .Problem = aCommand.Text, .Solution = tour.Solution }); break;
		case tour_command_kind::solution: edits.push_back({ .Kind = local_tour_edit_kind::brief, .Problem = tour.Problem, .Solution = aCommand.Text }); break;
		case tour_command_kind::edit_stop:
			edits.push_back({ .Kind = local_tour_edit_kind::stop_text, .Stop = aCommand.Stop.value(), .Title = aCommand.Value.Title, .Text = aCommand.Value.Body });
			break;
		case tour_command_kind::accept:
		case tour_command_kind::complete: edits.push_back({ .Kind = local_tour_edit_kind::accept, .Stop = aCommand.Stop.value() }); break;
		case tour_command_kind::unaccept: edits.push_back({ .Kind = local_tour_edit_kind::unaccept, .Stop = aCommand.Stop.value() }); break;
		case tour_command_kind::remove:
			for (const auto stop : stops) {
				edits.push_back({ .Kind = local_tour_edit_kind::remove, .Stop = stop.value() });
			}
			break;
		case tour_command_kind::move:
			if (stops.size() == 1U) {
				edits.push_back({ .Kind = local_tour_edit_kind::move, .Stop = stops.front().value(), .Index = aCommand.Position });
			}
			break;
		default: break;
		}
		std::erase_if(edits, [&tour](const local_tour_edit& aEdit) {
			return aEdit.Kind != local_tour_edit_kind::brief && std::ranges::find(tour.Stops, aEdit.Stop, &local_tour_stop::Id) == tour.Stops.end();
		});
		return edits;
	}
}
