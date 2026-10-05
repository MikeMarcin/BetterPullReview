#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_CONVENTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_CONVENTION_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

/// \file
/// \brief The Conventional Comments catalog and the bounded prefix recognizer
/// (RFC-BPR-0047, grammar revision 1 of the pack's `specs/GRAMMAR.md`).
/// \details The recognizer reads the start of one user body and reports what
/// it found as byte spans into that exact body and typed facts. It never
/// rewrites, normalizes or reconstructs the Markdown: every status, including
/// `malformed` and `limited`, leaves the original body as the readable text.
/// A recognized label is public intent only. Nothing here resolves a thread,
/// classifies a defect, chooses a review verdict or performs any effect.
namespace bpr::core {
	/// \brief The twelve stable label IDs, in catalog order.
	enum class feedback_label : std::uint8_t {
		praise,
		nitpick,
		suggestion,
		issue,
		todo,
		question,
		thought,
		chore,
		note,
		typo,
		polish,
		quibble,
	};
	inline constexpr std::size_t feedback_label_count = 12U;

	/// \brief The built-in convention's advice about a label. It is BPR
	/// product policy, separate from any decoration the author wrote, and is
	/// never presented as an explicit `(blocking)` or `(non-blocking)`.
	enum class feedback_label_guidance : std::uint8_t {
		positive,
		non_blocking_by_nature,
		unspecified,
		necessary_change,
		required_before_acceptance,
		always_non_blocking,
	};

	/// \brief One catalog entry. Every label carries the same fields, so no
	/// label has a capability another lacks.
	struct feedback_label_definition
	{
		feedback_label          Label;
		std::string_view        Id;
		std::string_view        Description;
		feedback_label_guidance Guidance;
		std::string_view        IconRole;
	};

	/// \brief Revision of `bpr.feedback-label-catalog.v1` this catalog equals.
	inline constexpr std::uint32_t feedback_catalog_revision = 1U;

	/// \brief The complete catalog in its stable order.
	[[nodiscard]] auto feedback_label_catalog() noexcept -> std::span<const feedback_label_definition, feedback_label_count>;
	[[nodiscard]] auto definition_of(feedback_label aLabel) noexcept -> const feedback_label_definition&;
	/// \brief Exact catalog ID lookup, ASCII case-insensitive. No alias,
	/// abbreviation or plural matches: `nit` and `typos` are not labels.
	[[nodiscard]] auto find_feedback_label(std::string_view aRaw) noexcept -> std::optional<feedback_label>;
	[[nodiscard]] auto stable_name(feedback_label aLabel) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(feedback_label_guidance aGuidance) noexcept -> std::string_view;

	/// \brief Half-open UTF-8 byte span into the exact body revision that was
	/// recognized. It is not a Qt character index; an editor converts it.
	struct feedback_span
	{
		std::size_t Begin                                          = 0U;
		std::size_t End                                            = 0U;
		auto        operator==(const feedback_span&) const -> bool = default;
	};
	/// \brief The bytes a span names. An out-of-range span yields nothing.
	[[nodiscard]] auto feedback_text(std::string_view aBody, feedback_span aSpan) noexcept -> std::string_view;

	enum class feedback_prefix_status : std::uint8_t {
		recognized,
		/// No header shape at the start of the user body.
		plain,
		/// A well-formed header whose label is not in the catalog.
		unknown_label,
		/// A catalog label began a header whose syntax is broken.
		malformed,
		/// The classifier budget ran out before a decision.
		limited,
	};
	/// \brief What the author's own decorations declare. Only the three
	/// standard decorations contribute; custom ones never do.
	enum class feedback_expectation : std::uint8_t {
		unspecified,
		blocking,
		non_blocking,
		if_minor,
		conflicting,
	};
	enum class feedback_decoration_kind : std::uint8_t {
		custom,
		blocking,
		non_blocking,
		if_minor,
	};
	/// \brief The three header shapes revision 1 reads.
	enum class feedback_header_form : std::uint8_t {
		/// `label (decorations): subject`
		plain,
		/// `**label (decorations):** subject`
		strong_header,
		/// `**label** (decorations): subject`
		strong_label,
	};
	enum class feedback_diagnostic : std::uint8_t {
		/// More than one distinct standard expectation was declared.
		conflicting_decorations,
		duplicate_decoration,
		/// An explicit decoration contradicts the built-in guidance, such as
		/// `note (blocking)`. Advisory only.
		label_expectation_conflict,
		empty_subject,
		empty_decoration_list,
		empty_decoration,
		nested_decoration,
		unclosed_decoration,
		invalid_decoration,
		/// A decoration list not followed directly by the colon.
		missing_colon,
		/// The colon or strong markers are not followed by the separator the
		/// form requires.
		malformed_header,
		classifier_budget_exceeded,
		/// The caller had no validated user-body segment, so the body is read
		/// as ordinary text and not classified.
		classification_unavailable,
	};
	[[nodiscard]] auto stable_name(feedback_prefix_status aStatus) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(feedback_expectation aExpectation) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(feedback_diagnostic aDiagnostic) noexcept -> std::string_view;

	struct feedback_decoration
	{
		feedback_span            Raw;
		feedback_decoration_kind Kind = feedback_decoration_kind::custom;
	};

	/// \brief Grammar revision; a cache of annotations keys on it.
	inline constexpr std::uint32_t feedback_parser_revision = 1U;
	/// \brief The header, from the start of the user body through the first
	/// byte of its subject, must lie within this many bytes. Past it the body
	/// is `limited`: unclassified, never truncated.
	inline constexpr std::size_t maximum_feedback_prefix_bytes = 1024U;
	/// \brief A longer leading word is prose, not a label.
	inline constexpr std::size_t maximum_feedback_label_bytes = 32U;
	/// \brief More decorations than this make the body `limited`.
	inline constexpr std::size_t maximum_feedback_decorations = 32U;

	/// \brief Facts about one user body. Spans are offsets into the whole body
	/// passed in, even when only a segment of it was the user's.
	/// \details `Label` and `Guidance` are set only when `recognized`.
	/// `RawLabel` keeps the observed token whenever one was read, including
	/// for `unknown_label`, `malformed` and a `limited` scan. `Header`,
	/// `Subject` and `Decorations` are set for `recognized` and
	/// `unknown_label`; a malformed decoration list is never reported as an
	/// empty one. `Header` ends where `Subject` begins. `Discussion` runs from
	/// after the subject line's terminator to the end of the user body.
	struct feedback_annotation
	{
		feedback_prefix_status                 Status      = feedback_prefix_status::plain;
		std::optional<feedback_label>          Label       = std::nullopt;
		std::optional<feedback_label_guidance> Guidance    = std::nullopt;
		feedback_expectation                   Expectation = feedback_expectation::unspecified;
		feedback_header_form                   Form        = feedback_header_form::plain;
		std::optional<feedback_span>           UserBody    = std::nullopt;
		std::optional<feedback_span>           RawLabel    = std::nullopt;
		std::optional<feedback_span>           Header      = std::nullopt;
		std::optional<feedback_span>           Subject     = std::nullopt;
		std::optional<feedback_span>           Discussion  = std::nullopt;
		std::vector<feedback_decoration>       Decorations;
		std::vector<feedback_diagnostic>       Diagnostics;
		/// SHA-256 of every byte of the body passed in.
		std::array<std::uint8_t, 32> BodyDigest{ };
		std::uint32_t                ParserRevision = feedback_parser_revision;
		/// Bytes of the prefix window the classifier examined.
		std::size_t InspectedBytes = 0U;
	};

	/// \brief Recognize a header at the start of a whole ordinary body.
	/// \details Reads at most `maximum_feedback_prefix_bytes` to decide, then
	/// finds the end of the subject line. Linear, no backtracking, no effects.
	[[nodiscard]] auto recognize_feedback_prefix(std::string_view aBody) -> feedback_annotation;
	/// \brief Recognize a header at the start of a user-body segment that its
	/// owner (the tour-stop map, the permalink parser) already validated
	/// against these exact bytes.
	/// \details No segment, or one outside the body, means the owner could
	/// not find the user's text: the body stays ordinary text, `plain`, with
	/// `classification_unavailable`, and nothing is stripped by guesswork.
	[[nodiscard]] auto recognize_feedback_segment(std::string_view aBody, std::optional<feedback_span> aUserBody) -> feedback_annotation;

	/// \brief Which bytes of one body are the user's.
	/// \details An ordinary body is the user's from its first byte to its
	/// last. A managed envelope (a published tour stop, a permalink comment)
	/// is the user's only inside the span its owner validated, and not at all
	/// when the owner found none.
	struct feedback_body_segment
	{
		bool                         Managed                                                = false;
		std::optional<feedback_span> UserBody                                               = std::nullopt;
		auto                         operator==(const feedback_body_segment&) const -> bool = default;
	};
	/// \brief `recognize_feedback_prefix` for an ordinary body,
	/// `recognize_feedback_segment` for a managed one.
	[[nodiscard]] auto recognize_feedback_body(std::string_view aBody, const feedback_body_segment& aSegment) -> feedback_annotation;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_CONVENTION_HPP
