#include "bpr/application/review_cycle_share.hpp"

#include "bpr/application/review_cycle_retrieval.hpp"
#include "bpr/application/review_cycle_verification.hpp"
#include "bpr/core/review_cycle_bundle.hpp"
#include "bpr/core/review_cycle_comparison.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
#include "bpr/core/review_cycle_send.hpp"
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
			auto policy = aStore.disclosure_policy(aStop);
			if (!policy) {
				return std::unexpected(policy.error());
			}
			auto preview = preview_cycle_round(aStore, aHost, aSubject, aDraft, std::move(aStop));
			if (!preview) {
				return std::unexpected(preview.error());
			}
			auto draft = cycle_share_draft{ .Preview = std::move(*preview), .Notes = std::move(aNotes), .Policy = *policy };
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
			case core::cycle_share_route::save_document:
			case core::cycle_share_route::send_to_agent: {
				// [[BPR-1180]]. Send hands over the document Copy and Save give, within its own bound.
				const auto budget = aRoute == core::cycle_share_route::copy_document ? core::maximum_round_clipboard_bytes
					: aRoute == core::cycle_share_route::send_to_agent               ? core::maximum_round_send_bytes
																					 : core::maximum_round_document_file_bytes;
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
			case core::cycle_share_route::external_agent: return true;
			}
			return true;
		}

		/// One decision for every channel: the same one any other exporter gets.
		auto export_decision(const core::cycle_disclosure_policy& aPolicy, const core::local_cycle_subject& aSubject, const core::review_cycle_state& aCycle,
							 const core::cycle_round_id aRound, const core::cycle_round_digest& aDigest, const core::cycle_disclosure_channel aChannel) -> core::cycle_authorization
		{
			return core::authorize(aPolicy, {
												.Actor      = core::cycle_actor::reviewer,
												.Capability = core::cycle_capability::export_round,
												.Subject    = aSubject,
												.Origin     = core::cycle_origin(aCycle),
												.Round      = core::cycle_round_ref{ .Cycle = aSubject.Cycle, .Id = aRound },
												.Digest     = aDigest,
												.Channel    = aChannel,
											});
		}

		/// Saves a rendered round to the chosen file and records the save; a
		/// refusal, a failure or a conflict records nothing.
		auto save_and_record(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, cycle_share_output aOutput, cycle_export_destination& aDestination,
							 const std::string& aName, const std::optional<core::cycle_material_digest>& aConfirmed, const cycle_operation_id& aRecord, std::stop_token aStop)
			-> cycle_store_result<cycle_file_share>
		{
			auto result = cycle_file_share{ .Output = std::move(aOutput) };
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
	}

	auto read_note_context(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_id aItem,
						   const std::size_t aContext, const std::size_t aRows, std::stop_token aStop) -> cycle_store_result<review_cycle_note_context>
	{
		const auto item   = core::cycle_feedback_item_ref{ .Cycle = aSubject.Cycle, .Id = aItem };
		auto       thread = aStore.load_feedback(aSubject, item, aStop);
		if (!thread) {
			return std::unexpected(thread.error());
		}
		if (!*thread) {
			return failed(cycle_store_failure::not_found);
		}
		auto       context = review_cycle_note_context{ .Item = aItem };
		const auto noted   = std::get_if<core::cycle_source_anchor>(&(*thread)->Primary);
		const auto whole   = std::get_if<core::cycle_file_target>(&(*thread)->Primary);
		if (noted == nullptr && whole == nullptr) {
			return context;
		}
		context.Path            = noted != nullptr ? noted->Path : whole->Path;
		context.Revision        = noted != nullptr ? noted->Revision : whole->Revision;
		const auto& blob        = noted != nullptr ? noted->Blob : whole->Blob;
		auto        suggestions = aStore.suggestions(aSubject, item, aStop);
		if (!suggestions) {
			return std::unexpected(suggestions.error());
		}
		// Only changes to these exact bytes are placed; the rest are counted.
		auto changes = std::vector<core::cycle_excerpt_change>{ };
		for (const auto& suggestion : *suggestions) {
			if (suggestion.Source == *context.Revision && suggestion.Path == context.Path && suggestion.OriginalBlob == blob) {
				changes.push_back({ .Start = suggestion.Start, .End = suggestion.End, .Original = suggestion.Original, .Replacement = suggestion.Replacement });
			} else {
				++context.Elsewhere;
			}
		}
		// The size the revision recorded decides before any byte is read.
		const auto manifest = aHost.manifest(aSubject, *context.Revision, aStop);
		if (!manifest) {
			if (manifest.error() == review_material_status::cancelled) {
				return failed(cycle_store_failure::cancelled);
			}
			context.Status = manifest.error();
			return context;
		}
		const auto side = core::compare_manifest_path(nullptr, **manifest, context.Path).Right;
		if (side.State != core::cycle_path_state::present || side.Content != blob) {
			context.Status = review_material_status::missing;
			return context;
		}
		if (side.Size > maximum_note_context_file_bytes) {
			context.Status = review_material_status::too_large;
			return context;
		}
		const auto pair = core::cycle_comparison_pair{ .Left = std::nullopt, .Right = { .Cycle = aSubject.Cycle, .Id = *context.Revision } };
		auto       file = aHost.file(aSubject, pair, context.Path, { }, aStop);
		if (file.Status == review_material_status::cancelled) {
			return failed(cycle_store_failure::cancelled);
		}
		context.Status = file.Status;
		if (file.Status != review_material_status::available) {
			return context;
		}
		const auto range   = noted != nullptr ? std::optional{ core::cycle_byte_range{ .Start = noted->Start, .End = noted->End } } : std::nullopt;
		auto       excerpt = core::excerpt_note(file.Source, range, changes, aContext, aRows);
		if (!excerpt) {
			context.Status = review_material_status::corrupt;
			return context;
		}
		context.Excerpt = std::move(*excerpt);
		return context;
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
		auto policy = effective_cycle_policy(aStore, aRequest.Policy, aStop);
		if (!policy) {
			return std::unexpected(policy.error());
		}
		const auto& content = *preview->Content;
		output.Decision     = export_decision(*policy, aSubject, **cycle, preview->Round, content.Digest, core::share_channel(aRequest.Route));
		if (output.Decision != core::cycle_authorization::allowed || !render(content, aRequest.Route, output)) {
			return output;
		}
		const auto freeze = cycle_round_freeze_request{
			.Operation             = aRequest.Freeze,
			.Draft                 = aRequest.Draft,
			.ExpectedDraftRevision = aRequest.ExpectedDraftRevision,
			.Editor                = { .ShownRevision = aRequest.ExpectedDraftRevision },
		};
		// [[BPR-1274]]. The effect lane runs one call at a time, so the round
		// is frozen from the content just previewed and rendered: assembled,
		// encoded and digested once.
		auto shown  = cycle_round_preview{ .Draft = preview->Draft, .Selection = preview->Selection };
		auto frozen = freeze_cycle_round(aStore, aSubject, freeze, std::move(*preview), aStop);
		if (!frozen) {
			return std::unexpected(frozen.error());
		}
		if (!frozen->Frozen) {
			output.Bytes.clear();
			output.Refusal = frozen->Refusal;
			return output;
		}
		output.Round = std::move(*frozen->Frozen);
		// The sheet then shows the round as frozen, as reading the draft back would.
		auto stored = aStore.disclosure_policy(std::move(aStop));
		if (!stored) {
			return std::unexpected(stored.error());
		}
		shown.Round   = output.Round->Round.Round.Id.Id;
		shown.Content = output.Round->Content;
		shown.Frozen  = output.Round->Round;
		shown.Prior   = output.Round->Round;
		auto text     = core::render_round_markdown(output.Round->Content);
		output.Shown  = cycle_share_draft{ .Preview = std::move(shown), .DocumentBytes = text.size(), .Policy = *stored };
		if (text.size() <= maximum_shown_round_document_bytes) {
			output.Shown->Document = std::move(text);
		}
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
		return save_and_record(aStore, aSubject, std::move(*prepared), aDestination, aName, aConfirmed, aRecord, std::move(aStop));
	}

	auto export_frozen_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const cycle_round_export_request& aRequest, std::stop_token aStop)
		-> cycle_store_result<cycle_share_output>
	{
		auto cycle = aStore.load(aSubject, aStop);
		if (!cycle) {
			return std::unexpected(cycle.error());
		}
		if (!*cycle) {
			return failed(cycle_store_failure::not_found);
		}
		auto round = read_cycle_round(aStore, aSubject, aRequest.Round, aStop);
		if (!round) {
			return std::unexpected(round.error());
		}
		auto policy = effective_cycle_policy(aStore, aRequest.Policy, aStop);
		if (!policy) {
			return std::unexpected(policy.error());
		}
		auto output     = cycle_share_output{ };
		output.Decision = export_decision(*policy, aSubject, **cycle, aRequest.Round, round->Round.Round.Digest, aRequest.Channel);
		// A saved document's budget for either destination; a bundle's for a bundle.
		const auto route = aRequest.Format == core::cycle_round_file_kind::bundle ? core::cycle_share_route::save_bundle : core::cycle_share_route::save_document;
		if (output.Decision != core::cycle_authorization::allowed || !render(round->Content, route, output)) {
			return output;
		}
		output.Round = std::move(*round);
		return output;
	}

	auto export_frozen_round_to_file(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const cycle_round_export_request& aRequest,
									 cycle_export_destination& aDestination, const std::string& aName, const std::optional<core::cycle_material_digest>& aConfirmed,
									 const cycle_operation_id& aRecord, std::stop_token aStop) -> cycle_store_result<cycle_file_share>
	{
		auto prepared = export_frozen_round(aStore, aSubject, aRequest, aStop);
		if (!prepared) {
			return std::unexpected(prepared.error());
		}
		return save_and_record(aStore, aSubject, std::move(*prepared), aDestination, aName, aConfirmed, aRecord, std::move(aStop));
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
