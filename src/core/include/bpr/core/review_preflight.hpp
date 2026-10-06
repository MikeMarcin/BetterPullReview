#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_PREFLIGHT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_PREFLIGHT_HPP

#include "bpr/core/feedback_partition.hpp"
#include "bpr/core/review_submission.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1085]]. The submitting reviewer's own feedback, read before a
/// native review decision (RFC-BPR-0048 R14, R15, R42).
/// \details The preflight is context and nothing else. It reads the shared
/// population the label counters use (`feedback_partition.hpp`), the root
/// classification of each conversation and the reviewer's selected unsent
/// comment drafts, and sorts what it finds into four groups that are never
/// merged: my own published, open, explicitly blocking feedback; my selected
/// pending drafts that are explicitly blocking; label guidance (a `todo`,
/// `typo` or `chore` with no written decoration), which is advice and never
/// a blocker; and what could not be established. Other reviewers' explicit
/// blockers are counted apart and are never attributed to the submitter. A
/// conversation whose author is not known is uncertainty, never mine and
/// never theirs.
///
/// Nothing here chooses, changes or sends a review disposition. No label,
/// resolution or refresh yields one: the projection has no disposition to
/// give. Comment, Approve and Request changes stay the reviewer's explicit
/// choice through the unchanged review submission draft.
///
/// A prepared decision binds what one explicit Submit gesture saw: the draft,
/// its target and account, the summary, the disposition and the selected
/// drafts. A change to any of those invalidates it; a change to anything it
/// did not bind (a resolution, another reviewer's label, my published
/// comment's label on the website, an unselected draft, a newer read of the
/// same revision) leaves it valid and only refreshes the context.
namespace bpr::core {
	/// \brief Whose feedback a conversation is, from its root post's author.
	enum class review_preflight_author : std::uint8_t {
		submitter,
		other,
		/// The provider named no author (a deleted account), or the root was
		/// not observed.
		unknown,
	};
	[[nodiscard]] auto stable_name(review_preflight_author aAuthor) noexcept -> std::string_view;

	/// \brief One conversation's root author, by the provider's exact identity.
	struct review_preflight_authorship
	{
		std::string Conversation;
		/// Absent when the provider names no author.
		std::optional<std::string> RootAuthor;
		auto                       operator==(const review_preflight_authorship&) const -> bool = default;
	};

	/// \brief One of the reviewer's own unsent comment drafts.
	struct review_preflight_draft
	{
		std::string Id;
		std::string Body;
		/// Chosen as context for this review. Only selected drafts are read,
		/// and only they bind a prepared decision.
		bool Selected                                                = false;
		auto operator==(const review_preflight_draft&) const -> bool = default;
	};

	/// \brief My published, open, explicitly `(blocking)` conversation.
	struct review_preflight_blocker
	{
		/// The conversation identity the annotation set and the population use,
		/// which links back to the exact item.
		std::string Conversation;
		/// Only a catalog label carries a written expectation (grammar
		/// revision 1), so a blocker always has one.
		feedback_label Label = feedback_label::issue;
		/// `unresolved`, `unknown` or `not_applicable`; never `resolved`.
		feedback_resolution Resolution                                                = feedback_resolution::unresolved;
		auto                operator==(const review_preflight_blocker&) const -> bool = default;
	};

	/// \brief My selected, unsent, explicitly `(blocking)` draft.
	struct review_preflight_pending
	{
		std::string    Draft;
		feedback_label Label                                                     = feedback_label::issue;
		auto           operator==(const review_preflight_pending&) const -> bool = default;
	};

	/// \brief One label whose built-in guidance asks for work, on my open
	/// conversations or selected drafts that carry no written decoration.
	/// It is advice, never an explicit blocking decoration.
	struct review_preflight_guidance
	{
		feedback_label          Label                                                      = feedback_label::todo;
		feedback_label_guidance Guidance                                                   = feedback_label_guidance::necessary_change;
		std::size_t             Published                                                  = 0U;
		std::size_t             Pending                                                    = 0U;
		auto                    operator==(const review_preflight_guidance&) const -> bool = default;
	};

	/// \brief What the preflight could not establish. Each count is a reason
	/// the groups above may be missing an item.
	struct review_preflight_uncertainty
	{
		/// How much of the provider population was read.
		feedback_collection Collection = feedback_collection::complete;
		/// Open conversations with no known author whose root is explicitly
		/// blocking or not classified: they could be mine.
		std::size_t UnknownAuthor = 0U;
		/// My open conversations and selected drafts whose label is not known
		/// (malformed, over budget, inside an unmapped envelope, a missing,
		/// deleted or ambiguous root).
		std::size_t Unclassified = 0U;
		/// My explicit blockers whose resolution the provider did not report.
		/// They are listed as blockers too, marked unknown.
		std::size_t ResolutionUnknown                                             = 0U;
		auto        operator==(const review_preflight_uncertainty&) const -> bool = default;
	};

	enum class review_preflight_refusal : std::uint8_t {
		none,
		/// No submitting identity.
		invalid_submitter,
		/// The population was not built from this exact annotation set.
		mismatched_population,
		/// An empty identity, or one conversation or draft given twice with
		/// different facts.
		invalid_identity,
	};
	[[nodiscard]] auto stable_name(review_preflight_refusal aRefusal) noexcept -> std::string_view;

	/// \brief The four groups, and other reviewers' blockers apart.
	/// \details `Blockers` follow the population's order (conversation
	/// identity), `Pending` the draft identity, `Guidance` the catalog. A
	/// resolved conversation appears nowhere. `Exact` holds only when the
	/// whole population was read and nothing is uncertain.
	struct review_preflight
	{
		review_preflight_refusal               Refusal = review_preflight_refusal::none;
		cached_review_key                      Scope;
		std::uint64_t                          Epoch = 0U;
		std::vector<review_preflight_blocker>  Blockers;
		std::vector<review_preflight_pending>  Pending;
		std::vector<review_preflight_guidance> Guidance;
		review_preflight_uncertainty           Uncertain;
		/// Other reviewers' open, explicitly blocking conversations. They may
		/// be inspected; they are never the submitter's.
		std::size_t Others                                            = 0U;
		bool        Exact                                             = false;
		auto        operator==(const review_preflight&) const -> bool = default;
	};
	/// \brief My open blocking items, published and pending.
	[[nodiscard]] auto open_blocking(const review_preflight& aPreflight) noexcept -> std::size_t;
	/// \brief Every uncertain item, as one count.
	[[nodiscard]] auto uncertain(const review_preflight& aPreflight) noexcept -> std::size_t;

	/// \brief Read one population, its annotation set and the selected drafts
	/// for the reviewer `aSubmitter` names, by the provider's exact identity.
	[[nodiscard]] auto project_review_preflight(std::string_view aSubmitter, const feedback_annotation_set& aAnnotations,
												const feedback_population& aPopulation, std::span<const review_preflight_authorship> aAuthors,
												std::span<const review_preflight_draft> aDrafts) -> review_preflight;

	/// \brief What one explicit Submit gesture saw.
	struct review_prepared_decision
	{
		review_submission_draft_id Draft;
		review_mutation_target     Target;
		github_account_id          Account;
		review_disposition         Disposition = review_disposition::comment;
		/// SHA-256 of the summary's bytes.
		std::array<std::uint8_t, 32> Summary{ };
		/// SHA-256 over the selected drafts, in identity order, each its
		/// identity and its body.
		std::array<std::uint8_t, 32> Selection{ };
		auto                         operator==(const review_prepared_decision&) const -> bool = default;
	};
	/// \brief Prepare only a valid draft still being written.
	[[nodiscard]] auto prepare_review_decision(const review_submission_draft& aDraft, std::span<const review_preflight_draft> aDrafts)
		-> std::optional<review_prepared_decision>;

	/// \brief What changed since a decision was prepared.
	enum class review_preflight_change : std::uint8_t {
		/// Another review draft.
		draft,
		account,
		/// Another host, repository or pull request.
		target,
		head,
		summary,
		disposition,
		selection,
	};
	[[nodiscard]] auto stable_name(review_preflight_change aChange) noexcept -> std::string_view;
	/// \brief Every change, in enum order. A prepared decision is still valid
	/// exactly when none is reported.
	[[nodiscard]] auto review_decision_changes(const review_prepared_decision& aPrepared, const review_prepared_decision& aCurrent)
		-> std::vector<review_preflight_change>;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_PREFLIGHT_HPP
