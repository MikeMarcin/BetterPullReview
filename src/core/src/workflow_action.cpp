#include "bpr/core/workflow_action.hpp"

#include "bpr/core/json_preflight.hpp"
#include "bpr/core/local_draft.hpp"
#include "bpr/core/utf8.hpp"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>

namespace bpr::core::detail {
	struct wire_finding
	{
		std::string                Check;
		std::string                State;
		std::optional<std::string> Detail;
	};
	struct wire_limitation
	{
		std::string                Kind;
		std::optional<std::string> Detail;
	};
	struct wire_upstream
	{
		std::string Name;
		std::string Fingerprint;
	};
	struct wire_reference
	{
		std::string                Kind;
		std::string                State;
		std::string                Id;
		std::optional<std::string> Url;
		std::optional<std::string> Detail;
	};
	struct wire_record
	{
		std::string                  Action;
		std::string                  State;
		std::uint64_t                Invocation = 0;
		std::uint64_t                Candidate  = 0;
		std::string                  Revision;
		std::uint64_t                TitleRevision       = 0;
		std::uint64_t                DescriptionRevision = 0;
		std::uint64_t                ProblemRevision     = 0;
		std::uint64_t                SolutionRevision    = 0;
		std::string                  PlanDigest;
		std::string                  Answer;
		std::string                  Question;
		std::string                  Message;
		std::vector<wire_finding>    Findings;
		std::vector<wire_limitation> Limitations;
		std::optional<wire_upstream> Upstream;
		std::vector<wire_reference>  References;
		std::vector<std::uint64_t>   Proposals;
		bool                         Reconciled = false;
		std::int64_t                 At         = 0;
	};
	struct wire_draft_workflow
	{
		glz::raw_json            Workflow;
		std::string              Revision;
		std::vector<wire_record> Actions;
		std::uint64_t            NextInvocation = 1;
		std::string              Publisher;
	};

	struct wire_request_action
	{
		std::string              Id;
		std::string              Slot;
		std::string              Task;
		std::string              Workflow;
		std::string              WorkflowName;
		std::string              Revision;
		std::vector<std::string> Effects;
	};
	struct wire_request_candidate
	{
		std::uint64_t            Ordinal = 0;
		std::string              Head;
		std::string              Base;
		std::string              TargetTip;
		std::vector<std::string> Paths;
	};
	struct wire_request_destination
	{
		std::string Host;
		std::string Owner;
		std::string Name;
		std::string BaseBranch;
		std::string HeadBranch;
	};
	struct wire_request_context
	{
		std::string              Title;
		std::string              Description;
		std::string              Problem;
		std::string              Solution;
		std::vector<std::string> Notes;
	};
	struct wire_request_plan
	{
		std::string Digest;
		std::string Host;
		std::string Account;
		std::string Owner;
		std::string Repository;
		std::string BaseBranch;
		std::string HeadBranch;
		std::string Head;
		std::string Title;
		std::string Body;
		bool        Draft = true;
	};
	struct wire_request_pull_request
	{
		std::uint64_t Number = 0;
		std::string   Url;
		std::string   Head;
	};
	struct wire_request
	{
		std::string                              Format;
		std::uint32_t                            Version    = 0U;
		std::uint64_t                            Invocation = 0;
		std::string                              Draft;
		std::uint64_t                            DraftVersion = 0;
		wire_request_action                      Action;
		wire_request_candidate                   Candidate;
		wire_request_destination                 Destination;
		wire_request_context                     Context;
		std::string                              Guidance;
		std::optional<std::string>               Answer;
		std::optional<std::string>               Question;
		std::optional<wire_request_plan>         Plan;
		std::optional<wire_request_pull_request> PullRequest;
		std::vector<wire_reference>              Previous;
	};

	struct wire_proposal
	{
		std::string                Target;
		std::optional<std::string> Title;
		std::string                Text;
	};
	struct wire_response
	{
		std::string                  Format;
		std::uint32_t                Version    = 0U;
		std::uint64_t                Invocation = 0;
		std::string                  Draft;
		std::string                  Action;
		std::string                  Outcome;
		std::optional<std::string>   Message;
		std::vector<wire_proposal>   Proposals;
		std::vector<wire_finding>    Findings;
		std::optional<std::string>   Question;
		std::optional<wire_upstream> Upstream;
		std::vector<wire_limitation> Limitations;
		std::vector<wire_reference>  References;
	};
	struct wire_response_head
	{
		std::string   Format;
		std::uint32_t Version = 0U;
	};
} // namespace bpr::core::detail

template <>
struct glz::meta<bpr::core::detail::wire_finding>
{
	using value_type            = bpr::core::detail::wire_finding;
	static constexpr auto value = object("check", &value_type::Check, "state", &value_type::State, "detail", &value_type::Detail);
};
template <>
struct glz::meta<bpr::core::detail::wire_limitation>
{
	using value_type            = bpr::core::detail::wire_limitation;
	static constexpr auto value = object("kind", &value_type::Kind, "detail", &value_type::Detail);
};
template <>
struct glz::meta<bpr::core::detail::wire_upstream>
{
	using value_type            = bpr::core::detail::wire_upstream;
	static constexpr auto value = object("name", &value_type::Name, "fingerprint", &value_type::Fingerprint);
};
template <>
struct glz::meta<bpr::core::detail::wire_reference>
{
	using value_type            = bpr::core::detail::wire_reference;
	static constexpr auto value = object("kind", &value_type::Kind, "state", &value_type::State, "id", &value_type::Id, "url", &value_type::Url, "detail", &value_type::Detail);
};
template <>
struct glz::meta<bpr::core::detail::wire_record>
{
	using value_type            = bpr::core::detail::wire_record;
	static constexpr auto value = object("action", &value_type::Action, "state", &value_type::State, "invocation", &value_type::Invocation, "candidate", &value_type::Candidate, "revision",
										 &value_type::Revision, "title_revision", &value_type::TitleRevision, "description_revision", &value_type::DescriptionRevision, "problem_revision",
										 &value_type::ProblemRevision, "solution_revision", &value_type::SolutionRevision, "plan_digest", &value_type::PlanDigest, "answer", &value_type::Answer,
										 "question", &value_type::Question, "message", &value_type::Message, "findings", &value_type::Findings, "limitations", &value_type::Limitations, "upstream",
										 &value_type::Upstream, "references", &value_type::References, "proposals", &value_type::Proposals, "reconciled", &value_type::Reconciled, "at",
										 &value_type::At);
};
template <>
struct glz::meta<bpr::core::detail::wire_draft_workflow>
{
	using value_type            = bpr::core::detail::wire_draft_workflow;
	static constexpr auto value = object("workflow", &value_type::Workflow, "revision", &value_type::Revision, "actions", &value_type::Actions, "next_invocation", &value_type::NextInvocation,
										 "publisher", &value_type::Publisher);
};
template <>
struct glz::meta<bpr::core::detail::wire_request_action>
{
	using value_type            = bpr::core::detail::wire_request_action;
	static constexpr auto value = object("id", &value_type::Id, "slot", &value_type::Slot, "task", &value_type::Task, "workflow", &value_type::Workflow, "workflow_name", &value_type::WorkflowName,
										 "revision", &value_type::Revision, "declared_effects", &value_type::Effects);
};
template <>
struct glz::meta<bpr::core::detail::wire_request_candidate>
{
	using value_type            = bpr::core::detail::wire_request_candidate;
	static constexpr auto value = object("ordinal", &value_type::Ordinal, "head", &value_type::Head, "base", &value_type::Base, "target_tip", &value_type::TargetTip, "paths", &value_type::Paths);
};
template <>
struct glz::meta<bpr::core::detail::wire_request_destination>
{
	using value_type            = bpr::core::detail::wire_request_destination;
	static constexpr auto value = object("host", &value_type::Host, "owner", &value_type::Owner, "name", &value_type::Name, "base_branch", &value_type::BaseBranch, "head_branch",
										 &value_type::HeadBranch);
};
template <>
struct glz::meta<bpr::core::detail::wire_request_context>
{
	using value_type            = bpr::core::detail::wire_request_context;
	static constexpr auto value = object("title", &value_type::Title, "description", &value_type::Description, "problem", &value_type::Problem, "solution", &value_type::Solution, "notes",
										 &value_type::Notes);
};
template <>
struct glz::meta<bpr::core::detail::wire_request_plan>
{
	using value_type            = bpr::core::detail::wire_request_plan;
	static constexpr auto value = object("digest", &value_type::Digest, "host", &value_type::Host, "account", &value_type::Account, "owner", &value_type::Owner, "repository",
										 &value_type::Repository, "base_branch", &value_type::BaseBranch, "head_branch", &value_type::HeadBranch, "head", &value_type::Head, "title",
										 &value_type::Title, "body", &value_type::Body, "draft", &value_type::Draft);
};
template <>
struct glz::meta<bpr::core::detail::wire_request_pull_request>
{
	using value_type            = bpr::core::detail::wire_request_pull_request;
	static constexpr auto value = object("number", &value_type::Number, "url", &value_type::Url, "head", &value_type::Head);
};
template <>
struct glz::meta<bpr::core::detail::wire_request>
{
	using value_type            = bpr::core::detail::wire_request;
	static constexpr auto value = object("format", &value_type::Format, "version", &value_type::Version, "invocation", &value_type::Invocation, "draft", &value_type::Draft, "draft_version",
										 &value_type::DraftVersion, "action", &value_type::Action, "candidate", &value_type::Candidate, "destination", &value_type::Destination, "context",
										 &value_type::Context, "guidance", &value_type::Guidance, "answer", &value_type::Answer, "question", &value_type::Question, "plan", &value_type::Plan,
										 "pull_request", &value_type::PullRequest, "previous", &value_type::Previous);
};
template <>
struct glz::meta<bpr::core::detail::wire_proposal>
{
	using value_type            = bpr::core::detail::wire_proposal;
	static constexpr auto value = object("target", &value_type::Target, "title", &value_type::Title, "text", &value_type::Text);
};
template <>
struct glz::meta<bpr::core::detail::wire_response>
{
	using value_type            = bpr::core::detail::wire_response;
	static constexpr auto value = object("format", &value_type::Format, "version", &value_type::Version, "invocation", &value_type::Invocation, "draft", &value_type::Draft, "action",
										 &value_type::Action, "outcome", &value_type::Outcome, "message", &value_type::Message, "proposals", &value_type::Proposals, "findings",
										 &value_type::Findings, "question", &value_type::Question, "upstream", &value_type::Upstream, "limitations", &value_type::Limitations, "references",
										 &value_type::References);
};
template <>
struct glz::meta<bpr::core::detail::wire_response_head>
{
	using value_type            = bpr::core::detail::wire_response_head;
	static constexpr auto value = object("format", &value_type::Format, "version", &value_type::Version);
};

namespace bpr::core {
	namespace {
		constexpr auto strict_options = glz::opts{ .error_on_unknown_keys = true };
		constexpr auto head_options   = glz::opts{ .error_on_unknown_keys = false };
		constexpr auto pretty_options = glz::opts{ .prettify = true };

		template <typename TEnum, std::size_t TCount>
		[[nodiscard]] auto parse_enum(const std::string_view aName) noexcept -> std::optional<TEnum>
		{
			for (std::size_t index = 0U; index < TCount; ++index) {
				if (stable_name(static_cast<TEnum>(index)) == aName) {
					return static_cast<TEnum>(index);
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto bounded(const std::string_view aText, const std::size_t aLimit) noexcept -> bool { return aText.size() <= aLimit && utf8::valid(aText); }

		[[nodiscard]] auto hex(const git_commit_oid& aOid) -> std::string { return std::string{ canonical_hex(aOid).view() }; }

		[[nodiscard]] auto advanced(const local_draft& aDraft, const std::int64_t aNow) -> local_draft
		{
			auto draft = aDraft;
			++draft.Version;
			draft.Updated = std::max(aNow, aDraft.Updated);
			return draft;
		}

		[[nodiscard]] auto mutable_record(local_draft& aDraft, const std::string_view aAction) -> workflow_action_record&
		{
			auto&      records = aDraft.Workflow->Actions;
			const auto found   = std::ranges::find(records, aAction, &workflow_action_record::Action);
			if (found != records.end()) {
				return *found;
			}
			records.push_back(workflow_action_record{ .Action = std::string{ aAction } });
			return records.back();
		}

		/// A handoff whose answer did not settle what happened.
		[[nodiscard]] auto unsettled(const workflow_action_record& aRecord) noexcept -> bool
		{
			if (aRecord.Reconciled) {
				return false;
			}
			switch (aRecord.State) {
			case workflow_action_state::handed_off:
			case workflow_action_state::failed:
			case workflow_action_state::interrupted:
			case workflow_action_state::uncertain:   return true;
			case workflow_action_state::reported:
				return std::ranges::any_of(aRecord.References, [](const workflow_reference& aReference) {
					return aReference.State == workflow_reference_state::failed || aReference.State == workflow_reference_state::unknown;
				});
			case workflow_action_state::idle:
			case workflow_action_state::awaiting_input: return false;
			}
			return false;
		}

		[[nodiscard]] auto linked(const local_draft& aDraft) noexcept -> bool
		{
			return aDraft.Publication && aDraft.Publication->Journal.Phase == local_publication_phase::linked && aDraft.Publication->Journal.Link.has_value();
		}

		/// Starts a new invocation of an action on the latest candidate.
		void begin_invocation(local_draft& aDraft, workflow_action_record& aRecord, const std::int64_t aNow)
		{
			const auto& preparation     = aDraft.Preparation;
			aRecord.Invocation          = aDraft.Workflow->NextInvocation++;
			aRecord.State               = workflow_action_state::handed_off;
			aRecord.Candidate           = latest_candidate(aDraft)->Ordinal;
			aRecord.Revision            = aDraft.Workflow->Revision;
			aRecord.TitleRevision       = preparation.Description.TitleRevision;
			aRecord.DescriptionRevision = preparation.Description.Revision;
			aRecord.ProblemRevision     = preparation.Tour.ProblemRevision;
			aRecord.SolutionRevision    = preparation.Tour.SolutionRevision;
			aRecord.Question.clear();
			aRecord.Message.clear();
			aRecord.Findings.clear();
			aRecord.Limitations.clear();
			aRecord.Upstream.reset();
			aRecord.Reconciled = false;
			aRecord.At         = aNow;
		}

		[[nodiscard]] auto wire_reference_of(const workflow_reference& aReference) -> detail::wire_reference
		{
			return detail::wire_reference{ .Kind   = std::string{ stable_name(aReference.Kind) },
										   .State  = std::string{ stable_name(aReference.State) },
										   .Id     = aReference.Id,
										   .Url    = aReference.Url.empty() ? std::nullopt : std::optional{ aReference.Url },
										   .Detail = aReference.Detail.empty() ? std::nullopt : std::optional{ aReference.Detail } };
		}

		[[nodiscard]] auto read_reference(const detail::wire_reference& aWire) -> std::optional<workflow_reference>
		{
			const auto kind  = parse_enum<workflow_reference_kind, 4U>(aWire.Kind);
			const auto state = parse_enum<workflow_reference_state, 4U>(aWire.State);
			auto       item  = workflow_reference{ .Id = aWire.Id, .Url = aWire.Url.value_or(std::string{ }), .Detail = aWire.Detail.value_or(std::string{ }) };
			if (!kind || !state || !bounded(item.Id, 256U) || !bounded(item.Url, 2048U) || !bounded(item.Detail, maximum_workflow_message_bytes)) {
				return std::nullopt;
			}
			item.Kind  = *kind;
			item.State = *state;
			return item;
		}

		[[nodiscard]] auto read_finding(const detail::wire_finding& aWire) -> std::optional<workflow_finding>
		{
			const auto state = parse_enum<workflow_finding_state, 3U>(aWire.State);
			auto       item  = workflow_finding{ .Check = aWire.Check, .Detail = aWire.Detail.value_or(std::string{ }) };
			if (!state || item.Check.empty() || !bounded(item.Check, 256U) || !bounded(item.Detail, maximum_workflow_message_bytes)) {
				return std::nullopt;
			}
			item.State = *state;
			return item;
		}

		[[nodiscard]] auto read_limitation(const detail::wire_limitation& aWire) -> std::optional<workflow_limitation>
		{
			const auto kind = parse_enum<workflow_limitation_kind, 5U>(aWire.Kind);
			auto       item = workflow_limitation{ .Detail = aWire.Detail.value_or(std::string{ }) };
			if (!kind || !bounded(item.Detail, maximum_workflow_message_bytes)) {
				return std::nullopt;
			}
			item.Kind = *kind;
			return item;
		}

		template <typename TWire, typename TItem>
		[[nodiscard]] auto read_all(const std::vector<TWire>& aWires, std::vector<TItem>& aItems, auto aRead) -> bool
		{
			if (aWires.size() > maximum_workflow_items) {
				return false;
			}
			for (const auto& wire : aWires) {
				auto item = aRead(wire);
				if (!item) {
					return false;
				}
				aItems.push_back(std::move(*item));
			}
			return true;
		}

		[[nodiscard]] auto proposal_target(const std::string_view aName) noexcept -> std::optional<local_proposal_target>
		{
			// A companion proposes the title, the description and the tour's
			// brief; stops need the candidate's lines, which it is not sent.
			for (const auto target : { local_proposal_target::title, local_proposal_target::description, local_proposal_target::problem, local_proposal_target::solution }) {
				if (name(target) == aName) {
					return target;
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto invocation_revision(const workflow_action_record& aRecord, const local_proposal_target aTarget) noexcept -> std::uint64_t
		{
			switch (aTarget) {
			case local_proposal_target::title:       return aRecord.TitleRevision;
			case local_proposal_target::description: return aRecord.DescriptionRevision;
			case local_proposal_target::problem:     return aRecord.ProblemRevision;
			case local_proposal_target::solution:    return aRecord.SolutionRevision;
			case local_proposal_target::stop:
			case local_proposal_target::new_stop:    return 0U;
			}
			return 0U;
		}

		/// Adds or updates reported objects by kind and identity, keeping every earlier one.
		void merge_references(std::vector<workflow_reference>& aKept, const std::vector<workflow_reference>& aReported)
		{
			for (const auto& reference : aReported) {
				const auto same = std::ranges::find_if(aKept, [&reference](const workflow_reference& aEarlier) {
					return !reference.Id.empty() && aEarlier.Kind == reference.Kind && aEarlier.Id == reference.Id;
				});
				if (same != aKept.end()) {
					*same = reference;
				} else if (aKept.size() < maximum_workflow_items) {
					aKept.push_back(reference);
				}
			}
		}

		[[nodiscard]] auto outcome_state(const workflow_outcome aOutcome) noexcept -> workflow_action_state
		{
			switch (aOutcome) {
			case workflow_outcome::proposal:
			case workflow_outcome::findings:
			case workflow_outcome::completed:     return workflow_action_state::reported;
			case workflow_outcome::input_request: return workflow_action_state::awaiting_input;
			case workflow_outcome::failed:        return workflow_action_state::failed;
			case workflow_outcome::interrupted:   return workflow_action_state::interrupted;
			case workflow_outcome::uncertain:     return workflow_action_state::uncertain;
			}
			return workflow_action_state::failed;
		}
	} // namespace

	auto stable_name(const workflow_action_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case workflow_action_state::idle:           return "idle";
		case workflow_action_state::handed_off:     return "handed_off";
		case workflow_action_state::awaiting_input: return "awaiting_input";
		case workflow_action_state::reported:       return "reported";
		case workflow_action_state::failed:         return "failed";
		case workflow_action_state::interrupted:    return "interrupted";
		case workflow_action_state::uncertain:      return "uncertain";
		}
		return "idle";
	}

	auto stable_name(const workflow_finding_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case workflow_finding_state::met:     return "met";
		case workflow_finding_state::unmet:   return "unmet";
		case workflow_finding_state::unknown: return "unknown";
		}
		return "unknown";
	}

	auto stable_name(const workflow_limitation_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case workflow_limitation_kind::upstream_changed: return "upstream_changed";
		case workflow_limitation_kind::missing_helper:   return "missing_helper";
		case workflow_limitation_kind::no_dry_run:       return "no_dry_run";
		case workflow_limitation_kind::review_marker:    return "review_marker";
		case workflow_limitation_kind::unsupported:      return "unsupported";
		}
		return "unsupported";
	}

	auto describe(const workflow_limitation_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case workflow_limitation_kind::upstream_changed: return "The upstream tool's rules changed; affected preparation needs checking again.";
		case workflow_limitation_kind::missing_helper:   return "A helper the companion needs is not available.";
		case workflow_limitation_kind::no_dry_run:       return "The upstream tool has no dry run, so it cannot be used as a check.";
		case workflow_limitation_kind::review_marker:    return "Only the upstream tool can record this marker; BPR does not write it.";
		case workflow_limitation_kind::unsupported:      return "The companion cannot do this step on its own.";
		}
		return "";
	}

	auto stable_name(const workflow_reference_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case workflow_reference_kind::pull_request: return "pull_request";
		case workflow_reference_kind::ticket:       return "ticket";
		case workflow_reference_kind::visual:       return "visual";
		case workflow_reference_kind::checklist:    return "checklist";
		}
		return "ticket";
	}

	auto stable_name(const workflow_reference_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case workflow_reference_state::created: return "created";
		case workflow_reference_state::updated: return "updated";
		case workflow_reference_state::failed:  return "failed";
		case workflow_reference_state::unknown: return "unknown";
		}
		return "unknown";
	}

	auto stable_name(const workflow_publisher aPublisher) noexcept -> std::string_view { return aPublisher == workflow_publisher::external ? "external" : "built_in"; }

	auto stable_name(const workflow_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case workflow_refusal::stale:            return "stale";
		case workflow_refusal::unknown:          return "unknown";
		case workflow_refusal::state:            return "state";
		case workflow_refusal::mismatched:       return "mismatched";
		case workflow_refusal::invalid:          return "invalid";
		case workflow_refusal::publication_open: return "publication_open";
		case workflow_refusal::no_plan:          return "no_plan";
		case workflow_refusal::checks_unmet:     return "checks_unmet";
		case workflow_refusal::not_linked:       return "not_linked";
		case workflow_refusal::other_publisher:  return "other_publisher";
		case workflow_refusal::external_open:    return "external_open";
		}
		return "invalid";
	}

	auto stable_name(const workflow_outcome aOutcome) noexcept -> std::string_view
	{
		switch (aOutcome) {
		case workflow_outcome::proposal:      return "proposal";
		case workflow_outcome::findings:      return "findings";
		case workflow_outcome::input_request: return "input_request";
		case workflow_outcome::completed:     return "completed";
		case workflow_outcome::failed:        return "failed";
		case workflow_outcome::interrupted:   return "interrupted";
		case workflow_outcome::uncertain:     return "uncertain";
		}
		return "failed";
	}

	auto stable_name(const workflow_response_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case workflow_response_refusal::too_large:           return "too_large";
		case workflow_response_refusal::malformed:           return "malformed";
		case workflow_response_refusal::not_a_response:      return "not_a_response";
		case workflow_response_refusal::unsupported_version: return "unsupported_version";
		case workflow_response_refusal::invalid:             return "invalid";
		}
		return "malformed";
	}

	auto describe(const workflow_response_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case workflow_response_refusal::too_large:           return "The answer is larger than 16 MiB.";
		case workflow_response_refusal::malformed:           return "The answer is not JSON this version can read, or has fields it does not know.";
		case workflow_response_refusal::not_a_response:      return "The file is not a workflow answer.";
		case workflow_response_refusal::unsupported_version: return "The answer was written for a newer version of BetterPullReview.";
		case workflow_response_refusal::invalid:             return "The answer has a value out of its bounds.";
		}
		return "";
	}

	auto stable_name(const check_gap aGap) noexcept -> std::string_view
	{
		switch (aGap) {
		case check_gap::never_run:             return "never_run";
		case check_gap::pending:               return "pending";
		case check_gap::earlier_candidate:     return "earlier_candidate";
		case check_gap::earlier_configuration: return "earlier_configuration";
		case check_gap::upstream_changed:      return "upstream_changed";
		case check_gap::finding:               return "finding";
		case check_gap::limitation:            return "limitation";
		}
		return "never_run";
	}

	auto describe(const check_gap aGap) noexcept -> std::string_view
	{
		switch (aGap) {
		case check_gap::never_run:             return "Not run";
		case check_gap::pending:               return "Waiting for its answer";
		case check_gap::earlier_candidate:     return "Ran on an earlier candidate";
		case check_gap::earlier_configuration: return "Ran under an earlier revision of the workflow";
		case check_gap::upstream_changed:      return "The upstream tool changed since it ran";
		case check_gap::finding:               return "Not met, as reported";
		case check_gap::limitation:            return "Reported a limitation";
		}
		return "";
	}

	auto find_action(const draft_workflow& aWorkflow, const std::string_view aAction) -> const workflow_action*
	{
		const auto found = std::ranges::find(aWorkflow.Workflow.Actions, aAction, &workflow_action::Id);
		return found == aWorkflow.Workflow.Actions.end() ? nullptr : &*found;
	}

	auto find_record(const draft_workflow& aWorkflow, const std::string_view aAction) -> const workflow_action_record*
	{
		const auto found = std::ranges::find(aWorkflow.Actions, aAction, &workflow_action_record::Action);
		return found == aWorkflow.Actions.end() ? nullptr : &*found;
	}

	auto valid(const draft_workflow& aWorkflow) -> bool
	{
		auto encoded = encode_workflows(std::span{ &aWorkflow.Workflow, 1U });
		if (!encoded || aWorkflow.Revision != workflow_revision(aWorkflow.Workflow) || aWorkflow.Actions.size() > aWorkflow.Workflow.Actions.size()) {
			return false;
		}
		const auto hasPublisher = std::ranges::any_of(aWorkflow.Workflow.Actions, [](const workflow_action& aAction) { return aAction.Slot == workflow_slot::publish_draft; });
		if (aWorkflow.Publisher == workflow_publisher::external && !hasPublisher) {
			return false;
		}
		auto seen = std::set<std::string>{ };
		return std::ranges::all_of(aWorkflow.Actions, [&aWorkflow, &seen](const workflow_action_record& aRecord) {
			return find_action(aWorkflow, aRecord.Action) != nullptr && seen.insert(aRecord.Action).second && aRecord.Invocation < aWorkflow.NextInvocation
				&& bounded(aRecord.Answer, maximum_workflow_answer_bytes) && bounded(aRecord.Question, maximum_workflow_message_bytes)
				&& bounded(aRecord.Message, maximum_workflow_message_bytes) && aRecord.Findings.size() <= maximum_workflow_items && aRecord.Limitations.size() <= maximum_workflow_items
				&& aRecord.References.size() <= maximum_workflow_items;
		});
	}

	auto encode_draft_workflow(const draft_workflow& aWorkflow) -> std::string
	{
		auto workflow = encode_workflows(std::span{ &aWorkflow.Workflow, 1U });
		auto wire     = detail::wire_draft_workflow{
			.Workflow = glz::raw_json{ workflow.value_or("[]") }, .Revision = aWorkflow.Revision, .Actions = { }, .NextInvocation = aWorkflow.NextInvocation, .Publisher = std::string{ stable_name(aWorkflow.Publisher) }
		};
		for (const auto& record : aWorkflow.Actions) {
			auto item = detail::wire_record{ .Action              = record.Action,
											 .State               = std::string{ stable_name(record.State) },
											 .Invocation          = record.Invocation,
											 .Candidate           = record.Candidate,
											 .Revision            = record.Revision,
											 .TitleRevision       = record.TitleRevision,
											 .DescriptionRevision = record.DescriptionRevision,
											 .ProblemRevision     = record.ProblemRevision,
											 .SolutionRevision    = record.SolutionRevision,
											 .PlanDigest          = record.PlanDigest,
											 .Answer              = record.Answer,
											 .Question            = record.Question,
											 .Message             = record.Message,
											 .Proposals           = record.Proposals,
											 .Reconciled          = record.Reconciled,
											 .At                  = record.At };
			for (const auto& finding : record.Findings) {
				item.Findings.push_back({ .Check = finding.Check, .State = std::string{ stable_name(finding.State) }, .Detail = finding.Detail });
			}
			for (const auto& limitation : record.Limitations) {
				item.Limitations.push_back({ .Kind = std::string{ stable_name(limitation.Kind) }, .Detail = limitation.Detail });
			}
			if (record.Upstream) {
				item.Upstream = detail::wire_upstream{ .Name = record.Upstream->Name, .Fingerprint = record.Upstream->Fingerprint };
			}
			std::ranges::transform(record.References, std::back_inserter(item.References), wire_reference_of);
			wire.Actions.push_back(std::move(item));
		}
		auto text = std::string{ };
		static_cast<void>(glz::write<glz::opts{ }>(wire, text));
		return text;
	}

	auto decode_draft_workflow(const std::string_view aText) -> std::optional<draft_workflow>
	{
		auto wire = detail::wire_draft_workflow{ };
		if (glz::read<strict_options>(wire, aText)) {
			return std::nullopt;
		}
		// The pinned copy's agent IDs are its own (the profile binds them by
		// name), so a first read collects them and a second validates.
		auto agents = std::vector<std::string>{ };
		auto shape  = decode_workflows(wire.Workflow.str, { });
		if (!shape || shape->size() != 1U) {
			return std::nullopt;
		}
		for (const auto& action : shape->front().Workflow.Actions) {
			agents.push_back(action.Agent);
		}
		auto decoded = decode_workflows(wire.Workflow.str, agents);
		if (!decoded || decoded->size() != 1U || decoded->front().Problem) {
			return std::nullopt;
		}
		const auto publisher = parse_enum<workflow_publisher, 2U>(wire.Publisher);
		if (!publisher) {
			return std::nullopt;
		}
		auto workflow = draft_workflow{ .Workflow = std::move(decoded->front().Workflow), .Revision = wire.Revision, .Actions = { }, .NextInvocation = wire.NextInvocation, .Publisher = *publisher };
		for (auto& item : wire.Actions) {
			const auto state = parse_enum<workflow_action_state, 7U>(item.State);
			if (!state) {
				return std::nullopt;
			}
			auto record = workflow_action_record{ .Action              = std::move(item.Action),
												  .State               = *state,
												  .Invocation          = item.Invocation,
												  .Candidate           = item.Candidate,
												  .Revision            = std::move(item.Revision),
												  .TitleRevision       = item.TitleRevision,
												  .DescriptionRevision = item.DescriptionRevision,
												  .ProblemRevision     = item.ProblemRevision,
												  .SolutionRevision    = item.SolutionRevision,
												  .PlanDigest          = std::move(item.PlanDigest),
												  .Answer              = std::move(item.Answer),
												  .Question            = std::move(item.Question),
												  .Message             = std::move(item.Message),
												  .Proposals           = std::move(item.Proposals),
												  .Reconciled          = item.Reconciled,
												  .At                  = item.At };
			if (!read_all(item.Findings, record.Findings, read_finding) || !read_all(item.Limitations, record.Limitations, read_limitation)
				|| !read_all(item.References, record.References, read_reference)) {
				return std::nullopt;
			}
			if (item.Upstream) {
				record.Upstream = workflow_upstream{ .Name = item.Upstream->Name, .Fingerprint = item.Upstream->Fingerprint };
			}
			workflow.Actions.push_back(std::move(record));
		}
		if (!valid(workflow)) {
			return std::nullopt;
		}
		return workflow;
	}

	auto select_workflow(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::optional<workflow_configuration>& aWorkflow, const std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ workflow_refusal::stale };
		}
		if (external_publication_open(aDraft)) {
			return std::unexpected{ workflow_refusal::external_open };
		}
		auto draft = advanced(aDraft, aNow);
		if (!aWorkflow) {
			draft.Workflow.reset();
			return draft;
		}
		auto selected = draft_workflow{ .Workflow = *aWorkflow, .Revision = workflow_revision(*aWorkflow) };
		if (!valid(selected)) {
			return std::unexpected{ workflow_refusal::invalid };
		}
		draft.Workflow = std::move(selected);
		return draft;
	}

	auto accept_workflow_update(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const workflow_configuration& aWorkflow, const std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ workflow_refusal::stale };
		}
		if (!aDraft.Workflow || aDraft.Workflow->Workflow.PortableId != aWorkflow.PortableId) {
			return std::unexpected{ workflow_refusal::unknown };
		}
		// A saved publication plan is not changed under a running handoff.
		if (external_publication_open(aDraft)) {
			return std::unexpected{ workflow_refusal::external_open };
		}
		auto  draft       = advanced(aDraft, aNow);
		auto& workflow    = *draft.Workflow;
		workflow.Workflow = aWorkflow;
		workflow.Revision = workflow_revision(aWorkflow);
		std::erase_if(workflow.Actions, [&workflow](const workflow_action_record& aRecord) { return find_action(workflow, aRecord.Action) == nullptr; });
		if (workflow.Publisher == workflow_publisher::external
			&& std::ranges::none_of(aWorkflow.Actions, [](const workflow_action& aAction) { return aAction.Slot == workflow_slot::publish_draft; })) {
			workflow.Publisher = workflow_publisher::built_in;
		}
		if (!valid(workflow)) {
			return std::unexpected{ workflow_refusal::invalid };
		}
		return draft;
	}

	auto choose_publisher(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const workflow_publisher aPublisher, const std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ workflow_refusal::stale };
		}
		if (!aDraft.Workflow) {
			return std::unexpected{ workflow_refusal::unknown };
		}
		const auto phase = aDraft.Publication ? std::optional{ aDraft.Publication->Journal.Phase } : std::nullopt;
		if (phase == local_publication_phase::creating || phase == local_publication_phase::uncertain || phase == local_publication_phase::linked) {
			return std::unexpected{ workflow_refusal::publication_open };
		}
		if (external_publication_open(aDraft)) {
			return std::unexpected{ workflow_refusal::external_open };
		}
		auto draft                = advanced(aDraft, aNow);
		draft.Workflow->Publisher = aPublisher;
		if (!valid(*draft.Workflow)) {
			return std::unexpected{ workflow_refusal::unknown };
		}
		return draft;
	}

	auto begin_action(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::string_view aAction, std::string aAnswer, const std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ workflow_refusal::stale };
		}
		const auto* const action = aDraft.Workflow ? find_action(*aDraft.Workflow, aAction) : nullptr;
		if (action == nullptr) {
			return std::unexpected{ workflow_refusal::unknown };
		}
		if (effectful(action->Slot)) {
			return std::unexpected{ workflow_refusal::state };
		}
		if (!bounded(aAnswer, maximum_workflow_answer_bytes) || latest_candidate(aDraft) == nullptr) {
			return std::unexpected{ workflow_refusal::invalid };
		}
		auto  draft  = advanced(aDraft, aNow);
		auto& record = mutable_record(draft, aAction);
		// An answer only continues the question the companion asked.
		if (!aAnswer.empty() && record.State != workflow_action_state::awaiting_input) {
			return std::unexpected{ workflow_refusal::state };
		}
		const auto question = record.Question;
		begin_invocation(draft, record, aNow);
		record.Answer   = std::move(aAnswer);
		record.Question = record.Answer.empty() ? std::string{ } : question;
		return draft;
	}

	auto encode_workflow_request(const local_draft& aDraft, const std::string_view aAction) -> std::optional<std::string>
	{
		const auto* const action    = aDraft.Workflow ? find_action(*aDraft.Workflow, aAction) : nullptr;
		const auto* const record    = aDraft.Workflow ? find_record(*aDraft.Workflow, aAction) : nullptr;
		const auto* const candidate = latest_candidate(aDraft);
		if (action == nullptr || record == nullptr || record->Invocation == 0U || candidate == nullptr) {
			return std::nullopt;
		}
		const auto& workflow = *aDraft.Workflow;
		const auto& target   = aDraft.Target.Target;
		auto        wire     = detail::wire_request{
			.Format       = std::string{ workflow_request_format },
			.Version      = workflow_exchange_version,
			.Invocation   = record->Invocation,
			.Draft        = aDraft.Id.Value,
			.DraftVersion = aDraft.Version,
			.Action       = { .Id = action->Id, .Slot = std::string{ stable_name(action->Slot) }, .Task = action->Task, .Workflow = workflow.Workflow.PortableId, .WorkflowName = workflow.Workflow.Name, .Revision = workflow.Revision, .Effects = { } },
			.Candidate    = { .Ordinal = candidate->Ordinal, .Head = hex(candidate->Head), .Base = hex(candidate->Base), .TargetTip = hex(candidate->TargetTip), .Paths = { } },
			.Destination  = { .Host = target.Repository.Host, .Owner = target.Repository.Owner, .Name = target.Repository.Name, .BaseBranch = target.Branch, .HeadBranch = aDraft.Branch },
			.Context      = { .Title       = aDraft.Title,
							  .Description = aDraft.Preparation.Description.Body,
							  .Problem     = aDraft.Preparation.Tour.Problem,
							  .Solution    = aDraft.Preparation.Tour.Solution,
							  .Notes       = { } },
			.Guidance     = workflow.Workflow.Guidance,
			.Answer       = record->Answer.empty() ? std::nullopt : std::optional{ record->Answer },
			.Question     = record->Question.empty() ? std::nullopt : std::optional{ record->Question },
			.Plan         = std::nullopt,
			.PullRequest  = std::nullopt,
			.Previous     = { },
		};
		std::ranges::transform(action->Effects, std::back_inserter(wire.Action.Effects), [](const workflow_effect aEffect) { return std::string{ stable_name(aEffect) }; });
		std::ranges::transform(candidate->Changes, std::back_inserter(wire.Candidate.Paths), &draft_path_change::Path);
		std::ranges::transform(aDraft.Notes, std::back_inserter(wire.Context.Notes), &local_draft_note::Text);
		if (effectful(action->Slot)) {
			if (!aDraft.Publication) {
				return std::nullopt;
			}
			const auto& plan = aDraft.Publication->Plan;
			const auto& to   = plan.Destination;
			wire.Plan        = detail::wire_request_plan{ .Digest     = aDraft.Publication->Journal.Digest,
														  .Host       = to.Host,
														  .Account    = to.Login,
														  .Owner      = to.Owner,
														  .Repository = to.Repository,
														  .BaseBranch = to.BaseBranch,
														  .HeadBranch = to.HeadBranch,
														  .Head       = hex(plan.Head),
														  .Title      = plan.Title,
														  .Body       = plan.Body,
														  .Draft      = plan.Draft };
			if (linked(aDraft)) {
				const auto& link = *aDraft.Publication->Journal.Link;
				wire.PullRequest = detail::wire_request_pull_request{ .Number = link.Number, .Url = link.Url, .Head = hex(link.Head) };
			}
			std::ranges::transform(record->References, std::back_inserter(wire.Previous), wire_reference_of);
		}
		auto text = std::string{ };
		if (glz::write<pretty_options>(wire, text)) {
			return std::nullopt;
		}
		text.push_back('\n');
		return text;
	}

	auto decode_workflow_response(const std::string_view aBytes) -> std::expected<workflow_response, workflow_response_refusal>
	{
		if (aBytes.size() > maximum_workflow_response_bytes) {
			return std::unexpected{ workflow_response_refusal::too_large };
		}
		if (!utf8::valid(aBytes)) {
			return std::unexpected{ workflow_response_refusal::malformed };
		}
		const auto limits = json_preflight_limits{ .Depth = 6U, .ObjectMembers = 32U, .ArrayElements = maximum_workflow_items, .Values = 20'000U, .StringBytes = maximum_workflow_response_bytes };
		if (json_preflight(aBytes, limits).Result != json_preflight_result::valid) {
			return std::unexpected{ workflow_response_refusal::malformed };
		}
		auto head = detail::wire_response_head{ };
		if (glz::read<head_options>(head, aBytes)) {
			return std::unexpected{ workflow_response_refusal::malformed };
		}
		if (head.Format != workflow_response_format) {
			return std::unexpected{ workflow_response_refusal::not_a_response };
		}
		if (head.Version != workflow_exchange_version) {
			return std::unexpected{ workflow_response_refusal::unsupported_version };
		}
		auto wire = detail::wire_response{ };
		if (glz::read<strict_options>(wire, aBytes)) {
			return std::unexpected{ workflow_response_refusal::malformed };
		}
		const auto outcome  = parse_enum<workflow_outcome, 7U>(wire.Outcome);
		auto       response = workflow_response{ .Invocation = wire.Invocation,
												 .Draft      = std::move(wire.Draft),
												 .Action     = std::move(wire.Action),
												 .Message    = wire.Message.value_or(std::string{ }),
												 .Question   = wire.Question.value_or(std::string{ }) };
		if (!outcome || !bounded(response.Message, maximum_workflow_message_bytes) || !bounded(response.Question, maximum_workflow_message_bytes) || wire.Proposals.size() > maximum_workflow_items) {
			return std::unexpected{ workflow_response_refusal::invalid };
		}
		response.Outcome = *outcome;
		for (const auto& proposal : wire.Proposals) {
			const auto target = proposal_target(proposal.Target);
			if (!target || proposal.Text.empty() || !bounded(proposal.Text, maximum_workflow_response_bytes)) {
				return std::unexpected{ workflow_response_refusal::invalid };
			}
			response.Proposals.push_back(local_proposal_draft{ .Target = *target, .Title = proposal.Title.value_or(std::string{ }), .Text = proposal.Text });
		}
		if (!read_all(wire.Findings, response.Findings, read_finding) || !read_all(wire.Limitations, response.Limitations, read_limitation)
			|| !read_all(wire.References, response.References, read_reference)) {
			return std::unexpected{ workflow_response_refusal::invalid };
		}
		if (wire.Upstream) {
			if (wire.Upstream->Name.empty() || !bounded(wire.Upstream->Name, 256U) || wire.Upstream->Fingerprint.empty() || !bounded(wire.Upstream->Fingerprint, 256U)) {
				return std::unexpected{ workflow_response_refusal::invalid };
			}
			response.Upstream = workflow_upstream{ .Name = wire.Upstream->Name, .Fingerprint = wire.Upstream->Fingerprint };
		}
		if (response.Outcome == workflow_outcome::input_request && response.Question.empty()) {
			return std::unexpected{ workflow_response_refusal::invalid };
		}
		return response;
	}

	auto admit_workflow_response(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const workflow_response& aResponse, const std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ workflow_refusal::stale };
		}
		const auto* const action = aDraft.Workflow ? find_action(*aDraft.Workflow, aResponse.Action) : nullptr;
		const auto* const record = aDraft.Workflow ? find_record(*aDraft.Workflow, aResponse.Action) : nullptr;
		if (action == nullptr || record == nullptr) {
			return std::unexpected{ workflow_refusal::unknown };
		}
		// Only the current invocation's answer, for this draft, is admitted:
		// an answer to an earlier one is stale and changes nothing.
		if (aResponse.Draft != aDraft.Id.Value || aResponse.Invocation != record->Invocation || record->Invocation == 0U) {
			return std::unexpected{ workflow_refusal::mismatched };
		}
		if (record->State != workflow_action_state::handed_off && record->State != workflow_action_state::uncertain) {
			return std::unexpected{ workflow_refusal::state };
		}
		const auto publication = effectful(action->Slot);
		if (publication && !aResponse.Proposals.empty()) {
			return std::unexpected{ workflow_refusal::invalid };
		}
		auto draft     = advanced(aDraft, aNow);
		auto proposals = std::vector<std::uint64_t>{ };
		if (!aResponse.Proposals.empty()) {
			const auto producer = aDraft.Workflow->Workflow.Name + ": " + action->Task;
			auto       recorded = record_proposals(draft, draft.Version, 0U, record->Candidate, aResponse.Proposals, { }, producer, aDraft.Workflow->Revision.substr(0U, 12U), aNow);
			if (!recorded) {
				return std::unexpected{ workflow_refusal::invalid };
			}
			draft     = std::move(recorded->first);
			proposals = std::move(recorded->second);
			// Bound to what the invocation read: a field edited since leaves them stale.
			for (auto& proposal : draft.Preparation.Proposals) {
				if (std::ranges::find(proposals, proposal.Id) != proposals.end()) {
					proposal.BaseRevision = invocation_revision(*record, proposal.Target);
				}
			}
			draft.Version = aDraft.Version + 1U;
		}
		auto& kept       = mutable_record(draft, aResponse.Action);
		kept.State       = outcome_state(aResponse.Outcome);
		kept.Message     = aResponse.Message;
		kept.Question    = aResponse.Question;
		kept.Findings    = aResponse.Findings;
		kept.Limitations = aResponse.Limitations;
		kept.Upstream    = aResponse.Upstream;
		kept.At          = aNow;
		kept.Proposals.insert(kept.Proposals.end(), proposals.begin(), proposals.end());
		if (kept.Proposals.size() > maximum_workflow_items) {
			kept.Proposals.erase(kept.Proposals.begin(), kept.Proposals.end() - static_cast<std::ptrdiff_t>(maximum_workflow_items));
		}
		merge_references(kept.References, aResponse.References);
		if (!valid(*draft.Workflow)) {
			return std::unexpected{ workflow_refusal::invalid };
		}
		return draft;
	}

	auto mark_action_lost(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::string_view aAction, const std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ workflow_refusal::stale };
		}
		const auto* const record = aDraft.Workflow ? find_record(*aDraft.Workflow, aAction) : nullptr;
		if (record == nullptr) {
			return std::unexpected{ workflow_refusal::unknown };
		}
		if (record->State != workflow_action_state::handed_off) {
			return std::unexpected{ workflow_refusal::state };
		}
		auto draft                           = advanced(aDraft, aNow);
		mutable_record(draft, aAction).State = workflow_action_state::uncertain;
		mutable_record(draft, aAction).At    = aNow;
		return draft;
	}

	auto required_checks(const local_draft& aDraft) -> std::vector<check_status>
	{
		auto statuses = std::vector<check_status>{ };
		if (!aDraft.Workflow) {
			return statuses;
		}
		const auto& workflow = *aDraft.Workflow;
		const auto* latest   = latest_candidate(aDraft);
		for (const auto& action : workflow.Workflow.Actions) {
			if (!action.Required) {
				continue;
			}
			auto        status        = check_status{ .Action = action.Id, .Task = action.Task };
			const auto* record        = find_record(workflow, action.Id);
			const auto  laterUpstream = [&workflow, record]() {
				return std::ranges::any_of(workflow.Actions, [record](const workflow_action_record& aOther) {
					return aOther.Upstream && aOther.Upstream->Name == record->Upstream->Name && aOther.Upstream->Fingerprint != record->Upstream->Fingerprint
						&& aOther.Invocation > record->Invocation;
				});
			};
			if (record == nullptr || record->Invocation == 0U) {
				status.Gap = check_gap::never_run;
			} else if (record->State == workflow_action_state::handed_off || record->State == workflow_action_state::awaiting_input) {
				status.Gap = check_gap::pending;
			} else if (record->State != workflow_action_state::reported) {
				status.Gap = check_gap::finding;
			} else if (latest == nullptr || record->Candidate != latest->Ordinal) {
				status.Gap = check_gap::earlier_candidate;
			} else if (record->Revision != workflow.Revision) {
				status.Gap = check_gap::earlier_configuration;
			} else if (!record->Limitations.empty()) {
				status.Gap = check_gap::limitation;
			} else if (record->Upstream && laterUpstream()) {
				status.Gap = check_gap::upstream_changed;
			} else if (record->Findings.empty()
					   || !std::ranges::all_of(record->Findings, [](const workflow_finding& aFinding) { return aFinding.State == workflow_finding_state::met; })) {
				status.Gap = check_gap::finding;
			}
			statuses.push_back(std::move(status));
		}
		return statuses;
	}

	auto external_publication_open(const local_draft& aDraft) -> bool
	{
		if (!aDraft.Workflow || linked(aDraft)) {
			return false;
		}
		return std::ranges::any_of(aDraft.Workflow->Actions, [&aDraft](const workflow_action_record& aRecord) {
			const auto* const action = find_action(*aDraft.Workflow, aRecord.Action);
			return action != nullptr && action->Slot == workflow_slot::publish_draft && aRecord.Invocation != 0U && aRecord.State != workflow_action_state::idle && !aRecord.Reconciled;
		});
	}

	auto hand_off_publication(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::string_view aAction, const std::string_view aDigest, const std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ workflow_refusal::stale };
		}
		const auto* const action = aDraft.Workflow ? find_action(*aDraft.Workflow, aAction) : nullptr;
		if (action == nullptr || action->Slot != workflow_slot::publish_draft) {
			return std::unexpected{ workflow_refusal::unknown };
		}
		if (aDraft.Workflow->Publisher != workflow_publisher::external) {
			return std::unexpected{ workflow_refusal::other_publisher };
		}
		if (external_publication_open(aDraft)) {
			return std::unexpected{ workflow_refusal::external_open };
		}
		if (!aDraft.Publication) {
			return std::unexpected{ workflow_refusal::no_plan };
		}
		const auto& journal = aDraft.Publication->Journal;
		if (journal.Phase == local_publication_phase::linked || journal.Phase == local_publication_phase::creating || journal.Phase == local_publication_phase::uncertain) {
			return std::unexpected{ workflow_refusal::publication_open };
		}
		const auto* const latest = latest_candidate(aDraft);
		if ((journal.Phase != local_publication_phase::prepared && journal.Phase != local_publication_phase::authorized) || aDigest != journal.Digest || latest == nullptr
			|| latest->Ordinal != aDraft.Publication->Plan.Candidate) {
			return std::unexpected{ workflow_refusal::no_plan };
		}
		const auto checks = required_checks(aDraft);
		if (std::ranges::any_of(checks, [](const check_status& aStatus) { return aStatus.Gap.has_value(); })) {
			return std::unexpected{ workflow_refusal::checks_unmet };
		}
		auto  draft  = advanced(aDraft, aNow);
		auto& record = mutable_record(draft, aAction);
		begin_invocation(draft, record, aNow);
		record.Answer.clear();
		record.References.clear();
		record.PlanDigest = std::string{ aDigest };
		return draft;
	}

	auto hand_off_follow_up(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::string_view aAction, const std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ workflow_refusal::stale };
		}
		const auto* const action = aDraft.Workflow ? find_action(*aDraft.Workflow, aAction) : nullptr;
		if (action == nullptr || action->Slot != workflow_slot::finish_publication) {
			return std::unexpected{ workflow_refusal::unknown };
		}
		if (!linked(aDraft)) {
			return std::unexpected{ workflow_refusal::not_linked };
		}
		const auto* const earlier = find_record(*aDraft.Workflow, aAction);
		if (earlier != nullptr && (unsettled(*earlier) || earlier->State == workflow_action_state::awaiting_input)) {
			return std::unexpected{ workflow_refusal::state };
		}
		auto  draft  = advanced(aDraft, aNow);
		auto& record = mutable_record(draft, aAction);
		begin_invocation(draft, record, aNow);
		record.Answer.clear();
		record.PlanDigest = draft.Publication->Journal.Digest;
		return draft;
	}

	auto reconcile_external_publication(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::string_view aAction, std::optional<local_publication_link> aFound,
										const std::int64_t aNow) -> std::expected<local_draft, workflow_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ workflow_refusal::stale };
		}
		const auto* const action = aDraft.Workflow ? find_action(*aDraft.Workflow, aAction) : nullptr;
		const auto* const record = aDraft.Workflow ? find_record(*aDraft.Workflow, aAction) : nullptr;
		if (action == nullptr || action->Slot != workflow_slot::publish_draft || record == nullptr) {
			return std::unexpected{ workflow_refusal::unknown };
		}
		if (!external_publication_open(aDraft)) {
			return std::unexpected{ workflow_refusal::state };
		}
		auto draft = advanced(aDraft, aNow);
		if (aFound) {
			// Associated as a pull request BPR did not create, after its head
			// and base are compared with the plan handed off.
			auto adopted = adopt_local_pull_request(aDraft, aExpectedVersion, std::move(*aFound), aNow);
			if (!adopted) {
				return std::unexpected{ adopted.error() == local_publication_refusal::linked ? workflow_refusal::publication_open : workflow_refusal::mismatched };
			}
			draft = std::move(*adopted);
		}
		auto& kept      = mutable_record(draft, aAction);
		kept.Reconciled = true;
		kept.At         = aNow;
		if (aFound) {
			const auto& link = *draft.Publication->Journal.Link;
			merge_references(kept.References, { workflow_reference{ .Kind = workflow_reference_kind::pull_request, .State = workflow_reference_state::created, .Id = std::to_string(link.Number), .Url = link.Url, .Detail = "Read on GitHub" } });
		}
		return draft;
	}

	auto reconcile_follow_up(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::string_view aAction, const std::int64_t aNow)
		-> std::expected<local_draft, workflow_refusal>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ workflow_refusal::stale };
		}
		const auto* const action = aDraft.Workflow ? find_action(*aDraft.Workflow, aAction) : nullptr;
		const auto* const record = aDraft.Workflow ? find_record(*aDraft.Workflow, aAction) : nullptr;
		if (action == nullptr || action->Slot != workflow_slot::finish_publication || record == nullptr) {
			return std::unexpected{ workflow_refusal::unknown };
		}
		if (!unsettled(*record)) {
			return std::unexpected{ workflow_refusal::state };
		}
		auto  draft     = advanced(aDraft, aNow);
		auto& kept      = mutable_record(draft, aAction);
		kept.Reconciled = true;
		kept.At         = aNow;
		return draft;
	}
} // namespace bpr::core
