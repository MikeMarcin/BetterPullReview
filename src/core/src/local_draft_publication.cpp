#include "bpr/core/local_draft_publication.hpp"

#include "bpr/core/local_draft.hpp"
#include "bpr/core/local_draft_evidence.hpp"
#include "bpr/core/local_draft_tour.hpp"
#include "bpr/core/review_cycle_publication.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/core/utf8.hpp"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <array>
#include <ranges>
#include <utility>

namespace bpr::core::detail {
	struct wire_publication_plan
	{
		std::string                Draft;
		std::uint64_t              Candidate = 0;
		std::string                Head;
		std::string                Base;
		std::string                TargetTip;
		std::optional<std::string> ObservedTargetTip;
		std::string                Host;
		std::uint64_t              Account = 0;
		std::string                Login;
		std::string                Owner;
		std::string                Repository;
		std::string                BaseBranch;
		std::string                HeadBranch;
		std::string                Title;
		std::string                Body;
		bool                       PullDraft = true;
		std::vector<std::uint64_t> Evidence;
		std::vector<std::uint64_t> Stale;
		std::uint64_t              PrivateRecords       = 0;
		std::uint64_t              PrivateNotes         = 0;
		std::uint64_t              PrivateConversations = 0;
		bool                       TourFollowUp         = false;
		std::vector<std::uint64_t> Unaccepted;
		std::uint64_t              Stops = 0;
	};
	struct wire_publication_link
	{
		std::uint64_t Number = 0;
		std::string   Url;
		std::string   NodeId;
		std::string   Owner;
		std::string   Repository;
		std::string   HeadBranch;
		std::string   Head;
		std::string   BaseBranch;
		std::string   Author;
		bool          Draft   = false;
		bool          Drifted = false;
		bool          Adopted = false;
	};
	struct wire_follow_up
	{
		std::string Kind;
		std::string State;
		std::string Reason;
	};
	struct wire_publication
	{
		wire_publication_plan                Plan;
		std::string                          Digest;
		std::string                          Phase;
		std::uint64_t                        Generation     = 0;
		std::uint64_t                        NextGeneration = 1;
		std::string                          Create;
		std::string                          Reason;
		std::optional<wire_publication_link> Link;
		std::vector<wire_follow_up>          FollowUps;
	};
} // namespace bpr::core::detail

template <>
struct glz::meta<bpr::core::detail::wire_publication_plan>
{
	using value_type            = bpr::core::detail::wire_publication_plan;
	static constexpr auto value = object("draft", &value_type::Draft, "candidate", &value_type::Candidate, "head", &value_type::Head, "base", &value_type::Base, "target_tip", &value_type::TargetTip,
										 "observed_target_tip", &value_type::ObservedTargetTip, "host", &value_type::Host, "account", &value_type::Account, "login", &value_type::Login, "owner",
										 &value_type::Owner, "repository", &value_type::Repository, "base_branch", &value_type::BaseBranch, "head_branch", &value_type::HeadBranch, "title",
										 &value_type::Title, "body", &value_type::Body, "draft_pull_request", &value_type::PullDraft, "evidence", &value_type::Evidence, "stale", &value_type::Stale,
										 "private_records", &value_type::PrivateRecords, "private_notes", &value_type::PrivateNotes, "private_conversations", &value_type::PrivateConversations,
										 "tour_follow_up", &value_type::TourFollowUp, "unaccepted", &value_type::Unaccepted, "stops", &value_type::Stops);
};
template <>
struct glz::meta<bpr::core::detail::wire_publication_link>
{
	using value_type            = bpr::core::detail::wire_publication_link;
	static constexpr auto value = object("number", &value_type::Number, "url", &value_type::Url, "node", &value_type::NodeId, "owner", &value_type::Owner, "repository", &value_type::Repository,
										 "head_branch", &value_type::HeadBranch, "head", &value_type::Head, "base_branch", &value_type::BaseBranch, "author", &value_type::Author, "draft",
										 &value_type::Draft, "drifted", &value_type::Drifted, "adopted", &value_type::Adopted);
};
template <>
struct glz::meta<bpr::core::detail::wire_follow_up>
{
	using value_type            = bpr::core::detail::wire_follow_up;
	static constexpr auto value = object("kind", &value_type::Kind, "state", &value_type::State, "reason", &value_type::Reason);
};
template <>
struct glz::meta<bpr::core::detail::wire_publication>
{
	using value_type            = bpr::core::detail::wire_publication;
	static constexpr auto value = object("plan", &value_type::Plan, "digest", &value_type::Digest, "phase", &value_type::Phase, "generation", &value_type::Generation, "next_generation",
										 &value_type::NextGeneration, "create", &value_type::Create, "reason", &value_type::Reason, "link", &value_type::Link, "follow_ups", &value_type::FollowUps);
};

namespace bpr::core {
	namespace {
		constexpr auto read_options        = glz::opts{ .error_on_unknown_keys = true };
		constexpr auto maximum_reason      = std::size_t{ 1024U };
		constexpr auto maximum_url_bytes   = std::size_t{ 2048U };
		constexpr auto maximum_title_bytes = std::size_t{ 256U };

		constexpr auto all_states   = std::array{ local_step_state::waiting, local_step_state::sending, local_step_state::sent, local_step_state::failed, local_step_state::not_sent };
		constexpr auto all_phases   = std::array{ local_publication_phase::prepared, local_publication_phase::authorized, local_publication_phase::creating, local_publication_phase::linked,
												  local_publication_phase::uncertain, local_publication_phase::refused, local_publication_phase::cancelled };
		constexpr auto all_followup = std::array{ local_follow_up::association, local_follow_up::tour };

		template <typename E, std::size_t N>
		[[nodiscard]] auto enum_from(const std::string_view aText, const std::array<E, N>& aAll) -> std::optional<E>
		{
			for (const auto value : aAll) {
				if (name(value) == aText) {
					return value;
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto hex(const git_commit_oid& aOid) -> std::string { return std::string{ canonical_hex(aOid).view() }; }

		[[nodiscard]] auto hex(const std::array<std::uint8_t, 32>& aBytes) -> std::string
		{
			constexpr auto digits = std::string_view{ "0123456789abcdef" };
			auto           out    = std::string{ };
			out.reserve(64U);
			for (const auto byte : aBytes) {
				out += digits[byte >> 4U];
				out += digits[byte & 0x0FU];
			}
			return out;
		}

		[[nodiscard]] auto folded_equal(const std::string_view aLeft, const std::string_view aRight) noexcept -> bool
		{
			const auto fold = [](const char aByte) { return aByte >= 'A' && aByte <= 'Z' ? static_cast<char>(aByte - 'A' + 'a') : aByte; };
			return aLeft.size() == aRight.size() && std::ranges::equal(aLeft, aRight, [&fold](const char aA, const char aB) { return fold(aA) == fold(aB); });
		}

		[[nodiscard]] auto short_hex(const git_commit_oid& aOid) -> std::string { return hex(aOid).substr(0U, 12U); }

		[[nodiscard]] auto stale(const local_draft& aDraft, const std::uint64_t aExpected) noexcept -> bool { return aDraft.Version != aExpected; }

		[[nodiscard]] auto advanced(const local_draft& aDraft, const std::int64_t aNow) -> local_draft
		{
			auto draft = aDraft;
			++draft.Version;
			draft.Updated = std::max(aNow, aDraft.Updated);
			return draft;
		}

		[[nodiscard]] auto plain(const std::string_view aText, const std::size_t aLimit) -> bool
		{
			return aText.size() <= aLimit && utf8::valid(aText) && std::ranges::none_of(aText, [](const char aByte) {
					   const auto byte = static_cast<unsigned char>(aByte);
					   return byte < 0x20U || byte == 0x7FU;
				   });
		}

		/// A single-quoted POSIX shell word when the text needs one.
		[[nodiscard]] auto shell_word(const std::string_view aText) -> std::string
		{
			const auto safe = !aText.empty() && std::ranges::all_of(aText, [](const char aByte) {
				return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || (aByte >= '0' && aByte <= '9') || aByte == '/' || aByte == '.' || aByte == '_' || aByte == '-'
					|| aByte == '+' || aByte == ':' || aByte == '@' || aByte == ',';
			});
			if (safe) {
				return std::string{ aText };
			}
			auto out = std::string{ "'" };
			for (const auto byte : aText) {
				if (byte == '\'') {
					out += "'\\''";
				} else {
					out += byte;
				}
			}
			return out + "'";
		}

		[[nodiscard]] auto unresolved_phase(const local_publication_phase aPhase) noexcept -> bool
		{
			return aPhase == local_publication_phase::authorized || aPhase == local_publication_phase::creating || aPhase == local_publication_phase::uncertain;
		}

		[[nodiscard]] auto valid_link(const local_publication_link& aLink) -> bool
		{
			return aLink.Number > 0U && aLink.Url.size() <= maximum_url_bytes && aLink.NodeId.size() <= maximum_url_bytes && valid_publication_owner(aLink.Owner)
				&& valid_publication_repository(aLink.Repository) && aLink.HeadBranch.size() <= maximum_publication_branch_bytes
				&& aLink.BaseBranch.size() <= maximum_publication_branch_bytes && aLink.Author.size() <= maximum_publication_login_bytes;
		}

		/// Associates the draft with the linked pull request and starts its follow-ups.
		[[nodiscard]] auto link(local_draft aDraft, local_publication_link aLink) -> local_draft
		{
			auto& publication  = *aDraft.Publication;
			aLink              = settle_link(publication.Plan, std::move(aLink));
			aDraft.Association = local_draft_association{
				.Host = publication.Plan.Destination.Host, .Owner = aLink.Owner, .Name = aLink.Repository, .Number = aLink.Number, .HeadRef = aLink.HeadBranch
			};
			auto& journal      = publication.Journal;
			journal.Phase      = local_publication_phase::linked;
			journal.Generation = 0U;
			journal.Create     = aLink.Adopted ? local_step_state::not_sent : local_step_state::sent;
			journal.Reason     = aLink.Adopted ? std::string{ "An existing pull request was associated; none was created." } : std::string{ };
			journal.Link       = std::move(aLink);
			journal.FollowUps  = { { .Kind = local_follow_up::association, .State = local_step_state::sent, .Reason = { } } };
			if (publication.Plan.TourFollowUp) {
				journal.FollowUps.push_back({ .Kind = local_follow_up::tour, .State = local_step_state::waiting, .Reason = { } });
			}
			return aDraft;
		}

		[[nodiscard]] auto to_wire(const local_publication_plan& aPlan) -> detail::wire_publication_plan
		{
			return detail::wire_publication_plan{
				.Draft                = aPlan.DraftId,
				.Candidate            = aPlan.Candidate,
				.Head                 = hex(aPlan.Head),
				.Base                 = hex(aPlan.Base),
				.TargetTip            = hex(aPlan.TargetTip),
				.ObservedTargetTip    = aPlan.ObservedTargetTip ? std::optional{ hex(*aPlan.ObservedTargetTip) } : std::nullopt,
				.Host                 = aPlan.Destination.Host,
				.Account              = aPlan.Destination.Account.value(),
				.Login                = aPlan.Destination.Login,
				.Owner                = aPlan.Destination.Owner,
				.Repository           = aPlan.Destination.Repository,
				.BaseBranch           = aPlan.Destination.BaseBranch,
				.HeadBranch           = aPlan.Destination.HeadBranch,
				.Title                = aPlan.Title,
				.Body                 = aPlan.Body,
				.PullDraft            = aPlan.Draft,
				.Evidence             = aPlan.Evidence,
				.Stale                = aPlan.Stale,
				.PrivateRecords       = aPlan.PrivateRecords,
				.PrivateNotes         = aPlan.PrivateNotes,
				.PrivateConversations = aPlan.PrivateConversations,
				.TourFollowUp         = aPlan.TourFollowUp,
				.Unaccepted           = aPlan.Unaccepted,
				.Stops                = aPlan.Stops,
			};
		}

		[[nodiscard]] auto from_wire(const detail::wire_publication_plan& aWire) -> std::optional<local_publication_plan>
		{
			const auto head = parse_git_commit_oid(aWire.Head);
			const auto base = parse_git_commit_oid(aWire.Base);
			const auto tip  = parse_git_commit_oid(aWire.TargetTip);
			auto       seen = std::optional<git_commit_oid>{ };
			if (aWire.ObservedTargetTip) {
				seen = parse_git_commit_oid(*aWire.ObservedTargetTip);
				if (!seen) {
					return std::nullopt;
				}
			}
			if (!head || !base || !tip) {
				return std::nullopt;
			}
			return local_publication_plan{
				.DraftId              = aWire.Draft,
				.Candidate            = aWire.Candidate,
				.Head                 = *head,
				.Base                 = *base,
				.TargetTip            = *tip,
				.ObservedTargetTip    = seen,
				.Destination          = { .Host       = aWire.Host,
										  .Account    = github_account_id{ aWire.Account },
										  .Login      = aWire.Login,
										  .Owner      = aWire.Owner,
										  .Repository = aWire.Repository,
										  .BaseBranch = aWire.BaseBranch,
										  .HeadBranch = aWire.HeadBranch },
				.Title                = aWire.Title,
				.Body                 = aWire.Body,
				.Draft                = aWire.PullDraft,
				.Evidence             = aWire.Evidence,
				.Stale                = aWire.Stale,
				.PrivateRecords       = static_cast<std::size_t>(aWire.PrivateRecords),
				.PrivateNotes         = static_cast<std::size_t>(aWire.PrivateNotes),
				.PrivateConversations = static_cast<std::size_t>(aWire.PrivateConversations),
				.TourFollowUp         = aWire.TourFollowUp,
				.Unaccepted           = aWire.Unaccepted,
				.Stops                = static_cast<std::size_t>(aWire.Stops),
			};
		}

		[[nodiscard]] auto valid(const local_publication_plan& aPlan) -> bool
		{
			const auto& to = aPlan.Destination;
			return aPlan.Candidate > 0U && !aPlan.Title.empty() && plain(aPlan.Title, maximum_title_bytes) && aPlan.Body.size() <= maximum_publication_body_bytes && utf8::valid(aPlan.Body)
				&& !to.Host.empty() && to.Host.size() <= maximum_publication_host_bytes && to.Account.value() != 0U && valid_publication_owner(to.Login) && valid_publication_owner(to.Owner)
				&& valid_publication_repository(to.Repository) && !to.BaseBranch.empty() && to.BaseBranch.size() <= maximum_publication_branch_bytes && valid_publication_branch(to.HeadBranch)
				&& to.HeadBranch != to.BaseBranch;
		}
	}

	auto name(const hosted_branch_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case hosted_branch_state::matches:     return "matches";
		case hosted_branch_state::missing:     return "missing";
		case hosted_branch_state::differs:     return "differs";
		case hosted_branch_state::uncommitted: return "uncommitted";
		case hosted_branch_state::unobserved:  return "unobserved";
		}
		return "unobserved";
	}

	auto name(const local_publication_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case local_publication_refusal::no_candidate:        return "no_candidate";
		case local_publication_refusal::no_account:          return "no_account";
		case local_publication_refusal::uncommitted:         return "uncommitted";
		case local_publication_refusal::branch_missing:      return "branch_missing";
		case local_publication_refusal::branch_differs:      return "branch_differs";
		case local_publication_refusal::branch_unobserved:   return "branch_unobserved";
		case local_publication_refusal::no_permission:       return "no_permission";
		case local_publication_refusal::pull_request_exists: return "pull_request_exists";
		case local_publication_refusal::invalid:             return "invalid";
		case local_publication_refusal::unresolved:          return "unresolved";
		case local_publication_refusal::linked:              return "linked";
		case local_publication_refusal::stale_plan:          return "stale_plan";
		case local_publication_refusal::authority_changed:   return "authority_changed";
		case local_publication_refusal::unauthorized:        return "unauthorized";
		case local_publication_refusal::stale:               return "stale";
		}
		return "invalid";
	}

	auto name(const local_publication_phase aPhase) noexcept -> std::string_view
	{
		switch (aPhase) {
		case local_publication_phase::prepared:   return "prepared";
		case local_publication_phase::authorized: return "authorized";
		case local_publication_phase::creating:   return "creating";
		case local_publication_phase::linked:     return "linked";
		case local_publication_phase::uncertain:  return "uncertain";
		case local_publication_phase::refused:    return "refused";
		case local_publication_phase::cancelled:  return "cancelled";
		}
		return "prepared";
	}

	auto name(const local_step_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case local_step_state::waiting:  return "waiting";
		case local_step_state::sending:  return "sending";
		case local_step_state::sent:     return "sent";
		case local_step_state::failed:   return "failed";
		case local_step_state::not_sent: return "not_sent";
		}
		return "waiting";
	}

	auto name(const local_follow_up aFollowUp) noexcept -> std::string_view
	{
		switch (aFollowUp) {
		case local_follow_up::association: return "association";
		case local_follow_up::tour:        return "tour";
		}
		return "association";
	}

	auto name(const tour_follow_up_block aBlock) noexcept -> std::string_view
	{
		switch (aBlock) {
		case tour_follow_up_block::not_linked:   return "not_linked";
		case tour_follow_up_block::not_selected: return "not_selected";
		case tour_follow_up_block::drifted:      return "drifted";
		case tour_follow_up_block::unaccepted:   return "unaccepted";
		case tour_follow_up_block::empty:        return "empty";
		}
		return "not_linked";
	}

	auto decide_hosted_branch(const local_draft_candidate& aCandidate, const bool aObserved, const std::optional<git_commit_oid>& aHead) -> hosted_branch_state
	{
		if (!aCandidate.Included.empty() || std::ranges::find(aCandidate.Qualifications, candidate_qualification::includes_working_files) != aCandidate.Qualifications.end()) {
			return hosted_branch_state::uncommitted;
		}
		if (!aObserved) {
			return hosted_branch_state::unobserved;
		}
		if (!aHead) {
			return hosted_branch_state::missing;
		}
		return *aHead == aCandidate.Head ? hosted_branch_state::matches : hosted_branch_state::differs;
	}

	auto name_push(const local_draft& aDraft, const local_draft_candidate& aCandidate) -> named_push
	{
		const auto& repository = aDraft.Target.Target.Repository;
		auto        push       = named_push{
			.Directory  = aDraft.Worktree.Location,
			.Repository = "https://" + repository.Host + "/" + repository.Owner + "/" + repository.Name + ".git",
			.Command    = { },
		};
		push.Command = "git -C " + shell_word(push.Directory) + " push " + shell_word(push.Repository) + " " + hex(aCandidate.Head) + ":refs/heads/" + shell_word(aDraft.Branch);
		return push;
	}

	auto make_local_publication(const local_draft& aDraft, const local_publication_observation& aObserved, const bool aTourFollowUp)
		-> std::expected<local_publication_plan, local_publication_refusal>
	{
		const auto* const candidate = latest_candidate(aDraft);
		if (candidate == nullptr) {
			return std::unexpected{ local_publication_refusal::no_candidate };
		}
		if (aDraft.Association || (aDraft.Publication && aDraft.Publication->Journal.Phase == local_publication_phase::linked)) {
			return std::unexpected{ local_publication_refusal::linked };
		}
		if (aDraft.Publication && unresolved_phase(aDraft.Publication->Journal.Phase) && aDraft.Publication->Journal.Phase != local_publication_phase::authorized) {
			return std::unexpected{ local_publication_refusal::unresolved };
		}
		const auto& target = aDraft.Target.Target;
		if (aObserved.Account.value() == 0U || aObserved.Login.empty() || aObserved.Host != target.Repository.Host) {
			return std::unexpected{ local_publication_refusal::no_account };
		}
		switch (decide_hosted_branch(*candidate, aObserved.BranchObserved, aObserved.BranchHead)) {
		case hosted_branch_state::matches:     break;
		case hosted_branch_state::missing:     return std::unexpected{ local_publication_refusal::branch_missing };
		case hosted_branch_state::differs:     return std::unexpected{ local_publication_refusal::branch_differs };
		case hosted_branch_state::uncommitted: return std::unexpected{ local_publication_refusal::uncommitted };
		case hosted_branch_state::unobserved:  return std::unexpected{ local_publication_refusal::branch_unobserved };
		}
		if (!aObserved.CanPush || aObserved.Archived) {
			return std::unexpected{ local_publication_refusal::no_permission };
		}
		if (aObserved.Existing) {
			return std::unexpected{ local_publication_refusal::pull_request_exists };
		}
		const auto preview = compose_local_publication(aDraft);
		auto       plan    = local_publication_plan{
			.DraftId           = aDraft.Id.Value,
			.Candidate         = candidate->Ordinal,
			.Head              = candidate->Head,
			.Base              = candidate->Base,
			.TargetTip         = candidate->TargetTip,
			.ObservedTargetTip = aObserved.TargetTip,
			.Destination       = { .Host       = aObserved.Host,
								   .Account    = aObserved.Account,
								   .Login      = aObserved.Login,
								   .Owner      = target.Repository.Owner,
								   .Repository = target.Repository.Name,
								   .BaseBranch = target.Branch,
								   .HeadBranch = aDraft.Branch },
			.Title             = preview.Title,
			.Body              = preview.Body,
			.Draft             = true,
			.TourFollowUp      = aTourFollowUp && !aDraft.Preparation.Tour.Stops.empty(),
			.Unaccepted        = preview.Unaccepted,
			.Stops             = aDraft.Preparation.Tour.Stops.size(),
		};
		const auto chosen = selected_evidence(aDraft);
		for (const auto& claim : chosen.Claims) {
			plan.Evidence.push_back(claim.Id);
		}
		for (const auto& result : chosen.Results) {
			plan.Evidence.push_back(result.Id);
			const auto state = currency(aDraft, result, candidate->Ordinal);
			if (state != evidence_currency::current && state != evidence_currency::carried) {
				plan.Stale.push_back(result.Id);
			}
		}
		for (const auto& question : chosen.Questions) {
			plan.Evidence.push_back(question.Id);
		}
		std::ranges::sort(plan.Evidence);
		const auto& evidence      = aDraft.Evidence;
		const auto  records       = evidence.Claims.size() + evidence.Checks.size() + evidence.Results.size() + evidence.Questions.size();
		plan.PrivateRecords       = records - std::min(records, plan.Evidence.size() + chosen.Checks.size());
		plan.PrivateNotes         = aDraft.Notes.size();
		plan.PrivateConversations = aDraft.Preparation.Conversations.size();
		if (!valid(plan)) {
			return std::unexpected{ local_publication_refusal::invalid };
		}
		return plan;
	}

	auto encode_local_publication_plan(const local_publication_plan& aPlan) -> std::string
	{
		auto text = std::string{ };
		if (glz::write_json(to_wire(aPlan), text)) {
			return { };
		}
		return text;
	}

	auto local_publication_digest(const local_publication_plan& aPlan) -> std::string
	{
		const auto text = encode_local_publication_plan(aPlan);
		return hex(sha256(std::span{ reinterpret_cast<const std::uint8_t*>(text.data()), text.size() }));
	}

	auto render_local_publication_preview(const local_publication_plan& aPlan) -> std::string
	{
		const auto& to  = aPlan.Destination;
		auto        out = std::string{ };
		out += "Create a draft pull request on " + to.Host + " as " + to.Login + ".\n";
		out += "Into " + to.Owner + "/" + to.Repository + " " + to.BaseBranch + " from " + to.HeadBranch + " at " + short_hex(aPlan.Head) + " (candidate " + std::to_string(aPlan.Candidate)
			+ "), the commits you pushed, unchanged.\n";
		if (aPlan.ObservedTargetTip && *aPlan.ObservedTargetTip != aPlan.TargetTip) {
			out += "The target moved since this candidate was captured: GitHub compares against " + short_hex(*aPlan.ObservedTargetTip) + ", not " + short_hex(aPlan.TargetTip) + ".\n";
		}
		out += "Title: " + aPlan.Title + "\n";
		out += aPlan.Body.empty() ? std::string{ "Description: empty.\n" } : "Description: " + std::to_string(aPlan.Body.size()) + " bytes, exactly as accepted.\n";
		out += "Public evidence: " + std::to_string(aPlan.Evidence.size()) + " selected record" + (aPlan.Evidence.size() == 1U ? "" : "s") + ".\n";
		if (!aPlan.Stale.empty()) {
			out += "Unresolved: " + std::to_string(aPlan.Stale.size()) + " selected result" + (aPlan.Stale.size() == 1U ? " does" : "s do") + " not speak for this candidate.\n";
		}
		out += "Stays on this computer: " + std::to_string(aPlan.PrivateRecords) + " evidence record" + (aPlan.PrivateRecords == 1U ? "" : "s") + ", " + std::to_string(aPlan.PrivateNotes)
			+ " note" + (aPlan.PrivateNotes == 1U ? "" : "s") + ", " + std::to_string(aPlan.PrivateConversations) + " agent conversation" + (aPlan.PrivateConversations == 1U ? "" : "s") + ".\n";
		if (aPlan.TourFollowUp) {
			out += "Then the tour (" + std::to_string(aPlan.Stops) + " stop" + (aPlan.Stops == 1U ? "" : "s") + "), as its own publication.";
			out += aPlan.Unaccepted.empty() ? std::string{ "\n" } : " " + std::to_string(aPlan.Unaccepted.size()) + " not accepted yet: the tour waits until they are.\n";
		} else if (!aPlan.Unaccepted.empty()) {
			out += "Unresolved: " + std::to_string(aPlan.Unaccepted.size()) + " tour stop" + (aPlan.Unaccepted.size() == 1U ? " is" : "s are") + " not accepted.\n";
		}
		out += "Plan " + local_publication_digest(aPlan).substr(0U, 16U) + ".\n";
		return out;
	}

	auto encode_local_draft_publication(const local_draft_publication& aPublication) -> std::string
	{
		auto wire = detail::wire_publication{
			.Plan           = to_wire(aPublication.Plan),
			.Digest         = aPublication.Journal.Digest,
			.Phase          = std::string{ name(aPublication.Journal.Phase) },
			.Generation     = aPublication.Journal.Generation,
			.NextGeneration = aPublication.Journal.NextGeneration,
			.Create         = std::string{ name(aPublication.Journal.Create) },
			.Reason         = aPublication.Journal.Reason,
			.Link           = std::nullopt,
			.FollowUps      = { },
		};
		if (const auto& link = aPublication.Journal.Link) {
			wire.Link = detail::wire_publication_link{ .Number     = link->Number,
													   .Url        = link->Url,
													   .NodeId     = link->NodeId,
													   .Owner      = link->Owner,
													   .Repository = link->Repository,
													   .HeadBranch = link->HeadBranch,
													   .Head       = hex(link->Head),
													   .BaseBranch = link->BaseBranch,
													   .Author     = link->Author,
													   .Draft      = link->Draft,
													   .Drifted    = link->Drifted,
													   .Adopted    = link->Adopted };
		}
		for (const auto& step : aPublication.Journal.FollowUps) {
			wire.FollowUps.push_back({ .Kind = std::string{ name(step.Kind) }, .State = std::string{ name(step.State) }, .Reason = step.Reason });
		}
		auto text = std::string{ };
		if (glz::write_json(wire, text)) {
			return { };
		}
		return text;
	}

	auto decode_local_draft_publication(const std::string_view aText) -> std::optional<local_draft_publication>
	{
		auto wire = detail::wire_publication{ };
		if (glz::read<read_options>(wire, aText)) {
			return std::nullopt;
		}
		auto plan   = from_wire(wire.Plan);
		auto phase  = enum_from(wire.Phase, all_phases);
		auto create = enum_from(wire.Create, all_states);
		if (!plan || !phase || !create || wire.NextGeneration == 0U || wire.Generation >= wire.NextGeneration || wire.Reason.size() > maximum_reason) {
			return std::nullopt;
		}
		auto publication = local_draft_publication{
			.Plan    = std::move(*plan),
			.Journal = { .Digest = wire.Digest, .Phase = *phase, .Generation = wire.Generation, .NextGeneration = wire.NextGeneration, .Create = *create, .Reason = wire.Reason },
		};
		if (wire.Link) {
			const auto head = parse_git_commit_oid(wire.Link->Head);
			if (!head) {
				return std::nullopt;
			}
			publication.Journal.Link = local_publication_link{ .Number     = wire.Link->Number,
															   .Url        = wire.Link->Url,
															   .NodeId     = wire.Link->NodeId,
															   .Owner      = wire.Link->Owner,
															   .Repository = wire.Link->Repository,
															   .HeadBranch = wire.Link->HeadBranch,
															   .Head       = *head,
															   .BaseBranch = wire.Link->BaseBranch,
															   .Author     = wire.Link->Author,
															   .Draft      = wire.Link->Draft,
															   .Drifted    = wire.Link->Drifted,
															   .Adopted    = wire.Link->Adopted };
			if (!valid_link(*publication.Journal.Link)) {
				return std::nullopt;
			}
		}
		for (const auto& step : wire.FollowUps) {
			const auto kind  = enum_from(step.Kind, all_followup);
			const auto state = enum_from(step.State, all_states);
			if (!kind || !state || step.Reason.size() > maximum_reason) {
				return std::nullopt;
			}
			publication.Journal.FollowUps.push_back({ .Kind = *kind, .State = *state, .Reason = step.Reason });
		}
		if (!valid(publication.Plan) || publication.Journal.Digest != local_publication_digest(publication.Plan)
			|| (publication.Journal.Phase == local_publication_phase::linked) != publication.Journal.Link.has_value()) {
			return std::nullopt;
		}
		return publication;
	}

	auto record_local_publication(const local_draft& aDraft, const std::uint64_t aExpectedVersion, local_publication_plan aPlan, const std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ local_publication_refusal::stale };
		}
		if (aDraft.Publication) {
			const auto phase = aDraft.Publication->Journal.Phase;
			if (phase == local_publication_phase::linked) {
				return std::unexpected{ local_publication_refusal::linked };
			}
			if (phase == local_publication_phase::creating || phase == local_publication_phase::uncertain) {
				return std::unexpected{ local_publication_refusal::unresolved };
			}
		}
		if (!valid(aPlan) || aPlan.DraftId != aDraft.Id.Value) {
			return std::unexpected{ local_publication_refusal::invalid };
		}
		auto draft        = advanced(aDraft, aNow);
		auto next         = aDraft.Publication ? aDraft.Publication->Journal.NextGeneration : std::uint64_t{ 1U };
		auto digest       = local_publication_digest(aPlan);
		draft.Publication = local_draft_publication{
			.Plan    = std::move(aPlan),
			.Journal = { .Digest = std::move(digest), .Phase = local_publication_phase::prepared, .Generation = 0U, .NextGeneration = next },
		};
		return draft;
	}

	auto authorize_local_publication(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::string_view aDigest, const github_account_id aAccount, const std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ local_publication_refusal::stale };
		}
		if (!aDraft.Publication) {
			return std::unexpected{ local_publication_refusal::stale_plan };
		}
		const auto& journal = aDraft.Publication->Journal;
		if (journal.Phase == local_publication_phase::linked) {
			return std::unexpected{ local_publication_refusal::linked };
		}
		if (journal.Phase == local_publication_phase::creating || journal.Phase == local_publication_phase::uncertain) {
			return std::unexpected{ local_publication_refusal::unresolved };
		}
		if (journal.Phase == local_publication_phase::cancelled || journal.Phase == local_publication_phase::refused) {
			return std::unexpected{ local_publication_refusal::stale_plan };
		}
		if (aDigest != journal.Digest) {
			return std::unexpected{ local_publication_refusal::stale_plan };
		}
		if (aAccount != aDraft.Publication->Plan.Destination.Account) {
			return std::unexpected{ local_publication_refusal::authority_changed };
		}
		const auto* const latest = latest_candidate(aDraft);
		if (latest == nullptr || latest->Ordinal != aDraft.Publication->Plan.Candidate) {
			return std::unexpected{ local_publication_refusal::stale_plan };
		}
		auto  draft     = advanced(aDraft, aNow);
		auto& next      = draft.Publication->Journal;
		next.Phase      = local_publication_phase::authorized;
		next.Generation = next.NextGeneration;
		next.NextGeneration += 1U;
		next.Reason.clear();
		next.Create = local_step_state::waiting;
		return draft;
	}

	auto begin_local_creation(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aGeneration, const github_account_id aAccount, const std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ local_publication_refusal::stale };
		}
		if (!aDraft.Publication || aDraft.Publication->Journal.Phase != local_publication_phase::authorized || aDraft.Publication->Journal.Generation == 0U
			|| aDraft.Publication->Journal.Generation != aGeneration) {
			return std::unexpected{ local_publication_refusal::unauthorized };
		}
		if (aAccount != aDraft.Publication->Plan.Destination.Account) {
			return std::unexpected{ local_publication_refusal::authority_changed };
		}
		auto draft                        = advanced(aDraft, aNow);
		draft.Publication->Journal.Phase  = local_publication_phase::creating;
		draft.Publication->Journal.Create = local_step_state::sending;
		return draft;
	}

	auto complete_local_creation(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const local_creation_outcome aOutcome, std::optional<local_publication_link> aLink,
								 std::string aReason, const std::int64_t aNow) -> std::expected<local_draft, local_publication_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ local_publication_refusal::stale };
		}
		if (!aDraft.Publication || aDraft.Publication->Journal.Phase != local_publication_phase::creating || aReason.size() > maximum_reason) {
			return std::unexpected{ local_publication_refusal::unauthorized };
		}
		auto  draft   = advanced(aDraft, aNow);
		auto& journal = draft.Publication->Journal;
		switch (aOutcome) {
		case local_creation_outcome::created:
			if (!aLink || !valid_link(*aLink) || aLink->Adopted || !local_pull_request_matches(draft.Publication->Plan, *aLink)) {
				return std::unexpected{ local_publication_refusal::invalid };
			}
			return link(std::move(draft), std::move(*aLink));
		case local_creation_outcome::refused:
			journal.Phase  = local_publication_phase::refused;
			journal.Create = local_step_state::failed;
			break;
		case local_creation_outcome::not_sent:
			journal.Phase  = local_publication_phase::prepared;
			journal.Create = local_step_state::not_sent;
			break;
		case local_creation_outcome::uncertain:
			journal.Phase  = local_publication_phase::uncertain;
			journal.Create = local_step_state::failed;
			break;
		}
		journal.Generation = 0U;
		journal.Reason     = std::move(aReason);
		return draft;
	}

	auto restart_local_publication(const local_draft& aDraft) -> local_draft
	{
		if (!aDraft.Publication) {
			return aDraft;
		}
		auto  draft   = aDraft;
		auto& journal = draft.Publication->Journal;
		if (journal.Phase == local_publication_phase::creating) {
			journal.Phase  = local_publication_phase::uncertain;
			journal.Create = local_step_state::failed;
			journal.Reason = "BPR stopped before GitHub answered.";
		} else if (journal.Phase == local_publication_phase::authorized) {
			journal.Phase = local_publication_phase::prepared;
		}
		journal.Generation = 0U;
		for (auto& step : journal.FollowUps) {
			if (step.State == local_step_state::sending) {
				step.State  = local_step_state::failed;
				step.Reason = "BPR stopped before it finished.";
			}
		}
		return draft;
	}

	auto observe_local_creation(const local_draft& aDraft, const std::uint64_t aExpectedVersion, std::optional<local_publication_link> aFound, const std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ local_publication_refusal::stale };
		}
		if (!aDraft.Publication || aDraft.Publication->Journal.Phase != local_publication_phase::uncertain) {
			return std::unexpected{ local_publication_refusal::unauthorized };
		}
		auto draft = advanced(aDraft, aNow);
		if (aFound) {
			if (!valid_link(*aFound) || aFound->Adopted || !local_pull_request_matches(draft.Publication->Plan, *aFound)
				|| aFound->Author != draft.Publication->Plan.Destination.Login) {
				return std::unexpected{ local_publication_refusal::invalid };
			}
			return link(std::move(draft), std::move(*aFound));
		}
		auto& journal  = draft.Publication->Journal;
		journal.Phase  = local_publication_phase::prepared;
		journal.Create = local_step_state::not_sent;
		journal.Reason = "GitHub has no pull request from this branch: nothing was created.";
		return draft;
	}

	auto adopt_local_pull_request(const local_draft& aDraft, const std::uint64_t aExpectedVersion, local_publication_link aLink, const std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ local_publication_refusal::stale };
		}
		if (!aDraft.Publication) {
			return std::unexpected{ local_publication_refusal::stale_plan };
		}
		const auto phase = aDraft.Publication->Journal.Phase;
		if (phase == local_publication_phase::linked) {
			return std::unexpected{ local_publication_refusal::linked };
		}
		if (phase == local_publication_phase::creating) {
			return std::unexpected{ local_publication_refusal::unresolved };
		}
		aLink.Adopted = true;
		if (!valid_link(aLink) || !local_pull_request_matches(aDraft.Publication->Plan, aLink)) {
			return std::unexpected{ local_publication_refusal::invalid };
		}
		return link(advanced(aDraft, aNow), std::move(aLink));
	}

	auto cancel_local_publication(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::int64_t aNow) -> std::expected<local_draft, local_publication_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ local_publication_refusal::stale };
		}
		if (!aDraft.Publication) {
			return std::unexpected{ local_publication_refusal::stale_plan };
		}
		const auto phase = aDraft.Publication->Journal.Phase;
		if (phase == local_publication_phase::linked) {
			return std::unexpected{ local_publication_refusal::linked };
		}
		if (phase == local_publication_phase::creating || phase == local_publication_phase::uncertain) {
			return std::unexpected{ local_publication_refusal::unresolved };
		}
		auto  draft        = advanced(aDraft, aNow);
		auto& journal      = draft.Publication->Journal;
		journal.Phase      = local_publication_phase::cancelled;
		journal.Generation = 0U;
		journal.Create     = local_step_state::not_sent;
		journal.Reason     = "Cancelled before anything was sent.";
		return draft;
	}

	auto record_follow_up(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const local_follow_up aKind, const local_step_state aState, std::string aReason, const std::int64_t aNow)
		-> std::expected<local_draft, local_publication_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ local_publication_refusal::stale };
		}
		if (!aDraft.Publication || aDraft.Publication->Journal.Phase != local_publication_phase::linked || aReason.size() > maximum_reason) {
			return std::unexpected{ local_publication_refusal::unauthorized };
		}
		auto  draft = advanced(aDraft, aNow);
		auto& steps = draft.Publication->Journal.FollowUps;
		auto  step  = std::ranges::find(steps, aKind, &local_follow_up_step::Kind);
		if (step == steps.end()) {
			return std::unexpected{ local_publication_refusal::invalid };
		}
		step->State  = aState;
		step->Reason = std::move(aReason);
		return draft;
	}

	auto local_pull_request_matches(const local_publication_plan& aPlan, const local_publication_link& aLink) -> bool
	{
		const auto& to = aPlan.Destination;
		return aLink.HeadBranch == to.HeadBranch && aLink.BaseBranch == to.BaseBranch && folded_equal(aLink.Owner, to.Owner) && folded_equal(aLink.Repository, to.Repository);
	}

	auto settle_link(const local_publication_plan& aPlan, local_publication_link aLink) -> local_publication_link
	{
		aLink.Drifted = aLink.Head != aPlan.Head || aLink.BaseBranch != aPlan.Destination.BaseBranch;
		return aLink;
	}

	auto tour_follow_up_blocked(const local_draft& aDraft) -> std::optional<tour_follow_up_block>
	{
		if (!aDraft.Publication || !aDraft.Publication->Journal.Link) {
			return tour_follow_up_block::not_linked;
		}
		if (!aDraft.Publication->Plan.TourFollowUp) {
			return tour_follow_up_block::not_selected;
		}
		if (aDraft.Publication->Journal.Link->Drifted) {
			return tour_follow_up_block::drifted;
		}
		if (aDraft.Preparation.Tour.Stops.empty()) {
			return tour_follow_up_block::empty;
		}
		if (!compose_local_publication(aDraft).Unaccepted.empty()) {
			return tour_follow_up_block::unaccepted;
		}
		return std::nullopt;
	}
}
