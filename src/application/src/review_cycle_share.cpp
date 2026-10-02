#include "bpr/application/review_cycle_share.hpp"

#include "bpr/application/review_cycle_verification.hpp"
#include "bpr/core/review_cycle_bundle.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <iterator>
#include <span>
#include <utility>

namespace bpr::application {
	namespace {
		auto failed(const cycle_store_failure aFailure) -> std::unexpected<cycle_store_error>
		{
			return std::unexpected(cycle_store_error{ .Failure = aFailure });
		}

		auto bytes_of(const std::string_view aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		auto read_draft(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft,
						std::vector<core::cycle_next_round_item> aNotes, std::stop_token aStop) -> cycle_store_result<cycle_share_draft>
		{
			auto preview = preview_cycle_round(aStore, aHost, aSubject, aDraft, std::move(aStop));
			if (!preview) {
				return std::unexpected(preview.error());
			}
			auto draft = cycle_share_draft{ .Preview = std::move(*preview), .Notes = std::move(aNotes) };
			if (draft.Preview.Content) {
				auto text           = core::render_round_markdown(*draft.Preview.Content);
				draft.DocumentBytes = text.size();
				if (text.size() <= maximum_shown_round_document_bytes) {
					draft.Document = std::move(text);
				}
			}
			return draft;
		}

		/// What leaves by the route, or its full measure when over the route's budget.
		auto render(const core::cycle_decoded_round& aRound, const core::cycle_share_route aRoute, cycle_share_output& aOutput) -> bool
		{
			switch (aRoute) {
			case core::cycle_share_route::copy_document:
			case core::cycle_share_route::save_document: {
				const auto budget = aRoute == core::cycle_share_route::copy_document ? core::maximum_round_clipboard_bytes : core::maximum_round_document_file_bytes;
				auto       text   = core::render_round_markdown(aRound, budget);
				if (!text) {
					aOutput.Limit = text.error();
					return false;
				}
				aOutput.Bytes  = std::move(*text);
				aOutput.Format = core::cycle_round_file_kind::markdown;
				return true;
			}
			case core::cycle_share_route::save_bundle: {
				auto bundle = core::build_round_bundle(aRound);
				if (bundle.size() > core::maximum_bundle_archive_bytes) {
					aOutput.Limit = core::cycle_round_output_limit{
						.RequiredBytes = bundle.size(),
						.BudgetBytes   = core::maximum_bundle_archive_bytes,
						.Items         = aRound.Payload.Items.size(),
						.Sources       = aRound.Payload.Sources.size(),
					};
					return false;
				}
				aOutput.Bytes  = std::move(bundle);
				aOutput.Format = core::cycle_round_file_kind::bundle;
				return true;
			}
			case core::cycle_share_route::external_agent:
			case core::cycle_share_route::send_to_agent:  return true;
			}
			return true;
		}
	}

	auto open_share_draft(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_operation_id& aBegin,
						  const cycle_operation_id& aSave, std::stop_token aStop) -> cycle_store_result<cycle_share_draft>
	{
		auto cycle = aStore.load(aSubject, aStop);
		if (!cycle) {
			return std::unexpected(cycle.error());
		}
		if (!*cycle) {
			return failed(cycle_store_failure::not_found);
		}
		auto notes = current_note_items(aStore, aSubject, aStop);
		if (!notes) {
			return std::unexpected(notes.error());
		}
		const auto& state = **cycle;
		if (state.Next.RoundDraft > 1U) {
			// The latest draft, brought to what its notes say now.
			const auto draft  = core::cycle_round_draft_ref{ .Cycle = aSubject.Cycle, .Id = core::cycle_round_draft_id{ state.Next.RoundDraft - 1U } };
			auto       stored = load_round_selection(aStore, aSubject, draft, aStop);
			if (!stored) {
				return std::unexpected(stored.error());
			}
			if (const auto refreshed = core::refresh_composition(stored->Selection, *notes)) {
				if (auto saved = save_round_selection(aStore, aSave, aSubject, draft, stored->Draft.Revision, *refreshed, aStop); !saved) {
					return std::unexpected(saved.error());
				}
			}
			return read_draft(aStore, aHost, aSubject, draft, std::move(*notes), std::move(aStop));
		}
		// Every round has a draft, so with none this is the cycle's first.
		if (!state.Latest) {
			return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_target });
		}
		auto begun = aStore.begin_draft(aBegin, aSubject, state.Revision, aStop);
		if (!begun) {
			return std::unexpected(begun.error());
		}
		const auto draft = core::cycle_round_draft_ref{ .Cycle = aSubject.Cycle, .Id = core::cycle_round_draft_id{ begun->Allocated } };
		if (auto saved = save_round_selection(aStore, aSave, aSubject, draft, 1U, core::first_composition(*notes, state.Latest->Id.Id), aStop); !saved) {
			return std::unexpected(saved.error());
		}
		return read_draft(aStore, aHost, aSubject, draft, std::move(*notes), std::move(aStop));
	}

	auto edit_share_draft(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_operation_id& aSave,
						  const core::cycle_round_draft_ref& aDraft, const std::uint64_t aExpectedRevision, const core::cycle_compose_edit& aEdit, std::stop_token aStop)
		-> cycle_store_result<cycle_share_draft>
	{
		auto stored = load_round_selection(aStore, aSubject, aDraft, aStop);
		if (!stored) {
			return std::unexpected(stored.error());
		}
		if (stored->Draft.Revision != aExpectedRevision) {
			return failed(cycle_store_failure::conflict);
		}
		auto notes = current_note_items(aStore, aSubject, aStop);
		if (!notes) {
			return std::unexpected(notes.error());
		}
		if (const auto edited = core::edit_composition(stored->Selection, *notes, aEdit)) {
			if (auto saved = save_round_selection(aStore, aSave, aSubject, aDraft, aExpectedRevision, *edited, aStop); !saved) {
				return std::unexpected(saved.error());
			}
		}
		return read_draft(aStore, aHost, aSubject, aDraft, std::move(*notes), std::move(aStop));
	}

	auto prepare_share(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_share_request& aRequest,
					   std::stop_token aStop) -> cycle_store_result<cycle_share_output>
	{
		auto output  = cycle_share_output{ };
		auto preview = preview_cycle_round(aStore, aHost, aSubject, aRequest.Draft, aStop);
		if (!preview) {
			return std::unexpected(preview.error());
		}
		if (preview->Draft.Revision != aRequest.ExpectedDraftRevision) {
			output.Refusal = core::cycle_round_refusal{ .Problem = core::cycle_round_problem::stale_draft };
			return output;
		}
		if (preview->Refusal || !preview->Content) {
			output.Refusal = preview->Refusal.value_or(core::cycle_round_refusal{ });
			return output;
		}
		auto cycle = aStore.load(aSubject, aStop);
		if (!cycle) {
			return std::unexpected(cycle.error());
		}
		if (!*cycle) {
			return failed(cycle_store_failure::not_found);
		}
		// One decision for every channel: the same one any other exporter gets.
		const auto& content = *preview->Content;
		output.Decision     = core::authorize(aRequest.Policy, {
																   .Actor      = core::cycle_actor::reviewer,
																   .Capability = core::cycle_capability::export_round,
																   .Subject    = aSubject,
																   .Origin     = core::cycle_origin(**cycle),
																   .Round      = core::cycle_round_ref{ .Cycle = aSubject.Cycle, .Id = preview->Round },
																   .Digest     = content.Digest,
																   .Channel    = core::share_channel(aRequest.Route),
															   });
		if (output.Decision != core::cycle_authorization::allowed || !render(content, aRequest.Route, output)) {
			return output;
		}
		const auto freeze = cycle_round_freeze_request{
			.Operation             = aRequest.Freeze,
			.Draft                 = aRequest.Draft,
			.ExpectedDraftRevision = aRequest.ExpectedDraftRevision,
			.Editor                = { .ShownRevision = aRequest.ExpectedDraftRevision },
		};
		auto frozen = freeze_cycle_round(aStore, aHost, aSubject, freeze, std::move(aStop));
		if (!frozen) {
			return std::unexpected(frozen.error());
		}
		if (!frozen->Frozen) {
			output.Bytes.clear();
			output.Refusal = frozen->Refusal;
			return output;
		}
		// The effect lane runs one call at a time, so the frozen content is
		// the previewed content; a digest that differs is rendered again.
		if (frozen->Frozen->Content.Digest != content.Digest && !render(frozen->Frozen->Content, aRequest.Route, output)) {
			return output;
		}
		output.Round = std::move(*frozen->Frozen);
		return output;
	}

	auto share_to_file(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_share_request& aRequest,
					   cycle_export_destination& aDestination, const std::string& aName, const std::optional<core::cycle_material_digest>& aConfirmed, const cycle_operation_id& aRecord,
					   std::stop_token aStop) -> cycle_store_result<cycle_file_share>
	{
		auto prepared = prepare_share(aStore, aHost, aSubject, aRequest, aStop);
		if (!prepared) {
			return std::unexpected(prepared.error());
		}
		auto result = cycle_file_share{ .Output = std::move(*prepared) };
		if (!result.Output.Round) {
			return result;
		}
		auto saved = save_round_document(aDestination, bytes_of(result.Output.Bytes), aConfirmed, aStop);
		if (!saved) {
			result.Failure = saved.error();
			return result;
		}
		result.Save = *saved;
		if (saved->Action == core::cycle_round_save_action::conflict) {
			return result;
		}
		const auto observation = delivery_observation(*result.Output.Round, core::cycle_delivery_state::saved, core::cycle_disclosure_channel::local_file, result.Output.Bytes,
													  result.Output.Format, aName);
		auto       recorded    = aStore.record_delivery(aRecord, aSubject, observation, std::move(aStop));
		if (!recorded) {
			return std::unexpected(recorded.error());
		}
		result.Recorded = *recorded;
		return result;
	}

	auto delivery_observation(const cycle_frozen_round& aRound, const core::cycle_delivery_state aState, const core::cycle_disclosure_channel aChannel, const std::string_view aBytes,
							  const core::cycle_round_file_kind aFormat, std::string aDestination) -> core::cycle_delivery_observation
	{
		auto observation = core::cycle_delivery_observation{
			.Round       = aRound.Round.Round.Id,
			.Digest      = aRound.Round.Round.Digest,
			.State       = aState,
			.Channel     = aChannel,
			.Destination = std::move(aDestination),
		};
		if (!aBytes.empty()) {
			observation.Output = core::cycle_delivery_output{ .Format = aFormat, .Digest = core::cycle_material_digest{ core::sha256(bytes_of(aBytes)) }, .Bytes = aBytes.size() };
		}
		return observation;
	}

	auto all_deliveries(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<std::vector<core::cycle_delivery_record>>
	{
		auto records = std::vector<core::cycle_delivery_record>{ };
		auto after   = std::optional<std::uint64_t>{ };
		do {
			auto page = aStore.deliveries(aSubject, after, aStop);
			if (!page) {
				return std::unexpected(page.error());
			}
			std::ranges::move(page->Deliveries, std::back_inserter(records));
			after = page->Next;
		} while (after);
		return records;
	}
}
