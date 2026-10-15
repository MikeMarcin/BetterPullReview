#include "bpr/application/shared_draft_application_host.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include <algorithm>
#include <limits>
#include <vector>
namespace bpr::application {
	struct shared_draft_application_host::implementation
	{
		struct consumer
		{
			shared_draft_consumer                        Id;
			core::bpr_window_context                     Window;
			publication                                  Publish;
			bool                                         Active      = true;
			std::optional<core::revisioned_review_draft> Placeholder = { };
		};
		shared_draft_authority                     Authority;
		core::review_draft_scope_id                Scope;
		shared_draft_context_port&                 Contexts;
		std::vector<std::shared_ptr<consumer>>     Consumers    = { };
		std::vector<core::revisioned_review_draft> Drafts       = { };
		std::uint64_t                              NextConsumer = 1, NextDraft = 1;
		std::vector<core::review_draft_id>         Durable         = { };
		std::optional<core::private_review_scope>  DurableScope    = { };
		bool                                       Closing         = false;
		std::uint64_t                              NextPlaceholder = std::numeric_limits<std::uint64_t>::max();
		std::uint64_t                              UnusedBefore    = 0;
		auto                                       admitted(shared_draft_consumer aConsumer) const -> bool
		{
			return !Closing && std::ranges::any_of(Consumers, [&](const auto& aEntry) { return aEntry->Active && aEntry->Id == aConsumer && Contexts.current(aEntry->Window); });
		}
		auto find(core::review_draft_id aDraft) -> core::revisioned_review_draft*
		{
			const auto found = std::ranges::find_if(Drafts, [&](const auto& aEntry) { return aEntry.Draft.Id == aDraft; });
			return found == Drafts.end() ? nullptr : &*found;
		}
		void publish(const core::revisioned_review_draft& aDraft)
		{
			const auto& snapshot  = aDraft;
			const auto  consumers = Consumers;
			for (const auto& subscriber : consumers) {
				const auto* latest = find(snapshot.Draft.Id);
				if (Closing || !latest || latest->Revision != snapshot.Revision) {
					return;
				}
				if (!subscriber->Active || !Contexts.current(subscriber->Window)) {
					continue;
				}
				try {
					subscriber->Publish(snapshot);
				} catch (...) {
					subscriber->Active = false;
				}
			}
		}
		auto change(const core::revisioned_review_draft_command& aCommand) -> shared_draft_result
		{
			auto* record = find(aCommand.Command.Id);
			if (!record) {
				return { .Failure = shared_draft_failure::unknown_draft };
			}
			if (std::ranges::find(Durable, aCommand.Command.Id) != Durable.end()) {
				return { *record, shared_draft_failure::durable_required };
			}
			const auto outcome = core::apply(*record, aCommand);
			if (outcome.Refusal != core::revisioned_draft_refusal::none) {
				return { *record, outcome.Refusal == core::revisioned_draft_refusal::conflict ? shared_draft_failure::conflict : shared_draft_failure::policy, outcome.Refusal };
			}
			*record             = outcome.Accepted;
			const auto accepted = *record;
			if (outcome.Changed) {
				publish(accepted);
			}
			const auto* latest = find(accepted.Draft.Id);
			if (Closing || !latest) {
				return { .Failure = shared_draft_failure::retired };
			}
			if (latest->Revision != accepted.Revision) {
				return { *latest, shared_draft_failure::conflict, core::revisioned_draft_refusal::conflict };
			}
			return { accepted };
		}
	};
	shared_draft_application_host::shared_draft_application_host(shared_draft_authority aAuthority, core::review_draft_scope_id aScope, shared_draft_context_port& aContexts)
		: Implementation{ std::make_unique<implementation>(implementation{ std::move(aAuthority), aScope, aContexts }) }
	{ }
	shared_draft_application_host::~shared_draft_application_host() { shutdown(); }
	auto shared_draft_application_host::attach(const shared_draft_authority& aAuthority, core::bpr_window_context aWindow, publication aPublish) -> std::expected<shared_draft_consumer, shared_draft_failure>
	{
		auto& host = *Implementation;
		if (host.Closing) {
			return std::unexpected(shared_draft_failure::closing);
		}
		if (host.Scope.value() == 0 || aAuthority != host.Authority || !valid(aAuthority.ProfileAuthority) || !aAuthority.HostAuthority.valid()
			|| aAuthority.BprProfile.value() == 0 || aAuthority.Session.value() == 0 || aAuthority.Generation.value() == 0 || aAuthority.Account.value() == 0
			|| aAuthority.Mode != core::github_access_mode::authenticated || aAuthority.Target.Host != aAuthority.ProfileAuthority.Host
			|| aAuthority.Target.Repository.value() == 0 || aAuthority.Target.PullRequest.value() == 0 || !aAuthority.Target.ExpectedHead.valid()
			|| aWindow.Profile != aAuthority.BprProfile || aWindow.Window.value() == 0 || aWindow.Generation.value() == 0 || !aPublish) {
			return std::unexpected(shared_draft_failure::invalid_authority);
		}
		if (!host.Contexts.current(aWindow)) {
			return std::unexpected(shared_draft_failure::retired);
		}
		std::erase_if(host.Consumers, [&](const auto& aEntry) { return !aEntry->Active || !host.Contexts.current(aEntry->Window); });
		if (host.Consumers.size() >= maximum_shared_draft_consumers || std::ranges::any_of(host.Consumers, [&](const auto& aEntry) { return aEntry->Window.Window == aWindow.Window; })) {
			return std::unexpected(shared_draft_failure::capacity);
		}
		if (host.NextConsumer == std::numeric_limits<std::uint64_t>::max()) {
			return std::unexpected(shared_draft_failure::exhausted);
		}
		const auto id = shared_draft_consumer{ host.NextConsumer++ };
		host.Consumers.push_back(std::make_shared<implementation::consumer>(implementation::consumer{ id, aWindow, std::move(aPublish) }));
		return id;
	}
	void shared_draft_application_host::detach(shared_draft_consumer aConsumer)
	{
		for (auto& entry : Implementation->Consumers) {
			if (entry->Id == aConsumer) {
				entry->Active = false;
			}
		}
	}
	auto shared_draft_application_host::allocate(shared_draft_consumer aConsumer, core::source_line_ref aAnchor, std::string aBody, std::optional<std::string> aReply) -> shared_draft_result
	{
		auto& host = *Implementation;
		if (!host.admitted(aConsumer)) {
			return { .Failure = shared_draft_failure::retired };
		}
		if (host.DurableScope) {
			return { .Failure = shared_draft_failure::durable_required };
		}
		if (host.Drafts.size() >= maximum_shared_drafts) {
			return { .Failure = shared_draft_failure::capacity };
		}
		if (host.NextDraft == std::numeric_limits<std::uint64_t>::max()) {
			return { .Failure = shared_draft_failure::exhausted };
		}
		if (!aAnchor.Comparison.valid() || aAnchor.Side > core::diff_side::right || aBody.size() > core::maximum_shared_draft_body_bytes || (aReply && (aReply->empty() || aReply->size() > core::maximum_shared_draft_reply_bytes))) {
			return { .Failure = shared_draft_failure::policy };
		}
		const auto record = core::revisioned_review_draft{ core::review_comment_draft{ core::review_draft_id{ host.NextDraft++ }, aAnchor, std::move(aBody), core::review_draft_state::drafting, std::move(aReply) }, 1, host.Scope };
		host.Drafts.push_back(record);
		host.publish(record);
		if (!host.admitted(aConsumer)) {
			return { .Failure = shared_draft_failure::retired };
		}
		const auto* latest = host.find(record.Draft.Id);
		if (!latest) {
			return { .Failure = shared_draft_failure::unknown_draft };
		}
		if (latest->Revision != record.Revision) {
			return { *latest, shared_draft_failure::conflict, core::revisioned_draft_refusal::conflict };
		}
		return { record };
	}
	auto shared_draft_application_host::select(shared_draft_consumer aConsumer, core::source_line_ref aAnchor, std::optional<std::string> aReply) -> shared_draft_result
	{
		if (!Implementation->admitted(aConsumer)) {
			return { .Failure = shared_draft_failure::retired };
		}
		const auto found = std::ranges::find_if(Implementation->Drafts, [&](const auto& aRecord) {
			return (!Implementation->DurableScope || !core::unused_review_draft(aRecord)) && aRecord.Draft.Anchor == aAnchor && aRecord.Draft.ReplyToThread == aReply
				&& (aRecord.Draft.State == core::review_draft_state::drafting || aRecord.Draft.State == core::review_draft_state::publishing);
		});
		if (found != Implementation->Drafts.end()) {
			return { *found };
		}
		if (Implementation->DurableScope && !aReply) {
			auto& host = *Implementation;
			if (!aAnchor.Comparison.valid() || aAnchor.Side > core::diff_side::right || host.NextPlaceholder <= core::maximum_private_identity) {
				return { .Failure = shared_draft_failure::policy };
			}
			const auto consumer = std::ranges::find_if(host.Consumers, [&](const auto& value) { return value->Id == aConsumer; });
			if (!(*consumer)->Placeholder || (*consumer)->Placeholder->Draft.Anchor != aAnchor) {
				(*consumer)->Placeholder = core::revisioned_review_draft{ { core::review_draft_id{ host.NextPlaceholder-- }, aAnchor, { }, core::review_draft_state::drafting, { } }, 1, host.Scope };
			}
			return { *(*consumer)->Placeholder };
		}
		return allocate(aConsumer, aAnchor, { }, std::move(aReply));
	}

	auto shared_draft_application_host::read(shared_draft_consumer aConsumer, core::review_draft_id aDraft) const -> shared_draft_result
	{
		if (!Implementation->admitted(aConsumer)) {
			return { .Failure = shared_draft_failure::retired };
		}
		const auto consumer = std::ranges::find_if(Implementation->Consumers, [&](const auto& value) { return value->Id == aConsumer; });
		if ((*consumer)->Placeholder && (*consumer)->Placeholder->Draft.Id == aDraft) {
			return { *(*consumer)->Placeholder };
		}
		const auto* record = Implementation->find(aDraft);
		return record ? shared_draft_result{ *record } : shared_draft_result{ .Failure = shared_draft_failure::unknown_draft };
	}
	auto shared_draft_application_host::apply(shared_draft_consumer aConsumer, const core::revisioned_review_draft_command& aCommand) -> shared_draft_result
	{
		if (!Implementation->admitted(aConsumer)) {
			return { .Failure = shared_draft_failure::retired };
		}
		const auto consumer = std::ranges::find_if(Implementation->Consumers, [&](const auto& value) { return value->Id == aConsumer; });
		if ((*consumer)->Placeholder && (*consumer)->Placeholder->Draft.Id == aCommand.Command.Id) {
			return { *(*consumer)->Placeholder, shared_draft_failure::durable_required };
		}
		if (aCommand.Command.Kind == core::review_draft_command_kind::complete_publish || aCommand.Command.Kind == core::review_draft_command_kind::fail_publish) {
			return { .Failure = shared_draft_failure::policy };
		}
		auto result = Implementation->change(aCommand);
		if (!Implementation->admitted(aConsumer)) {
			if (!Implementation->Closing && !result.Failure && result.Accepted && aCommand.Command.Kind == core::review_draft_command_kind::begin_publish) {
				static_cast<void>(Implementation->change({ { result.Accepted->Draft.Id, core::review_draft_command_kind::fail_publish, { } }, result.Accepted->Revision, false, { }, result.Accepted->Scope }));
			}
			return { .Failure = shared_draft_failure::retired };
		}
		return result;
	}
	auto shared_draft_application_host::complete(const shared_draft_authority& aAuthority, core::review_draft_id aDraft, std::uint64_t aPublishingRevision, bool aSuccess) -> shared_draft_result
	{
		if (Implementation->Closing || aAuthority != Implementation->Authority) {
			return { .Failure = shared_draft_failure::retired };
		}
		return Implementation->change({ { aDraft, aSuccess ? core::review_draft_command_kind::complete_publish : core::review_draft_command_kind::fail_publish, { } }, aPublishingRevision, false, { }, Implementation->Scope });
	}
	auto shared_draft_application_host::adopt(const shared_draft_authority& aAuthority, const durable_draft_commit& aCommit) -> std::expected<void, shared_draft_failure>
	{
		auto& host = *Implementation;
		if (host.Closing || aAuthority != host.Authority) {
			return std::unexpected(shared_draft_failure::retired);
		}
		const auto& durableScope = aCommit.scope();
		if (!core::valid_private_scope(durableScope) || durableScope.Profile != host.Authority.BprProfile || durableScope.Host != host.Authority.Target.Host || durableScope.Account != host.Authority.Account || durableScope.Repository != host.Authority.Target.Repository || durableScope.PullRequest != host.Authority.Target.PullRequest || (host.DurableScope && *host.DurableScope != durableScope)) {
			return std::unexpected(shared_draft_failure::invalid_authority);
		}
		auto                                       drafts       = host.Drafts;
		auto                                       durable      = host.Durable;
		auto                                       next         = host.NextDraft;
		const auto                                 unusedBefore = std::max(host.UnusedBefore, aCommit.unused_retirement_frontier());
		std::vector<core::revisioned_review_draft> changed;
		for (const auto id : aCommit.removed_unused()) {
			const auto found = std::ranges::find_if(drafts, [&](const auto& value) { return value.Draft.Id == id; });
			if (found != drafts.end()) {
				if (!core::unused_review_draft(*found) || std::ranges::find(durable, id) == durable.end()) {
					return std::unexpected(shared_draft_failure::conflict);
				}
				drafts.erase(found);
				std::erase(durable, id);
			}
		}
		for (const auto& record : aCommit.records()) {
			const auto& binding = record.Binding;
			const auto& scope   = binding.Scope;
			if (scope != durableScope || scope.Profile != host.Authority.BprProfile || scope.Host != host.Authority.Target.Host || scope.Account != host.Authority.Account || scope.Repository != host.Authority.Target.Repository || scope.PullRequest != host.Authority.Target.PullRequest || binding.HostAuthority != host.Authority.HostAuthority || binding.Revision.HeadOid != host.Authority.Target.ExpectedHead || record.Draft.Scope.value() != 0 || record.Draft.Draft.Id.value() == 0 || record.Draft.Revision == 0) {
				return std::unexpected(shared_draft_failure::invalid_authority);
			}
			if (core::unused_review_draft(record.Draft) && record.Draft.Draft.Id.value() < unusedBefore) {
				continue;
			}
			auto value  = record.Draft;
			value.Scope = host.Scope;
			auto found  = std::ranges::find_if(drafts, [&](const auto& d) { return d.Draft.Id == value.Draft.Id; });
			if (found != drafts.end()) {
				if (std::ranges::find(durable, value.Draft.Id) == durable.end() || value.Revision < found->Revision || (value.Revision == found->Revision && value != *found)) {
					return std::unexpected(shared_draft_failure::conflict);
				}
				if (value == *found) {
					continue;
				}
				*found = value;
			} else {
				if (drafts.size() >= maximum_retained_private_drafts) {
					return std::unexpected(shared_draft_failure::capacity);
				}
				drafts.push_back(value);
				durable.push_back(value.Draft.Id);
			}
			next = std::max(next, value.Draft.Id.value() + 1);
			changed.push_back(value);
		}
		host.DurableScope = durableScope;
		host.Drafts       = std::move(drafts);
		host.Durable      = std::move(durable);
		host.NextDraft    = next;
		host.UnusedBefore = unusedBefore;
		for (const auto& draft : changed) {
			host.publish(draft);
		}
		return { };
	}
	void shared_draft_application_host::shutdown()
	{
		Implementation->Closing = true;
		for (auto& entry : Implementation->Consumers) {
			entry->Active = false;
		}
	}
	auto shared_draft_application_host::authority() const noexcept -> const shared_draft_authority& { return Implementation->Authority; }
	auto shared_draft_application_host::recovery_snapshot() const -> std::vector<core::revisioned_review_draft>
	{
		std::vector<core::revisioned_review_draft> result;
		for (const auto& draft : Implementation->Drafts) {
			if ((draft.Draft.State == core::review_draft_state::drafting || draft.Draft.State == core::review_draft_state::publishing) && !draft.Draft.Body.empty()) {
				result.push_back(draft);
			}
		}
		return result;
	}
}
