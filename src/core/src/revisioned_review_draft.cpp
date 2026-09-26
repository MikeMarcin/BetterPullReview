#include "bpr/core/revisioned_review_draft.hpp"
#include <algorithm>
#include <limits>
namespace bpr::core {
	auto promote_private_text(const std::string_view                        aText,
							  const std::optional<revisioned_review_draft>& aTarget)
		-> std::optional<revisioned_review_draft_command>
	{
		if (!aTarget || aText.size() > maximum_shared_draft_body_bytes
			|| !std::ranges::any_of(aText, [](const char aValue) { return aValue != ' ' && aValue != '\t' && aValue != '\r' && aValue != '\n' && aValue != '\v' && aValue != '\f'; })
			|| aTarget->Draft.State != review_draft_state::drafting) {
			return std::nullopt;
		}
		const auto command = revisioned_review_draft_command{
			.Command          = { .Id = aTarget->Draft.Id, .Kind = review_draft_command_kind::edit, .Body = std::string{ aText } },
			.ExpectedRevision = aTarget->Revision,
			.Scope            = aTarget->Scope
		};
		if (apply(*aTarget, command).Refusal != revisioned_draft_refusal::none) {
			return std::nullopt;
		}
		return command;
	}
	auto apply(const revisioned_review_draft& aDraft, const revisioned_review_draft_command& aCommand) -> revisioned_draft_outcome
	{
		auto result = revisioned_draft_outcome{ aDraft };
		if (aDraft.Scope.value() == 0 || aCommand.Scope != aDraft.Scope || aDraft.Draft.Id.value() == 0 || aDraft.Revision == 0 || aCommand.Command.Id != aDraft.Draft.Id) {
			result.Refusal = revisioned_draft_refusal::invalid_identity;
			return result;
		}
		if (aCommand.ExpectedRevision != aDraft.Revision) {
			result.Refusal = revisioned_draft_refusal::conflict;
			return result;
		}
		if (aCommand.Command.Body.size() > maximum_shared_draft_body_bytes || (aCommand.ReplyToThread && (aCommand.ReplyToThread->empty() || aCommand.ReplyToThread->size() > maximum_shared_draft_reply_bytes))) {
			result.Refusal = revisioned_draft_refusal::bounds;
			return result;
		}
		if (aCommand.Command.Kind > review_draft_command_kind::fail_publish || (aCommand.ChangeReplyTarget && aCommand.Command.Kind != review_draft_command_kind::edit)) {
			result.Refusal = revisioned_draft_refusal::invalid_command;
			return result;
		}
		auto command = aCommand.Command;
		if (aCommand.ChangeReplyTarget) {
			command.Body = aDraft.Draft.Body;
		}
		const auto applied = apply(aDraft.Draft, command);
		if (applied.Rejection != review_draft_rejection::none) {
			result.Refusal = revisioned_draft_refusal::policy;
			result.Policy  = applied.Rejection;
			return result;
		}
		auto next = applied.Draft;
		if (aCommand.ChangeReplyTarget) {
			next.ReplyToThread = aCommand.ReplyToThread;
		}
		if (next == aDraft.Draft) {
			return result;
		}
		if (aDraft.Revision == std::numeric_limits<std::uint64_t>::max()) {
			result.Refusal = revisioned_draft_refusal::exhausted;
			return result;
		}
		result.Accepted = { std::move(next), aDraft.Revision + 1, aDraft.Scope };
		result.Changed  = true;
		return result;
	}
}
