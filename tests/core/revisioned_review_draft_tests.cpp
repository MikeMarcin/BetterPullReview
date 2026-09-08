#include "bpr/core/revisioned_review_draft.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
namespace {
	void expect(bool aValue)
	{
		if (!aValue) {
			std::cerr << "revisioned draft failure\n";
			std::exit(EXIT_FAILURE);
		}
	}
}
int main()
{
	using namespace bpr::core;
	revisioned_review_draft draft{ { .Id = review_draft_id{ 1 }, .Body = "initial" }, 1, review_draft_scope_id{ 1 } };
	const auto              edited = apply(draft, { { draft.Draft.Id, review_draft_command_kind::edit, "first" }, 1, false, { }, review_draft_scope_id{ 1 } });
	expect(edited.Changed && edited.Accepted.Revision == 2);
	for (const auto kind : { review_draft_command_kind::edit, review_draft_command_kind::discard, review_draft_command_kind::begin_publish }) {
		const auto refused = apply(edited.Accepted, { { draft.Draft.Id, kind, "candidate" }, 1, false, { }, review_draft_scope_id{ 1 } });
		expect(refused.Refusal == revisioned_draft_refusal::conflict && refused.Accepted == edited.Accepted && !refused.Changed);
	}
	const auto reply = apply(edited.Accepted, { { draft.Draft.Id, review_draft_command_kind::edit, { } }, 2, true, "thread", review_draft_scope_id{ 1 } });
	expect(reply.Changed && reply.Accepted.Revision == 3 && reply.Accepted.Draft.Body == "first" && reply.Accepted.Draft.ReplyToThread == "thread");
	const auto publishing = apply(reply.Accepted, { { draft.Draft.Id, review_draft_command_kind::begin_publish, { } }, 3, false, { }, review_draft_scope_id{ 1 } });
	expect(publishing.Changed && publishing.Accepted.Revision == 4);
	expect(apply(publishing.Accepted, { { draft.Draft.Id, review_draft_command_kind::complete_publish, { } }, 3, false, { }, review_draft_scope_id{ 1 } }).Refusal == revisioned_draft_refusal::conflict);
	expect(apply(publishing.Accepted, { { draft.Draft.Id, review_draft_command_kind::fail_publish, { } }, 4, false, { }, review_draft_scope_id{ 1 } }).Accepted.Revision == 5);
	draft.Revision = std::numeric_limits<std::uint64_t>::max();
	expect(apply(draft, { { draft.Draft.Id, review_draft_command_kind::edit, "new" }, draft.Revision, false, { }, review_draft_scope_id{ 1 } }).Refusal == revisioned_draft_refusal::exhausted);
	expect(apply(edited.Accepted, { { draft.Draft.Id, review_draft_command_kind::edit, std::string(maximum_shared_draft_body_bytes + 1, 'x') }, 2, false, { }, review_draft_scope_id{ 1 } }).Refusal == revisioned_draft_refusal::bounds);
	auto other  = edited.Accepted;
	other.Scope = review_draft_scope_id{ 2 };
	expect(apply(other, { { other.Draft.Id, review_draft_command_kind::edit, "foreign" }, other.Revision, false, { }, review_draft_scope_id{ 1 } }).Refusal == revisioned_draft_refusal::invalid_identity);
	other.Scope = { };
	expect(apply(other, { { other.Draft.Id, review_draft_command_kind::edit, "zero" }, other.Revision }).Refusal == revisioned_draft_refusal::invalid_identity);
	return EXIT_SUCCESS;
}
