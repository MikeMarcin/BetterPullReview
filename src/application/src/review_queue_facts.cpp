#include "bpr/application/review_queue_facts.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace bpr::application {
	namespace {
		constexpr std::size_t MaximumNames = 8, MaximumNameBytes = 512, MaximumFactsBytes = 32768;
		void                  append_name(std::vector<std::string>& aNames, std::string aName)
		{
			if (aNames.size() < MaximumNames && !aName.empty() && aName.size() <= MaximumNameBytes) {
				aNames.push_back(std::move(aName));
			}
		}
	}
	auto queue_waiting_for(const github_pull_request_snapshot& aSnapshot) -> core::queue_waiting_facts
	{
		auto result = core::queue_waiting_facts{ };
		result.Head = aSnapshot.PullRequest.HeadOid.value();
		result.Base = aSnapshot.PullRequest.BaseOid.value();
		if (aSnapshot.ReviewRequestsAvailability == github_observation_availability::available || aSnapshot.ReviewRequestActorsObserved || aSnapshot.ReviewRequestsStale) {
			result.Requests      = static_cast<std::uint32_t>(aSnapshot.ReviewRequests.size());
			result.RequestsStale = aSnapshot.ReviewRequestsStale;
			result.NamesKnown    = true;
			for (const auto& request : aSnapshot.ReviewRequests) {
				const auto team = request.Kind == github_requested_actor_kind::team;
				result.People += request.Kind == github_requested_actor_kind::user ? 1U : 0U;
				result.Teams += team ? 1U : 0U;
				auto actor = team ? "@" + request.Organization + "/" + request.Login : "@" + request.Login;
				if (request.Kind == github_requested_actor_kind::unavailable) {
					actor = "Unavailable requested actor";
				}
				append_name(request.AsCodeOwner.value_or(false) ? result.CodeOwners : result.Reviewers, std::move(actor));
			}
		}
		if (aSnapshot.Merge.Availability == github_observation_availability::available || aSnapshot.Merge.Stale) {
			if (aSnapshot.Merge.Review != github_review_decision::unknown) {
				result.RequiredReview = aSnapshot.Merge.Review != github_review_decision::approved;
			}
			if (aSnapshot.Merge.Assessment != github_merge_assessment::unknown) {
				const auto state    = aSnapshot.Merge.Assessment;
				result.MergeBlocked = state == github_merge_assessment::behind || state == github_merge_assessment::blocked || state == github_merge_assessment::conflicts || state == github_merge_assessment::hooks || state == github_merge_assessment::unstable;
			}
			result.AssessmentStale = aSnapshot.Merge.Stale;
		}
		result.ChecksKnown = aSnapshot.ChecksAvailability == github_collection_availability::available && aSnapshot.StatusesAvailability == github_collection_availability::available;

		for (const auto& check : aSnapshot.Checks) {
			const auto conclusion = check.Conclusion;
			if (conclusion == github_check_conclusion::failure || conclusion == github_check_conclusion::timed_out || conclusion == github_check_conclusion::cancelled || conclusion == github_check_conclusion::action_required || check.StatusError) {
				append_name(result.FailedChecks, check.Name);
			}
		}
		return result;
	}
	void merge_queue_waiting(core::queue_waiting_facts& aRetained, const core::queue_waiting_facts& aRead)
	{
		const auto moved = (!aRead.Head.empty() && aRead.Head != aRetained.Head) || (!aRead.Base.empty() && aRead.Base != aRetained.Base);
		if (aRead.Requests && !aRead.RequestsStale) {
			aRetained.Requests = aRead.Requests;
			aRetained.People   = aRead.People;
			aRetained.Teams    = aRead.Teams;
			if (aRead.NamesKnown || *aRead.Requests == 0) {
				aRetained.CodeOwners    = aRead.CodeOwners;
				aRetained.Reviewers     = aRead.Reviewers;
				aRetained.NamesKnown    = aRead.NamesKnown;
				aRetained.RequestsStale = false;
			} else {
				aRetained.RequestsStale = aRetained.NamesKnown;
			}
		} else if (!aRead.Head.empty()) {
			aRetained.RequestsStale = aRetained.Requests.has_value();
		}
		if (aRead.RequiredReview && !aRead.AssessmentStale) {
			aRetained.RequiredReview = aRead.RequiredReview;
		}
		if (aRead.MergeBlocked && !aRead.AssessmentStale) {
			aRetained.MergeBlocked = aRead.MergeBlocked;
		}
		aRetained.AssessmentStale = aRead.AssessmentStale || ((!aRead.RequiredReview || !aRead.MergeBlocked) && (moved || !aRead.Head.empty()));
		if (aRead.ChecksKnown && !aRead.ChecksStale) {
			aRetained.FailedChecks = aRead.FailedChecks;
			aRetained.ChecksKnown  = true;
			aRetained.ChecksStale  = false;
		} else if (!aRead.Head.empty()) {
			for (const auto& name : aRead.FailedChecks) {
				if (aRetained.FailedChecks.size() < MaximumNames && std::ranges::find(aRetained.FailedChecks, name) == aRetained.FailedChecks.end()) {
					aRetained.FailedChecks.push_back(name);
				}
			}
			aRetained.ChecksStale = aRetained.ChecksKnown || !aRetained.FailedChecks.empty();
		}
		if (!aRead.Head.empty()) {
			aRetained.Head = aRead.Head;
		}
		if (!aRead.Base.empty()) {
			aRetained.Base = aRead.Base;
		}
	}
	void merge_queue_change(core::queue_change_facts& aRetained, const core::queue_change_facts& aRead)
	{
		for (const auto field : { &core::queue_change_facts::Additions, &core::queue_change_facts::Deletions, &core::queue_change_facts::Comments }) {
			if (aRead.*field) {
				aRetained.*field = aRead.*field;
			}
		}
		if (aRead.OpenedAt) {
			aRetained.OpenedAt = aRead.OpenedAt;
		}
	}
	// Length framing keeps remote names literal, including delimiters and newlines.
	// [[BPR-1407]]. Version 2 appends the change facts; version 1 still reads.
	// [[BPR-1718]]. Version 3 appends my verdict, its time and whether
	// commits followed it; a row with no verdict keeps its version 2 bytes.
	auto encode_queue_facts(const core::queue_item_facts& aFacts) -> std::optional<std::string>
	{
		const auto verdict  = aFacts.MyReview.Verdict != core::my_review_verdict::none;
		auto       output   = std::string{ verdict ? "3;" : "2;" };
		const auto text     = [&output](std::string_view aText) { output += std::to_string(aText.size()) + ":"; output.append(aText); };
		const auto number   = [&text](std::uint64_t aNumber) { text(std::to_string(aNumber)); };
		const auto optional = [&number](const auto& aValue) { number(aValue ? static_cast<std::uint64_t>(*aValue) + 1 : 0); };
		text(aFacts.Title);
		text(aFacts.Author);
		optional(aFacts.Authored);
		const auto& waiting = aFacts.Waiting;
		optional(waiting.Requests);
		number(waiting.People);
		number(waiting.Teams);
		optional(waiting.RequiredReview);
		optional(waiting.MergeBlocked);
		number(waiting.ChecksKnown);
		number(waiting.NamesKnown);
		text(waiting.Head);
		text(waiting.Base);
		for (const auto* names : { &waiting.CodeOwners, &waiting.Reviewers, &waiting.FailedChecks }) {
			if (names->size() > MaximumNames) {
				return std::nullopt;
			}
			number(names->size());
			for (const auto& name : *names) {
				if (name.size() > MaximumNameBytes) {
					return std::nullopt;
				}
				text(name);
			}
		}
		const auto& change = aFacts.Change;
		if (change.OpenedAt && *change.OpenedAt < 0) {
			return std::nullopt;
		}
		for (const auto value : { change.OpenedAt.transform([](std::int64_t aValue) { return static_cast<std::uint64_t>(aValue); }), change.Additions, change.Deletions, change.Comments }) {
			if (value) {
				number(*value);
			} else {
				text({ });
			}
		}
		if (verdict) {
			const auto& mine = aFacts.MyReview;
			if ((mine.SubmittedAt && *mine.SubmittedAt < 0) || mine.Verdict > core::my_review_verdict::dismissed) {
				return std::nullopt;
			}
			number(static_cast<std::uint64_t>(mine.Verdict));
			if (mine.SubmittedAt) {
				number(static_cast<std::uint64_t>(*mine.SubmittedAt));
			} else {
				text({ });
			}
			number(mine.NewCommits ? 1U : 0U);
		}
		return output.size() <= MaximumFactsBytes ? std::optional{ std::move(output) } : std::nullopt;
	}
	auto decode_queue_facts(std::string_view aBytes) -> std::optional<core::queue_item_facts>
	{
		if (aBytes.empty()) {
			return core::queue_item_facts{ };
		}
		if (aBytes.size() > MaximumFactsBytes || (!aBytes.starts_with("1;") && !aBytes.starts_with("2;") && !aBytes.starts_with("3;"))) {
			return std::nullopt;
		}
		const auto version = aBytes.front();
		aBytes.remove_prefix(2);
		auto       okay = true;
		const auto text = [&]() -> std::string {
			std::size_t size  = 0;
			const auto  colon = aBytes.find(':');
			if (colon == std::string_view::npos) {
				okay = false;
				return { };
			}
			const auto digits = aBytes.substr(0, colon);
			const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), size);
			if (parsed.ec != std::errc{ } || parsed.ptr != digits.data() + digits.size() || size > aBytes.size() - colon - 1) {
				okay = false;
				return { };
			}
			aBytes.remove_prefix(colon + 1);
			auto result = std::string{ aBytes.substr(0, size) };
			aBytes.remove_prefix(size);
			return result;
		};
		const auto number   = [&]() -> std::uint64_t { const auto value = text(); std::uint64_t result = 0; const auto parsed = std::from_chars(value.data(), value.data()+value.size(), result); if (parsed.ec != std::errc{} || parsed.ptr != value.data()+value.size()) { okay = false; } return result; };
		auto       facts    = core::queue_item_facts{ };
		facts.Title         = text();
		facts.Author        = text();
		const auto authored = number();
		if (authored > 2) {
			okay = false;
		}
		if (authored) {
			facts.Authored = authored == 2;
		}
		auto&      waiting  = facts.Waiting;
		const auto requests = number();
		if (requests > 1001) {
			okay = false;
		}
		if (requests) {
			waiting.Requests = static_cast<std::uint32_t>(requests - 1);
		}
		const auto people = number(), teams = number(), required = number(), blocked = number(), known = number();
		if (people > 1000 || teams > 1000 || required > 2 || blocked > 2 || known > 1) {
			okay = false;
		}
		waiting.People = static_cast<std::uint32_t>(people);
		waiting.Teams  = static_cast<std::uint32_t>(teams);
		if (required) {
			waiting.RequiredReview = required == 2;
		}
		if (blocked) {
			waiting.MergeBlocked = blocked == 2;
		}
		waiting.ChecksKnown   = known == 1;
		const auto namesKnown = number();
		if (namesKnown > 1) {
			okay = false;
		}
		waiting.NamesKnown = namesKnown == 1;
		waiting.Head       = text();
		waiting.Base       = text();
		for (auto* names : { &waiting.CodeOwners, &waiting.Reviewers, &waiting.FailedChecks }) {
			const auto count = number();
			if (count > MaximumNames) {
				return std::nullopt;
			}
			for (std::uint64_t index = 0; index < count; ++index) {
				auto name = text();
				if (name.empty() || name.size() > MaximumNameBytes) {
					okay = false;
				}
				names->push_back(std::move(name));
			}
		}
		const auto optional = [&]() -> std::optional<std::uint64_t> {
			if (aBytes.starts_with("0:")) {
				aBytes.remove_prefix(2);
				return std::nullopt;
			}
			return number();
		};
		const auto seconds = [&okay](const std::optional<std::uint64_t> aValue) {
			if (aValue && *aValue > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
				okay = false;
			}
			return aValue.transform([](std::uint64_t aSeconds) { return static_cast<std::int64_t>(aSeconds); });
		};
		if (version >= '2') {
			facts.Change.OpenedAt  = seconds(optional());
			facts.Change.Additions = optional();
			facts.Change.Deletions = optional();
			facts.Change.Comments  = optional();
		}
		if (version == '3') {
			const auto verdict = number();
			const auto at      = seconds(optional());
			const auto moved   = number();
			// Version 3 is written only for a verdict, so none is refused.
			if (verdict == 0U || verdict > static_cast<std::uint64_t>(core::my_review_verdict::dismissed) || moved > 1U) {
				okay = false;
			}
			facts.MyReview = { .Verdict = static_cast<core::my_review_verdict>(std::min<std::uint64_t>(verdict, static_cast<std::uint64_t>(core::my_review_verdict::dismissed))), .SubmittedAt = at, .NewCommits = moved == 1U };
		}
		waiting.RequestsStale   = waiting.Requests.has_value();
		waiting.AssessmentStale = waiting.RequiredReview.has_value() || waiting.MergeBlocked.has_value();
		waiting.ChecksStale     = waiting.ChecksKnown || !waiting.FailedChecks.empty();
		return okay && aBytes.empty() ? std::optional{ std::move(facts) } : std::nullopt;
	}
}
