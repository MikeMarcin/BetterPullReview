#ifndef BPR_SRC_ADAPTERS_SYNTAX_INCLUDE_BPR_ADAPTERS_SYNTAX_PACKAGED_SYNTAX_CLASSIFIER_HPP
#define BPR_SRC_ADAPTERS_SYNTAX_INCLUDE_BPR_ADAPTERS_SYNTAX_PACKAGED_SYNTAX_CLASSIFIER_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/epoch_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace bpr::adapters::syntax {
	/// The stable definition identifier used when no packaged definition claims
	/// a path. It is deliberately non-empty so a plain-text blob still produces
	/// a valid `application::token_span_cache_key`.
	inline constexpr std::string_view plain_text_definition_name = "None";

	/// The largest blob this adapter will classify. A blob above the bound is
	/// refused with `limit_exceeded` rather than partially classified, because a
	/// truncated span list would silently unclassify the tail of a file.
	inline constexpr std::size_t maximum_classified_blob_bytes = 8U * 1024U * 1024U;

	/// The revision of the post-pass the C and C++ definitions run over the
	/// grammar's own runs: functions, types, macros and literal prefixes in a
	/// normal-styled run (BPR-0726), and in revision 2 the macro arguments of a
	/// preprocessor run together with the string role of an `#include` header
	/// name (BPR-0744). Revision 3 recognizes conditional macro identifiers
	/// independently of capitalization (BPR-0748). Revision 4 paints the `//`
	/// comment after a null directive as a comment rather than an error
	/// (BPR-0747). Revision 5 maps explicit C-family language type tokens to
	/// built-in while preserving typedef-style DataType spans, and adds the
	/// Windows Resource identity backed by packaged C with resource keywords
	/// (BPR-0930). It is folded into those definitions' published version,
	/// so raising it retires every span set cached under the previous
	/// heuristics.
	inline constexpr std::uint32_t identifier_heuristic_revision = 5U;

	/// C++ alone additionally recognizes scoped namespace/enum context. C and
	/// Windows Resource retain their existing rules and cache identities.
	/// Revision 7 closes RFC-BPR-0081's confirmed gaps: inline and attributed
	/// namespace heads, enum comparisons, X-macros in enum bodies, enums opened
	/// in both conditional branches, brace-initialized shadows, non-ASCII
	/// names and transparent `extern "C"` blocks (BPR-1474).
	inline constexpr std::uint32_t cpp_identifier_heuristic_revision = 7U;

	/// \brief Supplies the complete bytes of one blob.
	///
	/// \details The classifier reads no repository and runs no process; the
	/// composition supplies the bytes that a blob identity names. Returning no
	/// value means the blob is not available, which the classifier reports as
	/// `unavailable` rather than as an empty classification.
	class blob_source
	{
	public:
		virtual ~blob_source() = default;

		[[nodiscard]] virtual auto blob_bytes(const core::git_blob_oid& aBlob)
			-> std::optional<std::string> = 0;
	};

	/// \brief Owns the packaged syntax definitions.
	///
	/// \details Definitions come from the resources compiled into the packaged
	/// KSyntaxHighlighting build. The catalog adds no search path, so nothing on
	/// the host filesystem and nothing downloaded can contribute a definition;
	/// `filesystem_definitions()` and `custom_search_paths()` publish that fact
	/// so a test can assert it rather than trust it.
	///
	/// One catalog is moderately expensive to build and cheap to share. It is
	/// immutable after construction and its lookups are const.
	class packaged_definition_catalog final
	{
	public:
		packaged_definition_catalog();
		~packaged_definition_catalog();

		packaged_definition_catalog(const packaged_definition_catalog&) = delete;
		auto operator=(const packaged_definition_catalog&)
			-> packaged_definition_catalog&                        = delete;
		packaged_definition_catalog(packaged_definition_catalog&&) = delete;
		auto operator=(packaged_definition_catalog&&)
			-> packaged_definition_catalog& = delete;

		/// \brief Selects the definition that classifies the supplied path.
		///
		/// \details The selection uses the file name alone, so a path outside the
		/// repository cannot influence it. A path no definition claims resolves to
		/// the plain-text record, which is a decision rather than a failure.
		/// `.rc` (case-insensitive) selects the stable `Windows Resource` identity,
		/// backed by packaged C and the versioned resource keyword post-pass.
		[[nodiscard]] auto definition_for_path(std::string_view aPath) const
			-> application::syntax_definition_version;

		/// \brief Resolves a definition identifier to its current revision.
		///
		/// \returns The canonical record, or no value when this build packages no
		/// definition of that name.
		[[nodiscard]] auto definition_named(std::string_view aName) const
			-> std::optional<application::syntax_definition_version>;

		[[nodiscard]] auto packaged_definitions() const noexcept -> std::size_t;
		/// Definitions loaded from the host filesystem rather than the packaged
		/// resource. Zero is the contract, not an observation.
		[[nodiscard]] auto filesystem_definitions() const noexcept -> std::size_t;
		[[nodiscard]] auto custom_search_paths() const noexcept -> std::size_t;

	private:
		friend class packaged_syntax_classifier;

		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};

	/// \brief Names one contiguous line range of a blob.
	///
	/// \details `First` is a zero-based line of the complete blob. An empty range
	/// selects nothing.
	struct blob_line_range
	{
		std::uint32_t First = 0U;
		std::uint32_t Count = 0U;

		auto operator<=>(const blob_line_range&) const = default;
	};

	/// \brief Classifies complete blobs with the packaged definitions.
	///
	/// \details Every classification is a single pass over the complete blob,
	/// carrying the highlighter's line-end state from each line into the next.
	/// That is what makes a hunk beginning inside a multiline comment or string
	/// classify the way the file reads: the state that opened the construct lives
	/// on lines the hunk does not contain, so a hunk-local pass cannot recover it.
	///
	/// The classifier holds no theme, no palette, and no font. It maps the
	/// packaged text styles onto the core's closed role vocabulary and names no
	/// colour, so an appearance change cannot invalidate a classification.
	class packaged_syntax_classifier final : public application::syntax_classification_port
	{
	public:
		packaged_syntax_classifier(
			const packaged_definition_catalog& aCatalog,
			blob_source&                       aBlobs);
		~packaged_syntax_classifier() override;

		packaged_syntax_classifier(const packaged_syntax_classifier&) = delete;
		auto operator=(const packaged_syntax_classifier&)
			-> packaged_syntax_classifier&                       = delete;
		packaged_syntax_classifier(packaged_syntax_classifier&&) = delete;
		auto operator=(packaged_syntax_classifier&&)
			-> packaged_syntax_classifier& = delete;

		/// \brief Classifies the complete blob the key names.
		[[nodiscard]] auto classify(
			const application::syntax_classification_request& aRequest,
			std::stop_token                                   aStop)
			-> application::syntax_classification override;

		/// \brief Returns the spans of one hunk's line range.
		///
		/// \details The answer is the complete-blob classification restricted to
		/// the range, never a classification of the range alone. Span line numbers
		/// stay blob-absolute so a caller cannot lose track of which file line a
		/// span belongs to, and a span is either wholly inside the range or
		/// absent, because a span never crosses a line boundary.
		[[nodiscard]] auto classify_hunk(
			const application::syntax_classification_request& aRequest,
			blob_line_range                                   aLines,
			std::stop_token                                   aStop)
			-> application::syntax_classification;

		/// \brief Counts the complete-blob lexical passes this classifier ran.
		///
		/// \details This is the adapter-side reuse evidence. Nothing about a
		/// theme, palette, or font reaches this class, so the count cannot move
		/// when appearance changes; a test that switches appearance and sees the
		/// count hold has measured that directly.
		[[nodiscard]] auto lexical_passes() const noexcept -> std::size_t;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::adapters::syntax

#endif // BPR_SRC_ADAPTERS_SYNTAX_INCLUDE_BPR_ADAPTERS_SYNTAX_PACKAGED_SYNTAX_CLASSIFIER_HPP
