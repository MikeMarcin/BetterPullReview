#include "bpr/application/shared_draft_application_host.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>
namespace {
	using namespace bpr;
	void expect(bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	template <class T>
	auto digest() -> T
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes.fill(1);
		return T{ bytes };
	}
	struct contexts final : application::shared_draft_context_port
	{
		bool Live = true;
		auto current(const core::bpr_window_context&) const noexcept -> bool override { return Live; }
	};
	auto authority() -> application::shared_draft_authority
	{
		return { .BprProfile = core::bpr_profile_id{ 1 }, .ProfileAuthority = { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = core::git_host_id{ 1 }, .Hostname = application::github_canonical_hostname{ "github.com" }, .ProbeContractRevision = application::github_probe_contract_revision{ 1 } }, .HostAuthority = digest<core::host_authority_fingerprint>(), .Session = core::application_session_id{ 1 }, .Generation = core::github_identity_generation{ 1 }, .Account = core::github_account_id{ 1 }, .Mode = core::github_access_mode::authenticated, .Target = { core::git_host_id{ 1 }, core::repository_id{ 1 }, core::pull_request_id{ 1 }, *core::parse_git_commit_oid(std::string(40, 'a')) } };
	}
	auto window(std::uint64_t aId) -> core::bpr_window_context { return { core::bpr_profile_id{ 1 }, core::bpr_window_id{ aId }, core::bpr_window_generation{ 1 } }; }
}
namespace {
	void test_shared_selection()
	{
		contexts                                   current;
		const auto                                 scope = authority();
		application::shared_draft_application_host host{ scope, core::review_draft_scope_id{ 1 }, current };
		std::size_t                                publications = 0;
		const auto                                 first        = host.attach(scope, window(1), [&](const auto&) { ++publications; });
		const auto                                 second       = host.attach(scope, window(2), [&](const auto&) { ++publications; });
		const auto                                 anchor       = core::source_line_ref{ digest<core::file_comparison_digest>(), core::diff_side::right, 0 };
		const auto                                 selected     = host.select(*first, anchor);
		expect(selected.Accepted && selected.Accepted->Draft.Anchor.Line == 0, "zero-based first line selects and allocates");
		const auto                                 id = selected.Accepted->Draft.Id;
		application::shared_draft_application_host otherOwner{ scope, core::review_draft_scope_id{ 2 }, current };
		const auto                                 replacementConsumer = otherOwner.attach(scope, window(1), [](const auto&) { });
		const auto                                 replacementDraft    = otherOwner.select(*replacementConsumer, anchor);
		expect(replacementDraft.Accepted->Draft.Id == id && replacementDraft.Accepted->Revision == selected.Accepted->Revision, "different owner intentionally repeats local draft ID and revision");
		expect(otherOwner.apply(*replacementConsumer, { { id, core::review_draft_command_kind::edit, "old scope" }, 1, false, { }, selected.Accepted->Scope }).Failure.has_value(), "old owner scope cannot edit replacement draft");

		const auto edited = host.apply(*first, { { id, core::review_draft_command_kind::edit, "shared" }, 1, false, { }, core::review_draft_scope_id{ 1 } });
		const auto before = publications;
		for (std::size_t index = 0; index < 128; ++index) {
			expect(host.select(index % 2 == 0 ? *first : *second, anchor).Accepted == edited.Accepted, "two windows repeatedly select the same accepted draft and revision");
		}
		expect(publications == before, "selection reuse publishes nothing and consumes no draft slots");
		const auto publishing = host.apply(*first, { { id, core::review_draft_command_kind::begin_publish, { } }, 2, false, { }, core::review_draft_scope_id{ 1 } });
		expect(host.select(*second, anchor).Accepted == publishing.Accepted, "publishing draft remains the shared selection");
		expect(!host.complete(scope, id, 3, true).Failure, "publication completes exactly");
		const auto replacement = host.select(*second, anchor);
		expect(replacement.Accepted->Draft.Id != id && replacement.Accepted->Revision == 1, "published drafts never become editable again");
		const auto replacementId = replacement.Accepted->Draft.Id;
		expect(!host.apply(*second, { { replacementId, core::review_draft_command_kind::discard, { } }, 1, false, { }, core::review_draft_scope_id{ 1 } }).Failure, "replacement discards");
		expect(host.select(*first, anchor).Accepted->Draft.Id != replacementId, "discarded identity is never reused");
		const auto reply = host.select(*first, anchor, "thread");
		expect(reply.Accepted->Draft.ReplyToThread == "thread" && host.select(*second, anchor, "thread").Accepted == reply.Accepted, "reply selection shares only the exact reply target");
		host.detach(*first);
		expect(host.select(*first, anchor).Failure.has_value(), "retired selection cannot reuse or allocate");
	}
}

namespace {
	void test_reentrant_initiator_retirement()
	{
		const auto scope  = authority();
		const auto anchor = core::source_line_ref{ digest<core::file_comparison_digest>(), core::diff_side::right, 0 };
		for (int action = 0; action < 3; ++action) {
			contexts                                   current;
			application::shared_draft_application_host host{ scope, core::review_draft_scope_id{ 1 }, current };
			application::shared_draft_consumer         initiator;
			const auto                                 attached = host.attach(scope, window(1), [&](const auto& aDraft) {
				if (aDraft.Revision != 1) {
					return;
				}
				if (action == 0) {
					static_cast<void>(host.apply(initiator, { { aDraft.Draft.Id, core::review_draft_command_kind::edit, "nested" }, 1, false, { }, aDraft.Scope }));
				} else if (action == 1) {
					host.detach(initiator);
				} else {
					host.shutdown();
				}
			});
			initiator                                           = *attached;
			const auto result                                   = host.allocate(initiator, anchor, "initial");
			expect(result.Failure.has_value(), "creation callback edit retirement or shutdown never returns stale admission");
			if (action == 0) {
				expect(result.Accepted && result.Accepted->Revision == 2 && result.Accepted->Draft.Body == "nested", "creation conflict returns latest accepted revision");
			}
		}
		contexts                                   current;
		application::shared_draft_application_host host{ scope, core::review_draft_scope_id{ 1 }, current };
		application::shared_draft_consumer         initiator;
		const auto                                 first = host.attach(scope, window(1), [&](const auto& aDraft) {if (aDraft.Draft.State==core::review_draft_state::publishing){ host.detach(initiator);
} });
		initiator                                        = *first;
		const auto sibling                               = host.attach(scope, window(2), [](const auto&) { });
		const auto draft                                 = host.allocate(initiator, anchor, "publish body");
		const auto id                                    = draft.Accepted->Draft.Id;
		const auto refused                               = host.apply(initiator, { { id, core::review_draft_command_kind::begin_publish, { } }, 1, false, { }, draft.Accepted->Scope });
		expect(refused.Failure == application::shared_draft_failure::retired, "publication callback retirement grants no effect admission");
		const auto retained = host.read(*sibling, id);
		expect(retained.Accepted && retained.Accepted->Draft.State == core::review_draft_state::drafting && retained.Accepted->Revision == 3, "refused unpublished transition settles back to drafting with fresh revision");
		const auto edited = host.apply(*sibling, { { id, core::review_draft_command_kind::edit, "sibling body" }, 3, false, { }, draft.Accepted->Scope });
		expect(!edited.Failure, "sibling can edit after initiator retirement");
		expect(!host.apply(*sibling, { { id, core::review_draft_command_kind::begin_publish, { } }, 4, false, { }, draft.Accepted->Scope }).Failure, "sibling can publish after refusal settlement");
	}
}

int main()
{
	test_reentrant_initiator_retirement();
	test_shared_selection();
	contexts                                   current;
	const auto                                 scope = authority();
	application::shared_draft_application_host host{ scope, core::review_draft_scope_id{ 1 }, current };
	const auto                                 first  = host.attach(scope, window(1), [](const auto&) { });
	const auto                                 second = host.attach(scope, window(2), [](const auto&) { });
	expect(first && second, "two consumers attach");
	const auto allocated = host.allocate(*first, { digest<core::file_comparison_digest>(), core::diff_side::right, 1 }, "initial");
	expect(allocated.Accepted.has_value(), "draft allocates once");
	const auto id       = allocated.Accepted->Draft.Id;
	const auto accepted = host.apply(*first, { { id, core::review_draft_command_kind::edit, "first" }, 1, false, { }, core::review_draft_scope_id{ 1 } });
	expect(accepted.Accepted->Revision == 2, "first revision wins");
	for (const auto kind : { core::review_draft_command_kind::edit, core::review_draft_command_kind::discard, core::review_draft_command_kind::begin_publish }) {
		const auto stale = host.apply(*second, { { id, kind, "exact candidate\n" }, 1, false, { }, core::review_draft_scope_id{ 1 } });
		expect(stale.Failure == application::shared_draft_failure::conflict && stale.Accepted == accepted.Accepted, "stale commands return current record without mutation");
	}
	auto foreign       = scope;
	foreign.Generation = core::github_identity_generation{ 2 };
	expect(!host.attach(foreign, window(3), [](const auto&) { }), "foreign generation refuses");
	host.detach(*first);
	expect(host.apply(*first, { { id, core::review_draft_command_kind::discard, { } }, 2, false, { }, core::review_draft_scope_id{ 1 } }).Failure.has_value(), "retired token refuses");
	for (std::uint64_t count = 3; count < 100; ++count) {
		const auto token = host.attach(scope, window(count), [](const auto&) { });
		expect(token.has_value(), "consumer churn reclaims bounded slots");
		host.detach(*token);
	}
	const auto reselected = host.read(*second, id);
	expect(reselected.Accepted == accepted.Accepted, "reselection retains exact draft revision");
	const auto publishing = host.apply(*second, { { id, core::review_draft_command_kind::begin_publish, { } }, 2, false, { }, core::review_draft_scope_id{ 1 } });
	expect(publishing.Accepted->Revision == 3, "publication gets exact revision");
	host.detach(*second);
	expect(host.complete(scope, id, 2, true).Failure.has_value(), "old terminal refuses");
	expect(!host.complete(scope, id, 3, false).Failure, "exact terminal works after editor detach");
	const auto reentrant = host.attach(scope, window(101), [&](const auto& aDraft) { if (aDraft.Revision==5){ static_cast<void>(host.complete(scope,id,5,false));
} });
	// A separate two-subscriber inline update must not deliver an older revision after a newer one.
	application::shared_draft_consumer editor;
	std::vector<std::uint64_t>         seen;
	const auto                         mutator  = host.attach(scope, window(102), [&](const auto& aDraft) { if (aDraft.Revision==5){ static_cast<void>(host.apply(editor,{{id,core::review_draft_command_kind::edit,"nested"},5,false,{},core::review_draft_scope_id{1}}));
} });
	const auto                         observer = host.attach(scope, window(103), [&](const auto& aDraft) { seen.push_back(aDraft.Revision); });
	expect(reentrant && mutator && observer, "reentrant consumers attach");
	editor = *mutator;
	static_cast<void>(host.apply(editor, { { id, core::review_draft_command_kind::edit, "outer" }, 4, false, { }, core::review_draft_scope_id{ 1 } }));
	expect(seen == std::vector<std::uint64_t>{ 6 }, "reentrant fanout refuses older outer publication");
	for (std::uint64_t count = 104; count < 133; ++count) {
		expect(host.attach(scope, window(count), [](const auto&) { }).has_value(), "consumer capacity accepts exactly bounded live set");
	}
	expect(!host.attach(scope, window(133), [](const auto&) { }), "one beyond consumer bound refuses");
	for (std::size_t count = 1; count < application::maximum_shared_drafts; ++count) {
		expect(host.allocate(editor, { digest<core::file_comparison_digest>(), core::diff_side::right, 1 }, "bounded").Accepted.has_value(), "record capacity accepts bounded draft allocations");
	}
	expect(host.allocate(editor, { digest<core::file_comparison_digest>(), core::diff_side::right, 1 }, "overflow").Failure == application::shared_draft_failure::capacity, "draft cap refuses without evicting accepted record");
	current.Live = false;
	expect(host.read(editor, id).Failure.has_value(), "current-window gate retires live token");
	host.shutdown();
	expect(!host.attach(scope, window(200), [](const auto&) { }), "shutdown closes ingress");
	return EXIT_SUCCESS;
}
