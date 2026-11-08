#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/core/comment_edit.hpp"
#include "bpr/core/description_edit.hpp"
#include "bpr/core/review_role.hpp"
#include "bpr/core/reviewer_request.hpp"
#include "github_mutation_contract.hpp"
#include "github_mutation_contract_support.hpp"
#include "github_paged_read.hpp"

#include <algorithm>
#include <array>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

namespace bpr::adapters::github::detail {
	auto review_mutation_session::set_reaction(
		const application::review_reaction_request& aRequest,
		const std::stop_token                       aStop)
		-> std::expected<application::review_reaction_receipt, application::review_mutation_failure>
	{
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return std::unexpected(operation.failure());
			}
			if (aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			if (Lease == nullptr || !Lease->active()) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			if (!valid(Configuration) || !aRequest.LiveBinding.has_value()
				|| aRequest.LiveBinding->Target != Configuration.Target
				|| aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return std::unexpected(application::review_mutation_failure::undesignated_target);
			}
			const auto receipt = [&aRequest](const review_reaction_subject_observation& aObserved, const bool aWritten, const bool aReadBack) {
				const auto* const group = core::find_reaction_group(aObserved.Reactions, aRequest.Content);
				return application::review_reaction_receipt{
					.Subject             = aObserved.Subject,
					.Content             = aRequest.Content,
					.Reacted             = group != nullptr && group->ViewerReacted,
					.Reactions           = aObserved.Reactions,
					.Written             = aWritten,
					.ConfirmedByReadBack = aReadBack,
				};
			};
			const auto asked = [&aRequest](const review_reaction_subject_observation& aObserved) {
				const auto* const group = core::find_reaction_group(aObserved.Reactions, aRequest.Content);
				return (group != nullptr && group->ViewerReacted) == aRequest.Add;
			};
			// Bound to the pull request before anything is written, and the
			// state GitHub has now: a reaction already as asked is not sent.
			const auto before = observe_reaction_subject(aRequest, aStop);
			if (!before.has_value()) {
				return std::unexpected(before.error());
			}
			if (asked(*before)) {
				return receipt(*before, false, false);
			}
			if (!before->Reactions.ViewerCanReact) {
				return std::unexpected(application::review_mutation_failure::refused_by_server);
			}
			const auto wire = compile_reaction_request(aRequest, Configuration.CredentialContext.Authority.ServerContract);
			if (!wire.has_value()) {
				return std::unexpected(map_contract_failure(wire.error()));
			}
			auto       failure  = application::review_mutation_failure::unavailable;
			const auto response = perform(*wire, aStop, { });
			if (response.has_value()) {
				const auto observed = parse_reaction_response(*response, aRequest);
				if (observed.has_value()) {
					return receipt(*observed, true, false);
				}
				// An answer naming another subject or pull request is not
				// ambiguous; it is refused as the pre-flight would have.
				if (observed.error() == review_mutation_contract_failure::identity_mismatch) {
					return std::unexpected(map_contract_failure(observed.error()));
				}
				failure = map_contract_failure(observed.error());
			} else {
				failure = response.error();
			}
			if (failure == application::review_mutation_failure::cancelled || aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			// The outcome is ambiguous: ask once what GitHub shows. Success only
			// when it is the state asked; the write is never sent again.
			const auto after = observe_reaction_subject(aRequest, aStop);
			if (after.has_value() && asked(*after)) {
				return receipt(*after, true, true);
			}
			return std::unexpected(after.has_value() ? application::review_mutation_failure::not_confirmed : failure);
		} catch (const std::bad_alloc&) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}

	auto review_mutation_session::observe_description(
		const application::review_description_request& aRequest,
		const std::stop_token                          aStop)
		-> std::expected<review_description_observation, application::review_mutation_failure>
	{
		const auto wire = compile_description_observation_request(aRequest, Configuration.CredentialContext.Authority.ServerContract);
		if (!wire.has_value()) {
			return std::unexpected(map_contract_failure(wire.error()));
		}
		auto response = perform(*wire, aStop, { });
		if (!response.has_value()) {
			return std::unexpected(response.error());
		}
		auto observed = parse_description_observation(*response, aRequest);
		if (!observed.has_value()) {
			return std::unexpected(map_contract_failure(observed.error()));
		}
		return std::move(*observed);
	}

	auto review_mutation_session::edit_description(
		const application::review_description_request& aRequest,
		const std::stop_token                          aStop)
		-> std::expected<application::review_description_receipt, application::review_mutation_failure>
	{
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return std::unexpected(operation.failure());
			}
			if (aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			if (Lease == nullptr || !Lease->active()) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			if (!valid(Configuration) || !aRequest.LiveBinding.has_value()
				|| aRequest.LiveBinding->Target != Configuration.Target
				|| aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return std::unexpected(application::review_mutation_failure::undesignated_target);
			}
			const auto receipt = [](const review_description_observation& aObserved, const bool aWritten, const bool aReadBack, const bool aConflict) {
				return application::review_description_receipt{
					.Body                = aObserved.Body,
					.UpdatedAt           = aObserved.UpdatedAt,
					.Written             = aWritten,
					.ConfirmedByReadBack = aReadBack,
					.Conflict            = aConflict,
				};
			};
			// [[BPR-1774]]. Bound to the pull request before anything is
			// written, with the body GitHub has now and its word on whether
			// this viewer may update it; the core decides from that.
			const auto before = observe_description(aRequest, aStop);
			if (!before.has_value()) {
				return std::unexpected(before.error());
			}
			switch (core::decide_description_save({ .Original = aRequest.Original, .Edited = aRequest.Body, .Current = before->Body, .ViewerCanUpdate = before->ViewerCanUpdate })) {
			case core::description_save_outcome::already_saved:
				return receipt(*before, false, false, false);
			case core::description_save_outcome::not_permitted:
				return std::unexpected(application::review_mutation_failure::refused_by_server);
			case core::description_save_outcome::conflict:
				// Never overwritten silently: GitHub's newer body goes back, so
				// editing can go on from it.
				return receipt(*before, false, false, true);
			case core::description_save_outcome::write:
				break;
			}
			const auto wire = compile_description_request(aRequest, Configuration.CredentialContext.Authority.ServerContract);
			if (!wire.has_value()) {
				// Every guard but the body's size passed the observation.
				return std::unexpected(application::review_mutation_failure::incompatible_body);
			}
			auto       failure  = application::review_mutation_failure::unavailable;
			const auto response = perform(*wire, aStop, { });
			if (response.has_value()) {
				const auto observed = parse_description_response(*response, aRequest);
				if (observed.has_value()) {
					return receipt(*observed, true, false, false);
				}
				// An answer naming another pull request, or another body, is not
				// ambiguous; it is refused as the pre-flight would have.
				if (observed.error() == review_mutation_contract_failure::identity_mismatch) {
					return std::unexpected(map_contract_failure(observed.error()));
				}
				failure = map_contract_failure(observed.error());
			} else {
				failure = response.error();
			}
			if (failure == application::review_mutation_failure::cancelled || aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			// The outcome is ambiguous: ask once what GitHub has. Success only
			// when it is the text asked; the write is never sent again.
			const auto after = observe_description(aRequest, aStop);
			if (after.has_value() && after->Body == aRequest.Body) {
				return receipt(*after, true, true, false);
			}
			return std::unexpected(after.has_value() ? application::review_mutation_failure::not_confirmed : failure);
		} catch (const std::bad_alloc&) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}

	auto review_mutation_session::reviewers(
		const application::review_reviewer_request& aRequest,
		const std::stop_token                       aStop)
		-> std::expected<application::review_reviewer_receipt, application::review_mutation_failure>
	{
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return std::unexpected(operation.failure());
			}
			if (aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			if (Lease == nullptr || !Lease->active()) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			if (!valid(Configuration) || !aRequest.LiveBinding.has_value()
				|| aRequest.LiveBinding->Target != Configuration.Target
				|| aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return std::unexpected(application::review_mutation_failure::undesignated_target);
			}
			const auto server  = Configuration.CredentialContext.Authority.ServerContract;
			const auto observe = [&]() -> std::expected<core::reviewer_search_answer, application::review_mutation_failure> {
				auto search = aRequest;
				if (aRequest.Operation == application::review_reviewer_operation::request) {
					search.Operation = application::review_reviewer_operation::search;
					search.Query     = aRequest.Login;
					search.User.clear();
					search.Login.clear();
				}
				const auto wire = aRequest.Operation == application::review_reviewer_operation::request
					? compile_reviewer_observation_request(aRequest, server)
					: compile_reviewer_request(aRequest, server);
				if (!wire.has_value()) {
					return std::unexpected(map_contract_failure(wire.error()));
				}
				const auto response = perform(*wire, aStop, { });
				if (!response.has_value()) {
					return std::unexpected(response.error());
				}
				auto observed = parse_reviewer_observation(*response, search);
				if (!observed.has_value()) {
					return std::unexpected(map_contract_failure(observed.error()));
				}
				return std::move(*observed);
			};
			// [[BPR-1804]]. A search is one read.
			if (aRequest.Operation == application::review_reviewer_operation::search) {
				auto answer = observe();
				if (!answer.has_value()) {
					return std::unexpected(answer.error());
				}
				return application::review_reviewer_receipt{ .Answer = std::move(*answer) };
			}
			// A request is bound to the pull request first, and the core decides
			// on what GitHub has now.
			const auto before = observe();
			if (!before.has_value()) {
				return std::unexpected(before.error());
			}
			switch (core::decide_reviewer_write(*before, aRequest.Login)) {
			case core::reviewer_write_decision::already_requested:
				return application::review_reviewer_receipt{ .Answer = *before };
			case core::reviewer_write_decision::author:
				return std::unexpected(application::review_mutation_failure::refused_by_server);
			case core::reviewer_write_decision::write:
				break;
			}
			const auto wire = compile_reviewer_request(aRequest, server);
			if (!wire.has_value()) {
				return std::unexpected(map_contract_failure(wire.error()));
			}
			auto       failure  = application::review_mutation_failure::unavailable;
			const auto response = perform(*wire, aStop, { });
			if (response.has_value()) {
				auto observed = parse_reviewer_response(*response, aRequest);
				if (observed.has_value()) {
					return application::review_reviewer_receipt{ .Answer = std::move(*observed), .Written = true };
				}
				// GraphQL refused the mutation, or the answer names another pull
				// request: neither is ambiguous.
				if (observed.error() == review_mutation_contract_failure::unexpected_body) {
					return std::unexpected(application::review_mutation_failure::refused_by_server);
				}
				if (observed.error() == review_mutation_contract_failure::identity_mismatch) {
					return std::unexpected(map_contract_failure(observed.error()));
				}
				failure = map_contract_failure(observed.error());
			} else {
				failure = response.error();
			}
			if (failure == application::review_mutation_failure::cancelled || aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			// The outcome is ambiguous: ask once what GitHub has. Success only
			// when it lists the person; the write is never sent again.
			const auto after = observe();
			if (after.has_value() && std::ranges::any_of(after->Requested, [&](const std::string& aLogin) { return core::same_login(aLogin, aRequest.Login); })) {
				return application::review_reviewer_receipt{ .Answer = *after, .Written = true, .ConfirmedByReadBack = true };
			}
			return std::unexpected(after.has_value() ? application::review_mutation_failure::not_confirmed : failure);
		} catch (const std::bad_alloc&) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}

	auto review_mutation_session::observe_draft_state(
		const application::review_draft_state_request& aRequest,
		const std::stop_token                          aStop)
		-> std::expected<review_draft_state_observation, application::review_mutation_failure>
	{
		const auto wire = compile_draft_state_observation_request(aRequest, Configuration.CredentialContext.Authority.ServerContract);
		if (!wire.has_value()) {
			return std::unexpected(map_contract_failure(wire.error()));
		}
		auto response = perform(*wire, aStop, { });
		if (!response.has_value()) {
			return std::unexpected(response.error());
		}
		auto observed = parse_draft_state_observation(*response, aRequest);
		if (!observed.has_value()) {
			return std::unexpected(map_contract_failure(observed.error()));
		}
		return *observed;
	}

	auto review_mutation_session::change_draft_state(
		const application::review_draft_state_request& aRequest,
		const std::stop_token                          aStop)
		-> std::expected<application::review_draft_state_receipt, application::review_mutation_failure>
	{
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return std::unexpected(operation.failure());
			}
			if (aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			if (Lease == nullptr || !Lease->active()) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			if (!valid(Configuration) || !aRequest.LiveBinding.has_value()
				|| aRequest.LiveBinding->Target != Configuration.Target
				|| aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return std::unexpected(application::review_mutation_failure::undesignated_target);
			}
			// [[BPR-1852]]. Bound to the pull request before anything is
			// written, with the state GitHub has now and its word on whether
			// this viewer may update it; the core decides from that.
			const auto before = observe_draft_state(aRequest, aStop);
			if (!before.has_value()) {
				return std::unexpected(before.error());
			}
			switch (core::decide_draft_state_write({ .Draft = before->Draft, .Open = before->Open, .ViewerCanUpdate = before->ViewerCanUpdate }, aRequest.Change)) {
			case core::draft_state_outcome::already:
				return application::review_draft_state_receipt{ .Draft = before->Draft };
			case core::draft_state_outcome::not_open:
				return std::unexpected(application::review_mutation_failure::target_unavailable);
			case core::draft_state_outcome::not_permitted:
				return std::unexpected(application::review_mutation_failure::refused_by_server);
			case core::draft_state_outcome::write:
				break;
			}
			const auto wire = compile_draft_state_request(aRequest, Configuration.CredentialContext.Authority.ServerContract);
			if (!wire.has_value()) {
				return std::unexpected(map_contract_failure(wire.error()));
			}
			const auto wanted   = core::draft_after(aRequest.Change);
			auto       failure  = application::review_mutation_failure::unavailable;
			const auto response = perform(*wire, aStop, { });
			if (response.has_value()) {
				const auto observed = parse_draft_state_response(*response, aRequest);
				if (observed.has_value()) {
					// GitHub's answer is what is believed: a state other than the
					// one asked is said, not claimed.
					return application::review_draft_state_receipt{ .Draft = observed->Draft, .Written = true };
				}
				// GraphQL refused the mutation, or the answer names another pull
				// request: neither is ambiguous.
				if (observed.error() == review_mutation_contract_failure::unexpected_body) {
					return std::unexpected(application::review_mutation_failure::refused_by_server);
				}
				if (observed.error() == review_mutation_contract_failure::identity_mismatch) {
					return std::unexpected(map_contract_failure(observed.error()));
				}
				failure = map_contract_failure(observed.error());
			} else {
				failure = response.error();
			}
			if (failure == application::review_mutation_failure::cancelled || aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			// The outcome is ambiguous: ask once what GitHub has. Success only
			// when it is the state asked; the write is never sent again.
			const auto after = observe_draft_state(aRequest, aStop);
			if (after.has_value() && after->Draft == wanted) {
				return application::review_draft_state_receipt{ .Draft = after->Draft, .Written = true, .ConfirmedByReadBack = true };
			}
			return std::unexpected(after.has_value() ? application::review_mutation_failure::not_confirmed : failure);
		} catch (const std::bad_alloc&) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}

	auto review_mutation_session::observe_comment(
		const application::review_comment_edit_request& aRequest,
		const std::stop_token                           aStop)
		-> std::expected<review_description_observation, application::review_mutation_failure>
	{
		const auto wire = compile_comment_observation_request(aRequest, Configuration.CredentialContext.Authority.ServerContract);
		if (!wire.has_value()) {
			return std::unexpected(map_contract_failure(wire.error()));
		}
		auto response = perform(*wire, aStop, { });
		if (!response.has_value()) {
			return std::unexpected(response.error());
		}
		auto observed = parse_comment_observation(*response, aRequest);
		if (!observed.has_value()) {
			return std::unexpected(map_contract_failure(observed.error()));
		}
		return std::move(*observed);
	}

	auto review_mutation_session::edit_comment(
		const application::review_comment_edit_request& aRequest,
		const std::stop_token                           aStop)
		-> std::expected<application::review_comment_edit_receipt, application::review_mutation_failure>
	{
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return std::unexpected(operation.failure());
			}
			if (aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			if (Lease == nullptr || !Lease->active()) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			if (!valid(Configuration) || !aRequest.LiveBinding.has_value()
				|| aRequest.LiveBinding->Target != Configuration.Target
				|| aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return std::unexpected(application::review_mutation_failure::undesignated_target);
			}
			const auto receipt = [](const review_description_observation& aObserved, const bool aWritten, const bool aReadBack, const bool aConflict) {
				return application::review_comment_edit_receipt{
					.Body                = aObserved.Body,
					.UpdatedAt           = aObserved.UpdatedAt,
					.Written             = aWritten,
					.ConfirmedByReadBack = aReadBack,
					.Conflict            = aConflict,
				};
			};
			// [[BPR-1775]]. Bound to the comment, its kind and the pull request
			// before anything is written, with the body GitHub has now and its
			// word on whether this viewer may update it; the core decides.
			const auto before = observe_comment(aRequest, aStop);
			if (!before.has_value()) {
				return std::unexpected(before.error());
			}
			switch (core::decide_comment_save({ .Original = aRequest.Original, .Edited = aRequest.Body, .Current = before->Body, .ViewerCanUpdate = before->ViewerCanUpdate })) {
			case core::description_save_outcome::already_saved:
				return receipt(*before, false, false, false);
			case core::description_save_outcome::not_permitted:
				return std::unexpected(application::review_mutation_failure::refused_by_server);
			case core::description_save_outcome::conflict:
				// Never overwritten silently: GitHub's newer body goes back, so
				// editing can go on from it.
				return receipt(*before, false, false, true);
			case core::description_save_outcome::write:
				break;
			}
			const auto wire = compile_comment_edit_request(aRequest, Configuration.CredentialContext.Authority.ServerContract);
			if (!wire.has_value()) {
				// Every guard but the body's size passed the observation.
				return std::unexpected(application::review_mutation_failure::incompatible_body);
			}
			auto       failure  = application::review_mutation_failure::unavailable;
			const auto response = perform(*wire, aStop, { });
			if (response.has_value()) {
				const auto observed = parse_comment_edit_response(*response, aRequest);
				if (observed.has_value()) {
					return receipt(*observed, true, false, false);
				}
				// An answer naming another comment, pull request or body, is not
				// ambiguous; it is refused as the pre-flight would have.
				if (observed.error() == review_mutation_contract_failure::identity_mismatch) {
					return std::unexpected(map_contract_failure(observed.error()));
				}
				failure = map_contract_failure(observed.error());
			} else {
				failure = response.error();
			}
			if (failure == application::review_mutation_failure::cancelled || aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			// The outcome is ambiguous: ask once what GitHub has. Success only
			// when it is the text asked; the write is never sent again.
			const auto after = observe_comment(aRequest, aStop);
			if (after.has_value() && after->Body == aRequest.Body) {
				return receipt(*after, true, true, false);
			}
			return std::unexpected(after.has_value() ? application::review_mutation_failure::not_confirmed : failure);
		} catch (const std::bad_alloc&) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}

	auto review_mutation_session::off_hunk_key(std::string aVersion) const -> std::optional<application::off_hunk_fact_key>
	{
		const auto& authority = Configuration.CredentialContext.Authority;
		const auto  product   = Configuration.Target.ServerContract == application::github_server_contract::github_com_2022_11_28 ? core::github_server_product::github_com
																																  : core::github_server_product::github_enterprise_server;
		auto        key       = application::off_hunk_fact_key{ .Scope              = { .Profile              = authority.Profile.value(),
																						.Host                 = std::string{ authority.Hostname.value() },
																						.Account              = authority.Account.value(),
																						.RepositoryDatabaseId = Configuration.Target.RepositoryDatabaseId },
																.Operation          = core::off_hunk_fact_operation::single_comment,
																.ProfileRevision    = authority.ProfileRevision.value(),
																.Product            = product,
																.ServerVersion      = std::move(aVersion),
																.ApiVersion         = 20221128U,
																.ClassifierRevision = off_hunk_classifier_revision };
		if (!valid(Configuration) || !application::valid(key)) {
			return std::nullopt;
		}
		return key;
	}

	auto review_mutation_session::lookup_off_hunk_key(const std::stop_token aStop) -> std::optional<application::off_hunk_fact_key>
	{
		auto* const facts = Recovery ? Recovery->off_hunk_facts() : nullptr;
		if (facts == nullptr) {
			return std::nullopt;
		}
		// GitHub Enterprise Server names its version; GitHub.com has none.
		if (Configuration.Target.ServerContract != application::github_server_contract::github_com_2022_11_28 && !ServerVersionAsked) {
			ServerVersionAsked = true;
			if (const auto request = compile_server_meta_request(Configuration.Target, Configuration.Target.ServerContract)) {
				if (const auto answer = perform(*request, aStop)) {
					ServerVersion = parse_server_meta(*answer);
				}
			}
		}
		const auto ghes = Configuration.Target.ServerContract != application::github_server_contract::github_com_2022_11_28;
		if (ghes && !ServerVersion) {
			return std::nullopt;
		}
		auto key = off_hunk_key(ghes ? *ServerVersion : std::string{ });
		if (key && !OtherServersForgotten) {
			OtherServersForgotten = facts->forget_other_servers(*key).has_value();
		}
		return key;
	}

	auto review_mutation_session::learned_file_comment(const application::review_mutation_request& aRequest, const std::stop_token aStop) -> std::optional<application::review_mutation_request>
	{
		if (!aRequest.Source || !aRequest.Source->CorePlacedOffHunk || aRequest.Source->Placement != core::review_placement::off_hunk || aRequest.Proposal.StartLine
			|| aRequest.Proposal.ReplyToThread || aRequest.Representation != representation::native_line || !aRequest.LiveBinding
			|| !compile_review_comment_request(aRequest, Configuration.Target.ServerContract)) {
			return std::nullopt;
		}
		const auto key = lookup_off_hunk_key(aStop);
		if (!key) {
			return std::nullopt;
		}
		const auto kept = Recovery->off_hunk_facts()->load(*key);
		if (!kept || !*kept
			|| core::current_off_hunk_fact(key->Product, (**kept).Observation, PublicationClock ? PublicationClock() : utc_now_ms()) != core::off_hunk_creation_capability::unsupported) {
			return std::nullopt;
		}
		// A fresh attempt only: a publication already under way keeps its course.
		const auto loaded = Recovery->load_publication(digest_hex(publication_fingerprint(aRequest, Configuration)));
		if (!loaded || (*loaded && (**loaded).State != publication_state::rejected)) {
			return std::nullopt;
		}
		auto file                      = aRequest;
		file.Source->Placement         = core::review_placement::unresolvable_line;
		file.Source->CorePlacedOffHunk = false;
		return file;
	}

	void review_mutation_session::learn_off_hunk(const application::review_mutation_request& aRequest, const review_mutation_wire_response& aResponse, const core::off_hunk_answer aAnswer)
	{
		auto* const facts = Recovery ? Recovery->off_hunk_facts() : nullptr;
		const auto  now   = PublicationClock ? PublicationClock() : utc_now_ms();
		const auto  fact  = core::learn_off_hunk_fact(aAnswer, now);
		if (facts == nullptr || !fact || !aRequest.LiveBinding || !aRequest.Source || !aRequest.Source->CorePlacedOffHunk) {
			return;
		}
		// Evidence is kept under the version the answer names; an answer from
		// GitHub Enterprise Server naming none keeps nothing.
		const auto ghes  = Configuration.Target.ServerContract != application::github_server_contract::github_com_2022_11_28;
		const auto named = ghes ? enterprise_version_header(aResponse) : std::optional<std::string>{ std::string{ } };
		if (!named) {
			return;
		}
		const auto key = off_hunk_key(*named);
		if (!key) {
			return;
		}
		if (ghes && ServerVersion != named) {
			ServerVersion         = named;
			OtherServersForgotten = facts->forget_other_servers(*key).has_value();
		}
		const auto& binding = *aRequest.LiveBinding;
		static_cast<void>(facts->record(*key, { .Observation = *fact, .Repository = binding.Target.Repository.Owner + "/" + binding.Target.Repository.Name, .PullRequest = binding.Target.Number.value(), .Path = binding.Anchor.Path, .Line = binding.Anchor.Semantic.Line }));
	}
}
