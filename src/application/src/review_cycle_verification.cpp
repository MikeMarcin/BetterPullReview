#include "bpr/application/review_cycle_verification.hpp"

#include "bpr/application/review_cycle_feedback.hpp"
#include "bpr/application/review_cycle_response.hpp"
#include "bpr/application/review_cycle_round.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <utility>

namespace bpr::application {
	namespace {
		auto failed(const cycle_store_failure aFailure) -> std::unexpected<cycle_store_error>
		{
			return std::unexpected(cycle_store_error{ .Failure = aFailure });
		}

		auto material_failure(const review_material_status aStatus) -> cycle_store_failure
		{
			switch (aStatus) {
			case review_material_status::cancelled: return cycle_store_failure::cancelled;
			case review_material_status::missing:   return cycle_store_failure::object_unavailable;
			case review_material_status::corrupt:   return cycle_store_failure::corrupt;
			default:                                return cycle_store_failure::unavailable;
			}
		}

		auto load_cycle(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const std::stop_token& aStop) -> cycle_store_result<core::review_cycle_state>
		{
			auto loaded = aStore.load(aSubject, aStop);
			if (!loaded) {
				return std::unexpected(loaded.error());
			}
			if (!*loaded) {
				return failed(cycle_store_failure::not_found);
			}
			return std::move(**loaded);
		}

		auto all_threads(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const std::stop_token& aStop) -> cycle_store_result<std::vector<core::cycle_feedback_thread>>
		{
			auto threads = std::vector<core::cycle_feedback_thread>{ };
			auto after   = std::optional<core::cycle_feedback_item_id>{ };
			do {
				auto page = aStore.feedback(aSubject, after, aStop);
				if (!page) {
					return std::unexpected(page.error());
				}
				std::ranges::move(page->Threads, std::back_inserter(threads));
				after = page->Next;
			} while (after);
			return threads;
		}

		/// The latest revision of the note's first turn: the wording a round carries.
		auto current_wording(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem, const std::stop_token& aStop)
			-> cycle_store_result<core::private_review_turn_revision_id>
		{
			auto wording = std::optional<core::private_review_turn_revision_id>{ };
			auto after   = std::optional<core::private_review_turn_revision_id>{ };
			do {
				auto page = aStore.feedback_revisions(aSubject, aItem, after, aStop);
				if (!page) {
					return std::unexpected(page.error());
				}
				for (const auto& revision : page->Revisions) {
					if (revision.Identity.Position.Turn == core::private_review_turn_id{ 1 }) {
						wording = revision.Identity.Position.Revision;
					}
				}
				after = page->Next;
			} while (after);
			if (!wording) {
				return failed(cycle_store_failure::corrupt);
			}
			return *wording;
		}

		auto all_decisions(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const std::stop_token& aStop)
			-> cycle_store_result<std::vector<core::cycle_verification_record>>
		{
			auto records = std::vector<core::cycle_verification_record>{ };
			auto after   = std::optional<core::cycle_verification_id>{ };
			do {
				auto page = aStore.verifications(aSubject, after, aStop);
				if (!page) {
					return std::unexpected(page.error());
				}
				std::ranges::move(page->Verifications, std::back_inserter(records));
				after = page->Next;
			} while (after);
			return records;
		}

		/// The revision's manifest and the digest it was recorded under.
		struct checked_revision
		{
			std::shared_ptr<const core::cycle_manifest> Manifest;
			core::cycle_material_digest                 Digest;
		};
		auto revision_material(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_source_revision_id aRevision,
							   const std::stop_token& aStop) -> cycle_store_result<checked_revision>
		{
			if (!core::valid_cycle_id(aRevision)) {
				return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_target });
			}
			const auto before = aRevision.value() == 1U ? std::nullopt : std::optional{ core::cycle_source_revision_id{ aRevision.value() - 1U } };
			auto       page   = aStore.revisions(aSubject, before, aStop);
			if (!page) {
				return std::unexpected(page.error());
			}
			if (page->Revisions.empty() || page->Revisions.front().Revision.Id.Id != aRevision) {
				return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_target });
			}
			auto manifest = aHost.manifest(aSubject, aRevision, aStop);
			if (!manifest) {
				return failed(material_failure(manifest.error()));
			}
			return checked_revision{ .Manifest = std::move(*manifest), .Digest = page->Revisions.front().Revision.Capture.Material };
		}

		/// What each of a note's suggestions shows in the reconciled revision.
		auto reconcile_suggestions(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem,
								   const core::cycle_manifest& aAgainst, const std::stop_token& aStop) -> cycle_store_result<std::vector<core::cycle_suggestion_reconciliation>>
		{
			auto suggestions = aStore.suggestions(aSubject, aItem, aStop);
			if (!suggestions) {
				return std::unexpected(suggestions.error());
			}
			auto result = std::vector<core::cycle_suggestion_reconciliation>{ };
			for (const auto& suggestion : *suggestions) {
				auto seen = core::cycle_suggestion_reconciliation{ .Suggestion = suggestion.Id.Id };
				if (suggestion.State == core::cycle_suggestion_state::observed_already_applied) {
					seen.Observation = core::cycle_application_observation::already_applied;
				} else if (suggestion.Intent == core::cycle_suggestion_intent::exact_replacement) {
					// Only the exact whole-file result counts; similar text proves nothing.
					const auto* later = core::find_entry(aAgainst, suggestion.Path);
					if (later != nullptr && later->Content) {
						const auto original = preview_cycle_suggestion(aHost, aSubject, suggestion, aStop);
						if (original.Status == review_material_status::cancelled) {
							return failed(cycle_store_failure::cancelled);
						}
						if (original.Status == review_material_status::available) {
							seen.Observation = core::observe_application(suggestion, original.Before, *later->Content);
						}
					}
				}
				result.push_back(seen);
			}
			return result;
		}
	}

	auto record_item_decision(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_decision_request& aRequest,
							  const cycle_operation_id& aOperation, std::stop_token aStop) -> cycle_store_result<cycle_decision_result>
	{
		auto cycle = load_cycle(aStore, aSubject, aStop);
		if (!cycle) {
			return std::unexpected(cycle.error());
		}
		auto thread = aStore.load_feedback(aSubject, aRequest.Item, aStop);
		if (!thread) {
			return std::unexpected(thread.error());
		}
		if (!*thread) {
			return cycle_decision_result{ .Refusal = core::cycle_refusal::invalid_identity };
		}
		auto wording = current_wording(aStore, aSubject, aRequest.Item, aStop);
		if (!wording) {
			return std::unexpected(wording.error());
		}
		auto checked = revision_material(aStore, aHost, aSubject, aRequest.Checked, aStop);
		if (!checked) {
			if (checked.error().Failure == cycle_store_failure::refused) {
				return cycle_decision_result{ .Refusal = checked.error().Refusal };
			}
			return std::unexpected(checked.error());
		}
		auto record = core::cycle_verification_record{
			.Item     = aRequest.Item.Id,
			.Wording  = *wording,
			.Checked  = aRequest.Checked,
			.Material = core::verification_material(**thread, *checked->Manifest, checked->Digest),
			.Decision = aRequest.Decision,
		};
		if (const auto refusal = core::check_verification(*cycle, **thread, *wording, record)) {
			return cycle_decision_result{ .Refusal = refusal };
		}
		auto receipt = aStore.record_verification(aOperation, aSubject, record, aStop);
		if (!receipt) {
			return std::unexpected(receipt.error());
		}
		record.Id = { .Cycle = aSubject.Cycle, .Id = core::cycle_verification_id{ receipt->Allocated } };
		return cycle_decision_result{ .Receipt = *receipt, .Record = record };
	}

	auto reconcile_cycle_items(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_source_revision_id aAgainst,
							   std::stop_token aStop) -> cycle_store_result<std::vector<core::cycle_item_reconciliation>>
	{
		auto against = revision_material(aStore, aHost, aSubject, aAgainst, aStop);
		if (!against) {
			return std::unexpected(against.error());
		}
		auto threads = all_threads(aStore, aSubject, aStop);
		if (!threads) {
			return std::unexpected(threads.error());
		}
		auto decisions = all_decisions(aStore, aSubject, aStop);
		if (!decisions) {
			return std::unexpected(decisions.error());
		}
		auto responses = read_cycle_responses(aStore, aSubject, aStop);
		if (!responses) {
			return std::unexpected(responses.error());
		}
		auto result = std::vector<core::cycle_item_reconciliation>{ };
		for (const auto& thread : *threads) {
			auto wording = current_wording(aStore, aSubject, thread.Id, aStop);
			if (!wording) {
				return std::unexpected(wording.error());
			}
			const auto observed = observe_feedback_target(aHost, aSubject, thread.Primary, aAgainst, aStop);
			if (observed.Status == review_material_status::cancelled) {
				return failed(cycle_store_failure::cancelled);
			}
			auto suggestions = reconcile_suggestions(aStore, aHost, aSubject, thread.Id, *against->Manifest, aStop);
			if (!suggestions) {
				return std::unexpected(suggestions.error());
			}
			auto mine = std::vector<core::cycle_verification_record>{ };
			std::ranges::copy_if(*decisions, std::back_inserter(mine), [&](const core::cycle_verification_record& aRecord) { return aRecord.Item == thread.Id.Id; });
			const auto statements = core::item_statements(*responses, core::cycle_wire_name(thread.Id.Id));
			result.push_back(core::reconcile_item({
				.Thread      = &thread,
				.Wording     = *wording,
				.Against     = aAgainst,
				.Material    = core::verification_material(thread, *against->Manifest, against->Digest),
				.Primary     = observed.Correspondence,
				.Suggestions = *suggestions,
				.Statements  = statements,
				.Decisions   = mine,
			}));
		}
		return result;
	}

	auto current_note_items(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<std::vector<core::cycle_next_round_item>>
	{
		auto threads = all_threads(aStore, aSubject, aStop);
		if (!threads) {
			return std::unexpected(threads.error());
		}
		auto items = std::vector<core::cycle_next_round_item>{ };
		items.reserve(threads->size());
		for (const auto& thread : *threads) {
			auto wording = current_wording(aStore, aSubject, thread.Id, aStop);
			if (!wording) {
				return std::unexpected(wording.error());
			}
			auto suggestions = aStore.suggestions(aSubject, thread.Id, aStop);
			if (!suggestions) {
				return std::unexpected(suggestions.error());
			}
			auto pins = std::vector<core::cycle_round_suggestion_pin>{ };
			for (const auto& suggestion : *suggestions) {
				if (suggestion.State == core::cycle_suggestion_state::proposed) {
					pins.push_back({ .Id = suggestion.Id.Id, .Revision = suggestion.Revision });
				}
			}
			items.push_back({ .Item = thread.Id.Id, .Wording = *wording, .Lifecycle = thread.Lineage.Lifecycle, .Suggestions = std::move(pins) });
		}
		return items;
	}

	auto prepare_next_round(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_operation_id& aBegin,
							const cycle_operation_id& aSave, std::stop_token aStop) -> cycle_store_result<cycle_next_round_result>
	{
		auto cycle = load_cycle(aStore, aSubject, aStop);
		if (!cycle) {
			return std::unexpected(cycle.error());
		}
		// Every frozen round's assignment, oldest first; the latest is carried.
		auto assignments = std::vector<std::pair<core::cycle_round_id, core::cycle_round_selection>>{ };
		auto after       = std::optional<core::cycle_round_id>{ };
		do {
			auto page = aStore.rounds(aSubject, after, aStop);
			if (!page) {
				return std::unexpected(page.error());
			}
			for (const auto& round : page->Rounds) {
				auto frozen = read_cycle_round(aStore, aSubject, round.Round.Id.Id, aStop);
				if (!frozen) {
					return std::unexpected(frozen.error());
				}
				auto assignment = core::round_assignment(frozen->Content.Payload);
				if (!assignment) {
					return failed(cycle_store_failure::corrupt);
				}
				assignments.emplace_back(round.Round.Id.Id, std::move(*assignment));
			}
			after = page->Next;
		} while (after);
		auto result = cycle_next_round_result{ };
		if (assignments.empty() || !cycle->Latest) {
			return result;
		}
		const auto latest     = cycle->Latest->Id.Id;
		auto       reconciled = reconcile_cycle_items(aStore, aHost, aSubject, latest, aStop);
		if (!reconciled) {
			return std::unexpected(reconciled.error());
		}
		auto items = current_note_items(aStore, aSubject, aStop);
		if (!items) {
			return std::unexpected(items.error());
		}
		auto carried = std::set<core::cycle_feedback_item_id>{ };
		for (const auto& [round, assignment] : assignments) {
			for (const auto& item : assignment.Items) {
				carried.insert(item.Item);
			}
		}
		for (auto& item : *items) {
			const auto state = std::ranges::find(*reconciled, item.Item, &core::cycle_item_reconciliation::Item);
			item.InRound     = carried.contains(item.Item);
			item.Resolved    = state != reconciled->end() && core::resolved(*state);
		}
		result.From = assignments.back().first;
		result.Plan = core::plan_next_round(assignments.back().second, *items, latest);
		auto begun  = aStore.begin_draft(aBegin, aSubject, cycle->Revision, aStop);
		if (!begun) {
			return std::unexpected(begun.error());
		}
		const auto draft = core::cycle_round_draft_ref{ .Cycle = aSubject.Cycle, .Id = core::cycle_round_draft_id{ begun->Allocated } };
		auto       saved = save_round_selection(aStore, aSave, aSubject, draft, 1, result.Plan.Selection, aStop);
		if (!saved) {
			return std::unexpected(saved.error());
		}
		result.Draft = draft;
		return result;
	}
}
