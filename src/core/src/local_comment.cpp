#include "bpr/core/local_comment.hpp"

#include <algorithm>

namespace bpr::core {
	auto name(const comment_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case comment_state::local:     return "local";
		case comment_state::draft:     return "draft";
		case comment_state::published: return "published";
		}
		return "local";
	}

	auto state_of(const review_comment_draft& aDraft) noexcept -> comment_state
	{
		return aDraft.State == review_draft_state::published ? comment_state::published : comment_state::draft;
	}

	auto valid(const local_comment_anchor& aAnchor) noexcept -> bool
	{
		const auto& place = aAnchor.Place;
		if (place.File.has_value() != place.Comparison.has_value() || (place.File && (!place.File->valid() || !place.Comparison->valid()))) {
			return false;
		}
		if (place.Line && (!place.File || (place.Line->Blob && !place.Line->Blob->valid()))) {
			return false;
		}
		if (aAnchor.ReplyToThread
			&& (aAnchor.ReplyToThread->empty() || aAnchor.ReplyToThread->size() > maximum_local_reply_thread_bytes || aAnchor.ReplyToThread->find('\0') != std::string::npos)) {
			return false;
		}
		// A range is a new comment on one line's side, starting above it.
		return !aAnchor.StartLine || (place.Line && !aAnchor.ReplyToThread && *aAnchor.StartLine < place.Line->Line);
	}

	auto name(const keep_local_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case keep_local_refusal::held_by_review: return "held_by_review";
		case keep_local_refusal::publishing:     return "publishing";
		case keep_local_refusal::published:      return "published";
		case keep_local_refusal::discarded:      return "discarded";
		}
		return "published";
	}

	auto decide_keep_local(const keep_local_input& aInput) -> std::expected<local_comment_anchor, keep_local_refusal>
	{
		const auto& draft = aInput.Draft;
		switch (draft.State) {
		case review_draft_state::published: return std::unexpected(keep_local_refusal::published);
		case review_draft_state::discarded: return std::unexpected(keep_local_refusal::discarded);
		case review_draft_state::publishing:
			return std::unexpected(held_by_review(draft) ? keep_local_refusal::held_by_review : keep_local_refusal::publishing);
		case review_draft_state::drafting: break;
		}
		auto anchor             = local_comment_anchor{ };
		anchor.Place.File       = aInput.File;
		anchor.Place.Comparison = draft.Anchor.Comparison;
		anchor.ReplyToThread    = draft.ReplyToThread;
		// A reply bound to its thread alone has no line of its own.
		if (!thread_bound_anchor(draft.Anchor)) {
			anchor.Place.Line = note_line_anchor{ .Side = draft.Anchor.Side, .Line = draft.Anchor.Line, .Blob = aInput.ShownBlob };
			anchor.StartLine  = draft.ReplyToThread ? std::nullopt : draft.StartLine;
		}
		return anchor;
	}

	auto name(const make_draft_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case make_draft_refusal::select_a_line: return "select_a_line";
		case make_draft_refusal::draft_stands:  return "draft_stands";
		}
		return "select_a_line";
	}

	namespace {
		[[nodiscard]] auto unsent_with_words(const review_comment_draft& aDraft) noexcept -> bool
		{
			return (aDraft.State == review_draft_state::drafting || aDraft.State == review_draft_state::publishing) && !aDraft.Body.empty();
		}
	} // namespace

	auto decide_make_draft(const make_draft_input& aInput) -> std::expected<make_draft_target, make_draft_refusal>
	{
		const auto& anchor = aInput.Anchor;
		// A Local reply becomes a reply to its thread, bound to it, in the
		// comparison it was written in.
		if (anchor.ReplyToThread) {
			const auto comparison = anchor.Place.Comparison ? anchor.Place.Comparison : aInput.Shown;
			if (!comparison) {
				return std::unexpected(make_draft_refusal::select_a_line);
			}
			const auto stands = std::ranges::any_of(aInput.Drafts, [&anchor](const review_comment_draft& aDraft) { return aDraft.ReplyToThread == anchor.ReplyToThread && unsent_with_words(aDraft); });
			if (stands) {
				return std::unexpected(make_draft_refusal::draft_stands);
			}
			return make_draft_target{ .Anchor = thread_reply_anchor(*comparison), .StartLine = std::nullopt, .ReplyToThread = anchor.ReplyToThread, .Reanchored = false };
		}
		auto target = std::optional<make_draft_target>{ };
		if (aInput.Placement.Kind == note_placement_kind::placed && aInput.Placement.At) {
			target = make_draft_target{ .Anchor = *aInput.Placement.At, .StartLine = anchor.StartLine, .ReplyToThread = std::nullopt, .Reanchored = false };
			// A range keeps its length where its last line is placed.
			if (target->StartLine && anchor.Place.Line) {
				const auto length = anchor.Place.Line->Line - *anchor.StartLine;
				target->StartLine = length <= target->Anchor.Line ? std::optional{ target->Anchor.Line - length } : std::nullopt;
			}
		} else if (aInput.Selected) {
			target = make_draft_target{ .Anchor = *aInput.Selected, .StartLine = std::nullopt, .ReplyToThread = std::nullopt, .Reanchored = true };
		}
		if (!target) {
			return std::unexpected(make_draft_refusal::select_a_line);
		}
		const auto stands = std::ranges::any_of(aInput.Drafts, [&target](const review_comment_draft& aDraft) { return !aDraft.ReplyToThread && aDraft.Anchor == target->Anchor && unsent_with_words(aDraft); });
		if (stands) {
			return std::unexpected(make_draft_refusal::draft_stands);
		}
		return *target;
	}
} // namespace bpr::core
