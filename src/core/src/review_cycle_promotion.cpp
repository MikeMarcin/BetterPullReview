#include "bpr/core/review_cycle_promotion.hpp"

#include "bpr/core/git_objects.hpp"
#include "bpr/core/hex.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::core {
	namespace {
		/// A regular file, executable or not; a link or a submodule has no lines.
		constexpr auto regular_file    = std::uint32_t{ 0100644U };
		constexpr auto executable_file = std::uint32_t{ 0100755U };

		struct digest_writer
		{
			sha256_stream Stream;

			void u8(const std::uint8_t aValue)
			{
				Stream.update(std::span{ &aValue, 1U });
			}
			void u64(const std::uint64_t aValue)
			{
				for (auto shift = 56; shift >= 0; shift -= 8) {
					u8(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
				}
			}
			void bytes(const std::span<const std::uint8_t> aValue)
			{
				u64(aValue.size());
				Stream.update(aValue);
			}
			void text(const std::string_view aValue)
			{
				bytes(std::span{ reinterpret_cast<const std::uint8_t*>(aValue.data()), aValue.size() });
			}
		};

		auto hex(const std::span<const std::uint8_t> aBytes, const std::size_t aCount) -> std::string
		{
			return lower_hex(aBytes.first(std::min(aCount, aBytes.size())));
		}

		auto lower(std::string_view aText) -> std::string
		{
			auto result = std::string{ aText };
			std::ranges::transform(result, result.begin(), [](const char aCharacter) { return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter; });
			return result;
		}

		/// The request identity: this cycle (its catalog and number), this note
		/// and this pull request. A note is added to one pull request's tour
		/// once, whatever its wording; another cycle's note is another request.
		auto request_identity(const local_cycle_subject& aSubject, const cycle_feedback_item_ref& aItem, const cycle_pull_address& aPull) -> std::string
		{
			auto writer = digest_writer{ };
			writer.text("bpr-cycle-promotion-v1");
			writer.u64(aSubject.Authority.Profile.value());
			writer.bytes(aSubject.Authority.Incarnation.bytes());
			writer.u64(aItem.Cycle.value());
			writer.u64(aItem.Id.value());
			writer.text(lower(aPull.Host));
			writer.text(lower(aPull.Owner));
			writer.text(lower(aPull.Repository));
			writer.u64(aPull.Number);
			const auto digest = writer.Stream.finish();
			// 12 + 48 bytes: within the 64 a tour request receipt holds.
			return "cycle-note-" + hex(digest, 24U);
		}

		/// What the request asks for, so the same identity with other content
		/// is refused rather than answered as if it were the same request.
		auto request_digest(const std::string_view aRequest, const tour_stop& aStop) -> std::array<std::uint8_t, 32>
		{
			auto writer = digest_writer{ };
			writer.text(aRequest);
			writer.text(aStop.Title);
			writer.text(aStop.Body);
			writer.u8(aStop.Destination ? 1U : 0U);
			if (aStop.Destination) {
				const auto& source = *aStop.Destination;
				writer.bytes(source.Commit.bytes());
				writer.u8(source.Blob ? 1U : 0U);
				if (source.Blob) {
					writer.bytes(source.Blob->bytes());
				}
				writer.text(source.Path);
				writer.u8(static_cast<std::uint8_t>(source.Side));
				writer.u64(source.FirstLine);
				writer.u64(source.LastLine);
			}
			return writer.Stream.finish();
		}

		auto blank(const std::string_view aText) -> bool
		{
			return aText.find_first_not_of(" \t\r\n") == std::string_view::npos;
		}

		/// The anchor's lines in the published commit, when its bytes are proved.
		auto exact_lines(const cycle_source_anchor& aAnchor, const publication_plan& aPlan, const candidate_blob& aBlob, const std::span<const std::uint8_t> aFile)
			-> std::optional<tour_code_destination>
		{
			// A refused identity, collision detection included, proves nothing.
			const auto computed = git_blob_identity(aFile, aPlan.Format);
			const auto identity = computed.value_or(git_blob_oid{ });
			if (!identity.valid() || identity != retag<git_blob_oid_kind>(aBlob.Object) || aFile.size() != aBlob.Size || !verify_target(aAnchor, aFile)) {
				return std::nullopt;
			}
			const auto lines = project_lines(aFile, aAnchor.Start, aAnchor.End);
			if (!lines || lines->First == 0U || lines->Last < lines->First || lines->Last >= std::numeric_limits<std::uint32_t>::max()) {
				return std::nullopt;
			}
			auto destination = tour_code_destination{
				.Commit    = aPlan.Commit,
				.Blob      = identity,
				.Path      = aAnchor.Path,
				.Side      = diff_side::right,
				.FirstLine = static_cast<std::uint32_t>(lines->First - 1U),
				.LastLine  = static_cast<std::uint32_t>(lines->Last - 1U),
			};
			if (!valid(destination)) {
				return std::nullopt;
			}
			return destination;
		}

		/// Whether the plan writes exactly the anchor's bytes at its path.
		auto same_bytes(const cycle_source_anchor& aAnchor, const candidate_blob& aBlob) -> bool
		{
			return aBlob.Content == aAnchor.Blob && !aBlob.LineFeed && (aBlob.Mode == regular_file || aBlob.Mode == executable_file);
		}

		auto native_plan(const cycle_pull_association& aLink, const publication_plan* aPlan) -> const publication_plan*
		{
			return aLink.Origin == cycle_pull_origin::published && aLink.Publication && aPlan != nullptr ? aPlan : nullptr;
		}
	}

	auto name(const cycle_promotion_source aSource) noexcept -> std::string_view
	{
		switch (aSource) {
		case cycle_promotion_source::exact_lines:      return "exact_lines";
		case cycle_promotion_source::earlier_revision: return "earlier_revision";
		case cycle_promotion_source::other_bytes:      return "other_bytes";
		case cycle_promotion_source::outside_changes:  return "outside_changes";
		case cycle_promotion_source::unread:           return "unread";
		case cycle_promotion_source::whole_file:       return "whole_file";
		case cycle_promotion_source::folder:           return "folder";
		case cycle_promotion_source::whole_change:     return "whole_change";
		case cycle_promotion_source::linked_by_hand:   return "linked_by_hand";
		}
		return "unknown";
	}

	auto name(const cycle_promotion_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case cycle_promotion_refusal::empty:    return "empty";
		case cycle_promotion_refusal::bounds:   return "bounds";
		case cycle_promotion_refusal::mismatch: return "mismatch";
		}
		return "unknown";
	}

	auto published_blob(const publication_plan& aPlan, const std::string_view aPath) noexcept -> const candidate_blob*
	{
		const auto found = std::ranges::find(aPlan.Blobs, aPath, &candidate_blob::Path);
		return found != aPlan.Blobs.end() ? &*found : nullptr;
	}

	auto promotion_needs_file(const cycle_pull_association& aLink, const publication_plan* aPlan, const cycle_promotion_note& aNote) noexcept -> bool
	{
		const auto* plan   = native_plan(aLink, aPlan);
		const auto* anchor = std::get_if<cycle_source_anchor>(&aNote.Primary);
		if (plan == nullptr || anchor == nullptr || anchor->Revision != plan->Final) {
			return false;
		}
		const auto* blob = published_blob(*plan, anchor->Path);
		return blob != nullptr && same_bytes(*anchor, *blob);
	}

	auto promote_note(const local_cycle_subject& aSubject, const cycle_pull_association& aLink, const publication_plan* aPlan, const cycle_promotion_note& aNote,
					  const std::optional<std::span<const std::uint8_t>> aFile) -> std::expected<cycle_promotion, cycle_promotion_refusal>
	{
		if (blank(aNote.Body)) {
			return std::unexpected(cycle_promotion_refusal::empty);
		}
		if (aNote.Body.size() > maximum_tour_field_bytes || aNote.Body.find('\0') != std::string::npos) {
			return std::unexpected(cycle_promotion_refusal::bounds);
		}
		if (aNote.Item.Cycle != aSubject.Cycle || aLink.Address.Number == 0U) {
			return std::unexpected(cycle_promotion_refusal::mismatch);
		}
		const auto* plan = native_plan(aLink, aPlan);
		if (aLink.Origin == cycle_pull_origin::published && plan == nullptr) {
			return std::unexpected(cycle_promotion_refusal::mismatch);
		}

		auto promotion = cycle_promotion{ .Item = aNote.Item };
		auto title     = std::string{ };
		std::visit(
			[&](const auto& aTarget) {
				using target = std::decay_t<decltype(aTarget)>;
				if constexpr (std::is_same_v<target, cycle_whole_target>) {
					promotion.Source = cycle_promotion_source::whole_change;
					title            = "The whole change";
				} else if constexpr (std::is_same_v<target, cycle_module_target>) {
					promotion.Source = cycle_promotion_source::folder;
					title            = aTarget.Path.empty() ? std::string{ "The whole change" } : aTarget.Path + "/";
				} else if constexpr (std::is_same_v<target, cycle_file_target>) {
					promotion.Source = plan != nullptr ? cycle_promotion_source::whole_file : cycle_promotion_source::linked_by_hand;
					title            = aTarget.Path;
				} else {
					title = aTarget.Path;
					if (plan == nullptr) {
						promotion.Source = cycle_promotion_source::linked_by_hand;
						return;
					}
					if (aTarget.Revision != plan->Final) {
						promotion.Source = cycle_promotion_source::earlier_revision;
						return;
					}
					const auto* blob = published_blob(*plan, aTarget.Path);
					if (blob == nullptr) {
						const auto deleted = std::ranges::find(plan->Deleted, aTarget.Path) != plan->Deleted.end();
						promotion.Source   = deleted ? cycle_promotion_source::other_bytes : cycle_promotion_source::outside_changes;
						return;
					}
					if (!same_bytes(aTarget, *blob)) {
						promotion.Source = cycle_promotion_source::other_bytes;
						return;
					}
					auto lines = aFile ? exact_lines(aTarget, *plan, *blob, *aFile) : std::nullopt;
					if (!lines) {
						promotion.Source = cycle_promotion_source::unread;
						return;
					}
					promotion.Source           = cycle_promotion_source::exact_lines;
					promotion.Stop.Destination = std::move(lines);
				}
			},
			aNote.Primary);
		if (title.size() > maximum_tour_field_bytes) {
			return std::unexpected(cycle_promotion_refusal::bounds);
		}
		promotion.Stop.Title       = std::move(title);
		promotion.Stop.Body        = aNote.Body;
		promotion.Stop.Mapping     = tour_mapping::unavailable;
		promotion.Stop.Preparation = tour_preparation::editing;
		promotion.RequestId        = request_identity(aSubject, aNote.Item, aLink.Address);
		promotion.Digest           = request_digest(promotion.RequestId, promotion.Stop);
		return promotion;
	}

	auto promoted_destination(const tour_stop& aStop, const git_commit_oid& aHead, const git_commit_oid& aBase) -> tour_destination_plan
	{
		const auto source = publication_source(aStop);
		if (!source) {
			return { };
		}
		return plan_tour_destination(publication_observation(*source, aHead, aBase, aStop.Body, false, true));
	}
}
