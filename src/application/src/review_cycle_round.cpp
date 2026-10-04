#include "bpr/application/review_cycle_round.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		auto failed(const cycle_store_failure aFailure) -> std::unexpected<cycle_store_error>
		{
			return std::unexpected(cycle_store_error{ .Failure = aFailure });
		}

		auto refused(const core::cycle_round_problem aProblem) -> cycle_round_freeze_result
		{
			return { .Refusal = core::cycle_round_refusal{ .Problem = aProblem } };
		}

		auto bytes_of(const std::string_view aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		/// The whole revision history of one note, page by page.
		auto history(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem, std::stop_token aStop)
			-> cycle_store_result<std::vector<core::cycle_feedback_revision>>
		{
			auto result = std::vector<core::cycle_feedback_revision>{ };
			auto after  = std::optional<core::private_review_turn_revision_id>{ };
			do {
				auto page = aStore.feedback_revisions(aSubject, aItem, after, aStop);
				if (!page) {
					return std::unexpected(page.error());
				}
				result.insert(result.end(), std::make_move_iterator(page->Revisions.begin()), std::make_move_iterator(page->Revisions.end()));
				after = page->Next;
			} while (after);
			return result;
		}

		/// The latest round frozen from `aDraft`, if any.
		auto latest_round_of(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft, std::stop_token aStop)
			-> cycle_store_result<std::optional<stored_cycle_round>>
		{
			auto result = std::optional<stored_cycle_round>{ };
			auto after  = std::optional<core::cycle_round_id>{ };
			do {
				auto page = aStore.rounds(aSubject, after, aStop);
				if (!page) {
					return std::unexpected(page.error());
				}
				for (const auto& round : page->Rounds) {
					if (round.Round.Draft == aDraft) {
						result = round;
					}
				}
				after = page->Next;
			} while (after);
			return result;
		}

		auto find_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_id aRound, std::stop_token aStop)
			-> cycle_store_result<std::optional<stored_cycle_round>>
		{
			if (!core::valid_cycle_id(aRound)) {
				return std::optional<stored_cycle_round>{ };
			}
			const auto before = aRound.value() == 1U ? std::nullopt : std::optional{ core::cycle_round_id{ aRound.value() - 1U } };
			auto       page   = aStore.rounds(aSubject, before, aStop);
			if (!page) {
				return std::unexpected(page.error());
			}
			if (page->Rounds.empty() || page->Rounds.front().Round.Id.Id != aRound) {
				return std::optional<stored_cycle_round>{ };
			}
			return std::optional{ page->Rounds.front() };
		}

		/// The capture of one revision, when this cycle holds it.
		auto capture_of(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_source_revision_id aRevision, std::stop_token aStop)
			-> cycle_store_result<std::optional<core::cycle_capture_observation>>
		{
			if (!core::valid_cycle_id(aRevision)) {
				return std::optional<core::cycle_capture_observation>{ };
			}
			const auto before = aRevision.value() == 1U ? std::nullopt : std::optional{ core::cycle_source_revision_id{ aRevision.value() - 1U } };
			auto       page   = aStore.revisions(aSubject, before, aStop);
			if (!page) {
				return std::unexpected(page.error());
			}
			if (page->Revisions.empty() || page->Revisions.front().Revision.Id.Id != aRevision) {
				return std::optional<core::cycle_capture_observation>{ };
			}
			return std::optional{ page->Revisions.front().Revision.Capture };
		}

		/// Each selected note's records as stored now. A note the store does
		/// not hold is left out, and the core names it.
		auto item_materials(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_selection& aSelection, std::stop_token aStop)
			-> cycle_store_result<std::vector<core::cycle_round_item_material>>
		{
			auto result = std::vector<core::cycle_round_item_material>{ };
			for (const auto& selected : aSelection.Items) {
				if (!core::valid_cycle_id(selected.Item)) {
					continue;
				}
				const auto ref    = core::cycle_feedback_item_ref{ .Cycle = aSubject.Cycle, .Id = selected.Item };
				auto       thread = aStore.load_feedback(aSubject, ref, aStop);
				if (!thread) {
					return std::unexpected(thread.error());
				}
				if (!*thread) {
					continue;
				}
				auto revisions = history(aStore, aSubject, ref, aStop);
				if (!revisions) {
					return std::unexpected(revisions.error());
				}
				auto suggestions = aStore.suggestions(aSubject, ref, aStop);
				if (!suggestions) {
					return std::unexpected(suggestions.error());
				}
				result.push_back({ .Thread = std::move(**thread), .History = std::move(*revisions), .Suggestions = std::move(*suggestions) });
			}
			return result;
		}

		/// The retained files selected context names. A file the revision
		/// does not hold readable is left out, and the core names it.
		auto context_files(review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_round_selection& aSelection, std::stop_token aStop)
			-> cycle_store_result<std::vector<core::cycle_round_file>>
		{
			auto result = std::vector<core::cycle_round_file>{ };
			for (const auto& selected : aSelection.Context) {
				const auto known = std::ranges::any_of(result, [&](const core::cycle_round_file& aFile) { return aFile.Revision == selected.Revision && aFile.Path == selected.Path; });
				if (known || !core::valid_cycle_id(selected.Revision)) {
					continue;
				}
				const auto pair = core::cycle_comparison_pair{ .Left = std::nullopt, .Right = { .Cycle = aSubject.Cycle, .Id = selected.Revision } };
				auto       file = aHost.file(aSubject, pair, selected.Path, { }, aStop);
				if (file.Status == review_material_status::cancelled) {
					return failed(cycle_store_failure::cancelled);
				}
				if (file.Status != review_material_status::available || !file.Path || !file.Path->Right.Content) {
					continue;
				}
				result.push_back({ .Revision = selected.Revision, .Path = selected.Path, .Blob = *file.Path->Right.Content, .Bytes = { file.Source.begin(), file.Source.end() } });
			}
			return result;
		}

		auto decoded_payload(review_cycle_store& aStore, const stored_cycle_round& aRound, std::stop_token aStop) -> cycle_store_result<core::cycle_decoded_round>
		{
			auto bytes = aStore.read_object(aRound.Payload, aStop);
			if (!bytes) {
				return std::unexpected(bytes.error());
			}
			auto decoded = core::decode_round_payload({ reinterpret_cast<const char*>(bytes->data()), bytes->size() });
			if (!decoded || decoded->Digest != aRound.Round.Digest) {
				return failed(cycle_store_failure::corrupt);
			}
			return std::move(*decoded);
		}
	}

	auto save_round_selection(review_cycle_store& aStore, const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft,
							  const std::uint64_t aExpectedRevision, const core::cycle_round_selection& aSelection, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt>
	{
		const auto body = core::encode_round_selection(aSelection);
		if (!body) {
			return failed(cycle_store_failure::bounds);
		}
		return aStore.save_draft(aOperation, aSubject, aDraft, aExpectedRevision, bytes_of(*body), std::move(aStop));
	}

	auto load_round_selection(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft, std::stop_token aStop)
		-> cycle_store_result<cycle_round_draft_content>
	{
		auto stored = aStore.load_draft(aSubject, aDraft, std::move(aStop));
		if (!stored) {
			return std::unexpected(stored.error());
		}
		if (!*stored) {
			return failed(cycle_store_failure::not_found);
		}
		auto selection = core::decode_round_selection({ reinterpret_cast<const char*>((*stored)->Body.data()), (*stored)->Body.size() });
		if (!selection) {
			return failed(cycle_store_failure::corrupt);
		}
		return cycle_round_draft_content{ .Draft = (*stored)->Draft, .Selection = std::move(*selection) };
	}

	auto preview_cycle_round(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft,
							 std::stop_token aStop) -> cycle_store_result<cycle_round_preview>
	{
		auto cycle = aStore.load(aSubject, aStop);
		if (!cycle) {
			return std::unexpected(cycle.error());
		}
		if (!*cycle) {
			return failed(cycle_store_failure::not_found);
		}
		auto draft = load_round_selection(aStore, aSubject, aDraft, aStop);
		if (!draft) {
			return std::unexpected(draft.error());
		}
		const auto& selection = draft->Selection;
		auto        prior     = latest_round_of(aStore, aSubject, aDraft, aStop);
		auto        capture   = capture_of(aStore, aSubject, selection.Source, aStop);
		auto        items     = item_materials(aStore, aSubject, selection, aStop);
		auto        files     = context_files(aHost, aSubject, selection, aStop);
		if (!prior || !capture || !items || !files) {
			return std::unexpected(!prior ? prior.error() : !capture ? capture.error()
									   : !items                      ? items.error()
																	 : files.error());
		}
		auto preview = cycle_round_preview{ .Draft = draft->Draft, .Selection = selection, .Prior = *prior };
		// A draft revision already frozen keeps its round; otherwise the next one.
		const auto reuse = *prior && (*prior)->Round.DraftRevision == draft->Draft.Revision;
		preview.Round    = reuse ? (*prior)->Round.Id.Id : core::cycle_round_id{ (*cycle)->Next.Round };
		auto built       = core::assemble_round({ .Cycle = &**cycle, .Round = preview.Round, .SourceCapture = *capture, .Selection = &selection, .Items = *items, .Files = *files });
		if (!built) {
			preview.Refusal = built.error();
			return preview;
		}
		auto canonical = core::encode_round_payload(*built);
		if (!canonical) {
			preview.Refusal = core::cycle_round_refusal{ .Problem = core::cycle_round_problem::invalid_text };
			return preview;
		}
		const auto digest = core::round_digest(*canonical);
		if (reuse && (*prior)->Round.Digest != digest) {
			// The records changed under an unchanged draft revision: the draft
			// must be saved again before its new content can be frozen.
			preview.Refusal = core::cycle_round_refusal{ .Problem = core::cycle_round_problem::stale_draft };
			return preview;
		}
		if (reuse) {
			preview.Frozen = *prior;
		}
		preview.Content = core::cycle_decoded_round{ .Payload = std::move(*built), .Digest = digest, .Canonical = std::move(*canonical) };
		return preview;
	}

	auto freeze_cycle_round(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_round_freeze_request& aRequest,
							std::stop_token aStop) -> cycle_store_result<cycle_round_freeze_result>
	{
		auto preview = preview_cycle_round(aStore, aHost, aSubject, aRequest.Draft, aStop);
		if (!preview) {
			return std::unexpected(preview.error());
		}
		return freeze_cycle_round(aStore, aSubject, aRequest, std::move(*preview), std::move(aStop));
	}

	auto freeze_cycle_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const cycle_round_freeze_request& aRequest, cycle_round_preview aPreview,
							std::stop_token aStop) -> cycle_store_result<cycle_round_freeze_result>
	{
		if (const auto problem = core::freeze_readiness(aRequest.Editor, aPreview.Draft.Revision)) {
			return refused(*problem);
		}
		if (aRequest.ExpectedDraftRevision != aPreview.Draft.Revision) {
			return refused(core::cycle_round_problem::stale_draft);
		}
		if (aPreview.Refusal) {
			return cycle_round_freeze_result{ .Refusal = aPreview.Refusal };
		}
		auto& content = *aPreview.Content;
		auto  payload = aStore.install(bytes_of(content.Canonical), aStop);
		if (!payload) {
			return std::unexpected(payload.error());
		}
		auto receipt = aStore.freeze(aRequest.Operation, { .Subject = aSubject, .Draft = aRequest.Draft, .ExpectedDraftRevision = aRequest.ExpectedDraftRevision, .Payload = *payload, .Round = aPreview.Round }, aStop);
		if (!receipt) {
			return std::unexpected(receipt.error());
		}
		auto frozen = cycle_frozen_round{
			.Round    = { .Round = { .Id = { .Cycle = aSubject.Cycle, .Id = aPreview.Round }, .Draft = aRequest.Draft, .DraftRevision = aPreview.Draft.Revision, .Digest = content.Digest }, .Payload = *payload },
			.Content  = std::move(content),
			.Reused   = !receipt->Changed,
			.Replayed = receipt->Replayed,
		};
		return cycle_round_freeze_result{ .Frozen = std::move(frozen) };
	}

	auto read_cycle_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const core::cycle_round_id aRound, std::stop_token aStop)
		-> cycle_store_result<cycle_frozen_round>
	{
		auto found = find_round(aStore, aSubject, aRound, aStop);
		if (!found) {
			return std::unexpected(found.error());
		}
		if (!*found) {
			return failed(cycle_store_failure::not_found);
		}
		auto content = decoded_payload(aStore, **found, std::move(aStop));
		if (!content) {
			return std::unexpected(content.error());
		}
		return cycle_frozen_round{ .Round = **found, .Content = std::move(*content), .Reused = false, .Replayed = false };
	}
}
