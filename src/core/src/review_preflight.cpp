#include "bpr/core/review_preflight.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <map>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto bytes_of(std::string_view aText) noexcept -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		void update_length(sha256_stream& aStream, std::size_t aSize) noexcept
		{
			std::array<std::uint8_t, 8> length{ };
			for (std::size_t index = 0U; index < length.size(); ++index) {
				length[index] = static_cast<std::uint8_t>((static_cast<std::uint64_t>(aSize) >> (8U * index)) & 0xFFU);
			}
			aStream.update(length);
		}

		/// The root post's annotation, when the set names exactly one root.
		[[nodiscard]] auto root_annotation(const feedback_annotation_set& aSet, std::string_view aConversation)
			-> const feedback_annotation*
		{
			const auto conversation = std::ranges::lower_bound(aSet.Conversations, aConversation, { }, [](const feedback_conversation& aEntry) -> std::string_view { return aEntry.Id; });
			if (conversation == aSet.Conversations.end() || conversation->Id != aConversation || !conversation->Root) {
				return nullptr;
			}
			const auto key  = std::pair<std::string_view, std::string_view>{ aConversation, *conversation->Root };
			const auto post = std::ranges::lower_bound(aSet.Posts, key, { }, [](const feedback_post& aPost) {
				return std::pair<std::string_view, std::string_view>{ aPost.Conversation, aPost.Id };
			});
			if (post == aSet.Posts.end() || post->Conversation != aConversation || post->Id != *conversation->Root || !post->Annotation) {
				return nullptr;
			}
			return &*post->Annotation;
		}

		/// What the author wrote. Grammar revision 1 reads decorations as an
		/// expectation on a catalog label only; a custom label declares none.
		[[nodiscard]] auto written_expectation(const feedback_annotation& aAnnotation) noexcept -> feedback_expectation
		{
			return aAnnotation.Status == feedback_prefix_status::recognized ? aAnnotation.Expectation : feedback_expectation::unspecified;
		}

		/// Guidance that asks for work, read only when nothing was written.
		[[nodiscard]] auto advisory(const std::optional<feedback_label> aLabel, const feedback_expectation aWritten)
			-> std::optional<feedback_label_guidance>
		{
			if (!aLabel || aWritten != feedback_expectation::unspecified) {
				return std::nullopt;
			}
			const auto guidance = definition_of(*aLabel).Guidance;
			if (guidance == feedback_label_guidance::necessary_change || guidance == feedback_label_guidance::required_before_acceptance) {
				return guidance;
			}
			return std::nullopt;
		}
	} // namespace

	auto stable_name(const review_preflight_author aAuthor) noexcept -> std::string_view
	{
		switch (aAuthor) {
		case review_preflight_author::submitter: return "submitter";
		case review_preflight_author::other:     return "other";
		case review_preflight_author::unknown:   return "unknown";
		}
		return "unknown";
	}

	auto stable_name(const review_preflight_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case review_preflight_refusal::none:                  return "none";
		case review_preflight_refusal::invalid_submitter:     return "invalid_submitter";
		case review_preflight_refusal::mismatched_population: return "mismatched_population";
		case review_preflight_refusal::invalid_identity:      return "invalid_identity";
		}
		return "invalid_identity";
	}

	auto stable_name(const review_preflight_change aChange) noexcept -> std::string_view
	{
		switch (aChange) {
		case review_preflight_change::draft:       return "draft";
		case review_preflight_change::account:     return "account";
		case review_preflight_change::target:      return "target";
		case review_preflight_change::head:        return "head";
		case review_preflight_change::summary:     return "summary";
		case review_preflight_change::disposition: return "disposition";
		case review_preflight_change::selection:   return "selection";
		}
		return "draft";
	}

	auto open_blocking(const review_preflight& aPreflight) noexcept -> std::size_t
	{
		return aPreflight.Blockers.size() + aPreflight.Pending.size();
	}

	auto uncertain(const review_preflight& aPreflight) noexcept -> std::size_t
	{
		const auto& doubt = aPreflight.Uncertain;
		return doubt.UnknownAuthor + doubt.Unclassified + doubt.ResolutionUnknown;
	}

	auto project_review_preflight(const std::string_view aSubmitter, const feedback_annotation_set& aAnnotations,
								  const feedback_population& aPopulation, const std::span<const review_preflight_authorship> aAuthors,
								  const std::span<const review_preflight_draft> aDrafts) -> review_preflight
	{
		if (aSubmitter.empty()) {
			return { .Refusal = review_preflight_refusal::invalid_submitter };
		}
		if (aPopulation.Scope != aAnnotations.Scope || aPopulation.Epoch != aAnnotations.Epoch) {
			return { .Refusal = review_preflight_refusal::mismatched_population };
		}
		auto authors = std::map<std::string_view, const std::optional<std::string>*>{ };
		for (const auto& entry : aAuthors) {
			if (entry.Conversation.empty() || (entry.RootAuthor && entry.RootAuthor->empty())) {
				return { .Refusal = review_preflight_refusal::invalid_identity };
			}
			const auto [found, inserted] = authors.try_emplace(entry.Conversation, &entry.RootAuthor);
			if (!inserted && *found->second != entry.RootAuthor) {
				return { .Refusal = review_preflight_refusal::invalid_identity };
			}
		}
		auto selected = std::map<std::string_view, const review_preflight_draft*>{ };
		for (const auto& draft : aDrafts) {
			if (draft.Id.empty()) {
				return { .Refusal = review_preflight_refusal::invalid_identity };
			}
			const auto [found, inserted] = selected.try_emplace(draft.Id, &draft);
			if (!inserted && *found->second != draft) {
				return { .Refusal = review_preflight_refusal::invalid_identity };
			}
		}

		auto  result        = review_preflight{ .Scope = aPopulation.Scope, .Epoch = aPopulation.Epoch };
		auto& doubt         = result.Uncertain;
		doubt.Collection    = aPopulation.Collection;
		auto       guidance = std::map<feedback_label, review_preflight_guidance>{ };
		const auto advise   = [&guidance](const feedback_label aLabel, const feedback_label_guidance aGuidance) -> review_preflight_guidance& {
			return guidance.try_emplace(aLabel, review_preflight_guidance{ .Label = aLabel, .Guidance = aGuidance }).first->second;
		};

		for (const auto& unit : aPopulation.Units) {
			// A resolved conversation is settled: it neither blocks nor doubts.
			if (unit.Resolution == feedback_resolution::resolved) {
				continue;
			}
			const auto  known      = classification_known(unit.Root);
			const auto* annotation = known ? root_annotation(aAnnotations, unit.Id) : nullptr;
			const auto  written    = annotation ? written_expectation(*annotation) : feedback_expectation::unspecified;
			const auto  blocking   = known && written == feedback_expectation::blocking;
			const auto  author     = authors.find(unit.Id);
			const auto  whose      = author == authors.end() || !*author->second ? review_preflight_author::unknown
				: **author->second == aSubmitter                                 ? review_preflight_author::submitter
																				 : review_preflight_author::other;
			switch (whose) {
			case review_preflight_author::submitter:
				if (!known) {
					++doubt.Unclassified;
				} else if (blocking && unit.Label) {
					result.Blockers.push_back({ .Conversation = unit.Id, .Label = *unit.Label, .Resolution = unit.Resolution });
					if (unit.Resolution == feedback_resolution::unknown) {
						++doubt.ResolutionUnknown;
					}
				} else if (const auto advice = advisory(unit.Label, written)) {
					++advise(*unit.Label, *advice).Published;
				}
				break;
			case review_preflight_author::other:
				if (blocking) {
					++result.Others;
				}
				break;
			case review_preflight_author::unknown:
				if (!known || blocking) {
					++doubt.UnknownAuthor;
				}
				break;
			}
		}

		for (const auto& [id, draft] : selected) {
			if (!draft->Selected) {
				continue;
			}
			const auto annotation = recognize_feedback_prefix(draft->Body);
			switch (annotation.Status) {
			case feedback_prefix_status::recognized:
				if (annotation.Expectation == feedback_expectation::blocking) {
					result.Pending.push_back({ .Draft = draft->Id, .Label = *annotation.Label });
				} else if (const auto advice = advisory(annotation.Label, annotation.Expectation)) {
					++advise(*annotation.Label, *advice).Pending;
				}
				break;
			case feedback_prefix_status::unknown_label:
			case feedback_prefix_status::plain:
				break;
			case feedback_prefix_status::malformed:
			case feedback_prefix_status::limited:
				++doubt.Unclassified;
				break;
			}
		}

		for (const auto& [label, entry] : guidance) {
			result.Guidance.push_back(entry);
		}
		result.Exact = doubt.Collection == feedback_collection::complete && uncertain(result) == 0U;
		return result;
	}

	auto prepare_review_decision(const review_submission_draft& aDraft, const std::span<const review_preflight_draft> aDrafts)
		-> std::optional<review_prepared_decision>
	{
		if (aDraft.State != review_submission_state::drafting || !valid(aDraft)) {
			return std::nullopt;
		}
		auto ordered = std::vector<const review_preflight_draft*>{ };
		for (const auto& draft : aDrafts) {
			if (draft.Selected) {
				ordered.push_back(&draft);
			}
		}
		std::ranges::sort(ordered, { }, [](const review_preflight_draft* aEntry) -> std::string_view { return aEntry->Id; });
		auto selection = sha256_stream{ };
		update_length(selection, ordered.size());
		for (const auto* draft : ordered) {
			update_length(selection, draft->Id.size());
			selection.update(bytes_of(draft->Id));
			update_length(selection, draft->Body.size());
			selection.update(bytes_of(draft->Body));
		}
		return review_prepared_decision{
			.Draft       = aDraft.Id,
			.Target      = aDraft.Target,
			.Account     = aDraft.ActingAccount,
			.Disposition = aDraft.Disposition,
			.Summary     = sha256(bytes_of(aDraft.Body)),
			.Selection   = selection.finish(),
		};
	}

	auto review_decision_changes(const review_prepared_decision& aPrepared, const review_prepared_decision& aCurrent)
		-> std::vector<review_preflight_change>
	{
		auto changes = std::vector<review_preflight_change>{ };
		if (aPrepared.Draft != aCurrent.Draft) {
			changes.push_back(review_preflight_change::draft);
		}
		if (aPrepared.Account != aCurrent.Account) {
			changes.push_back(review_preflight_change::account);
		}
		const auto& was = aPrepared.Target;
		const auto& now = aCurrent.Target;
		if (was.Host != now.Host || was.Repository != now.Repository || was.PullRequest != now.PullRequest) {
			changes.push_back(review_preflight_change::target);
		}
		if (was.ExpectedHead != now.ExpectedHead) {
			changes.push_back(review_preflight_change::head);
		}
		if (aPrepared.Summary != aCurrent.Summary) {
			changes.push_back(review_preflight_change::summary);
		}
		if (aPrepared.Disposition != aCurrent.Disposition) {
			changes.push_back(review_preflight_change::disposition);
		}
		if (aPrepared.Selection != aCurrent.Selection) {
			changes.push_back(review_preflight_change::selection);
		}
		return changes;
	}
}
