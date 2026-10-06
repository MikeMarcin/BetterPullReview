#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_HEADER_EDIT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_HEADER_EDIT_HPP

#include "bpr/core/feedback_convention.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/// \file
/// \brief Explicit Conventional Comments header edits, bound to the exact body
/// revision the author saw (RFC-BPR-0047 R05, R41; grammar revision 1).
/// \details A decision names one byte span of the old body and the bytes that
/// replace it. Everything outside that span, including the subject, the
/// discussion and any suggested-change fence, is copied unchanged, so line
/// endings and UTF-8 survive. The decision is pure: it neither reads nor
/// writes a draft. The caller applies `Body` through the existing revisioned
/// draft edit, whose own expected revision still guards against a newer
/// draft; an editor may instead apply `Replaced` and `Replacement` after
/// converting the byte span to its own character indexes.
namespace bpr::core {
	enum class feedback_label_change : std::uint8_t {
		/// Leave the label as written, including its spelling and case.
		keep,
		/// Write `Label` in canonical lowercase. Adding a label to a plain
		/// body inserts a header before its first line.
		set,
		/// Delete the whole header; the subject becomes the first line.
		remove,
	};

	/// \brief One decoration written into or taken out of the list, every
	/// other decoration kept exactly as authored ([[BPR-1083]] v004).
	/// \details Conventional Comments lets several decorations ride one
	/// label, as in `issue (non-blocking, if-minor):`, and an organisation may
	/// add its own, as in `(security)`. Writing a decoration that is already
	/// there changes nothing; a new one goes after the written ones, a
	/// standard one in its canonical lowercase spelling and a custom one as
	/// given. Writing `blocking` takes out `non-blocking`, and the reverse:
	/// a label carries one weight. `if-minor` and custom decorations combine
	/// with anything. Taking one out removes every copy of it, matched as the
	/// recognizer matches (standard by kind, custom ASCII case-insensitively).
	struct feedback_decoration_change
	{
		/// The decoration's text; `valid_feedback_decoration` must accept it.
		std::string Token;
		/// Write it (true) or take it out (false).
		bool Written = true;
	};
	/// \brief Whether a decoration can be written: 1 to
	/// `maximum_feedback_label_bytes` bytes of ASCII letters, digits, `-` and
	/// `_`, the token the recognizer reads inside the parentheses.
	[[nodiscard]] auto valid_feedback_decoration(std::string_view aToken) noexcept -> bool;

	/// \brief What the author asked to change.
	struct feedback_header_edit
	{
		feedback_label_change Change = feedback_label_change::keep;
		/// Read only when `Change` is `set`.
		feedback_label Label = feedback_label::issue;
		/// Absent keeps the standard decorations as written. `unspecified`
		/// removes them; `blocking`, `non_blocking` and `if_minor` leave
		/// exactly that one. `conflicting` is not something to write.
		/// Custom decorations are always kept, in order.
		std::optional<feedback_expectation> Expectation = std::nullopt;
		/// One decoration written or taken out, the rest kept as written.
		/// Not together with `Expectation`, and not with `remove`.
		std::optional<feedback_decoration_change> Decoration = std::nullopt;
	};

	struct feedback_header_edit_request
	{
		/// The current body.
		std::string_view      Body;
		feedback_body_segment Segment;
		/// The body revision the edit was chosen against: the `BodyDigest`
		/// of the annotation the author was shown.
		std::array<std::uint8_t, 32> ExpectedDigest{ };
		std::uint32_t                ExpectedParserRevision = feedback_parser_revision;
		feedback_header_edit         Edit;
	};

	enum class feedback_edit_refusal : std::uint8_t {
		none,
		/// The body or the parser changed since the edit was chosen. Nothing
		/// is applied; plan again from the current body.
		stale_revision,
		/// The request contradicts itself, such as `conflicting` as a target,
		/// decorations for a body without a header, or a decoration that
		/// cannot be written.
		invalid_edit,
		/// The header is malformed, over budget or inside an envelope whose
		/// user text is unknown. The author's text stays as written.
		unsupported_header,
		/// The body's first block cannot take a header without restructuring
		/// (a quote, list, heading, fence, HTML or table), or the result would
		/// not read back as the requested header. Nothing is deleted to make
		/// it fit.
		restructuring_required,
		/// The result would exceed the shared draft's admitted body bound.
		body_limit,
	};
	[[nodiscard]] auto stable_name(feedback_edit_refusal aRefusal) noexcept -> std::string_view;

	/// \brief The outcome of one explicit header edit.
	/// \details When `Changed`, `Body` is the old body with `Replaced`
	/// replaced by `Replacement`, and `Annotation` and `Segment` describe it.
	/// Otherwise `Body` is empty, `Replaced` is empty, and `Annotation` is
	/// the current body's. A no-op request is `none` and not `Changed`: the
	/// body is not rewritten to canonical form.
	struct feedback_header_edit_decision
	{
		feedback_edit_refusal Refusal = feedback_edit_refusal::none;
		bool                  Changed = false;
		/// Half-open UTF-8 byte span of the old body. Both ends lie on code
		/// point boundaries.
		feedback_span         Replaced;
		std::string           Replacement;
		std::string           Body;
		feedback_body_segment Segment;
		feedback_annotation   Annotation;
	};

	/// \brief Decide one explicit header edit against the exact current body.
	[[nodiscard]] auto decide_feedback_header_edit(const feedback_header_edit_request& aRequest) -> feedback_header_edit_decision;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_HEADER_EDIT_HPP
