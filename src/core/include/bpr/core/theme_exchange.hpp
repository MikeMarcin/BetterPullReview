#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_THEME_EXCHANGE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_THEME_EXCHANGE_HPP

#include "bpr/core/appearance_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief Names the first token of every theme document.
	///
	/// \details A document announces what it is before it says anything else, so
	/// a host can refuse an unrelated file without interpreting a single value.
	inline constexpr std::string_view theme_document_signature = "bpr-theme";

	/// \brief Names the only document version this build interprets.
	///
	/// \details A document of any other version is refused rather than read
	/// leniently: a later version may give an existing identifier a new meaning,
	/// and guessing at that is how a compatibility rule turns into a silent
	/// misreading.
	inline constexpr std::uint32_t theme_document_version = 1U;

	/// \brief Bounds an accepted document in bytes, inclusive.
	///
	/// \details A total document is well under two kilobytes, so this bound is
	/// pure headroom. It exists so that oversize is refused by measuring the
	/// input rather than by discovering exhaustion part way through parsing it.
	inline constexpr std::size_t theme_document_byte_limit = 16384U;

	/// \brief Bounds one rejection subject in bytes.
	///
	/// \details A rejection names what offended it, and the offending text comes
	/// from an untrusted document, so the quoted text is truncated to this bound
	/// and reduced to printable ASCII. A diagnostic must not become a way to
	/// carry a payload into whatever displays it.
	inline constexpr std::size_t theme_import_subject_limit = 64U;

	/// \brief Names why a theme document was refused.
	///
	/// \details Every member is a distinct refusal with a distinct cause, which
	/// is the point: a single catch-all reason would let an import that refuses
	/// every document look correct. There is deliberately no `other` member.
	enum class theme_import_reason : std::uint8_t {
		/// The input exceeds `theme_document_byte_limit`. Measured before the
		/// document is parsed, so this is reported alone.
		document_too_large,

		/// The document is not shaped like a theme document: a missing or
		/// unrecognised signature, a line that is not one identifier and one
		/// value, or a byte outside the format's printable-ASCII alphabet.
		malformed_document,

		/// The signature is present and the version is not
		/// `theme_document_version`.
		unsupported_version,

		/// A key outside the closed `appearance_role` vocabulary. The subject is
		/// the offending key.
		unknown_field,

		/// A key that appears more than once. Refused rather than resolved by
		/// precedence, because a silently overridden value is how a document
		/// hides what it actually sets.
		duplicate_field,

		/// A role of the closed vocabulary that the document never assigns. The
		/// subject is the missing role's stable identifier. A palette is total,
		/// so a partial document is refused rather than completed with defaults
		/// the core does not own. The one exception is a role for which
		/// `defaults_on_import` holds (the label tints, [[BPR-1083]]): it is
		/// imported as `unassigned_appearance_color`, which presentation reads
		/// as its built-in value.
		missing_field,

		/// A reference to content outside the document, such as a scheme-bearing
		/// address. Refused wherever it appears, including inside a key.
		external_reference,

		/// A directive that pulls in another document.
		include_directive,

		/// Style-sheet content, including block syntax the format does not have.
		style_payload,

		/// Executable or markup content, including a tag opening or an
		/// interpreter line.
		executable_payload,

		/// A value that is not `#rrggbb` or `#rrggbbaa`. The subject is the
		/// offending value.
		malformed_color,
	};

	/// \brief Describes one refused part of a theme document.
	///
	/// \details `Subject` quotes the offending identifier, value, or text,
	/// bounded by `theme_import_subject_limit` and reduced to printable ASCII;
	/// it is empty when the document as a whole is the subject. `Line` is
	/// one-based, and zero when no single line is responsible.
	struct theme_import_rejection
	{
		theme_import_reason Reason = theme_import_reason::malformed_document;
		std::string         Subject;
		std::size_t         Line = 0U;

		auto operator<=>(const theme_import_rejection&) const = default;
	};

	/// \brief Describes the outcome of one import.
	///
	/// \details `Palette` holds a value exactly when `Rejections` is empty, so a
	/// caller cannot read a partially trusted palette: an import either produces
	/// a total mapping over the closed vocabulary or it produces reasons.
	struct theme_import_result
	{
		std::optional<appearance_palette>   Palette;
		std::vector<theme_import_rejection> Rejections;
	};

	/// \brief Writes a palette as a theme document.
	///
	/// \details The document is the signature line followed by one line per role
	/// in vocabulary order, each an `appearance_role` stable identifier and an
	/// eight-digit `#rrggbbaa` value. Alpha is always written so that no reader
	/// has to assume one. The result is deterministic and imports back to an
	/// equal palette.
	[[nodiscard]] auto export_theme(const appearance_palette& aPalette) -> std::string;

	/// \brief Reads a theme document.
	///
	/// \details The core performs no file access; a host reads the bytes and
	/// passes them here. Checks run in a fixed order so the outcome is
	/// reproducible: the byte bound first and alone; then a scan for foreign
	/// content over the whole text, which reports the first occurrence of each
	/// of external references, include directives, style payloads, executable
	/// payloads, and alphabet violations, and stops before any key is
	/// interpreted; then the signature and version; then every malformed line,
	/// unknown key, duplicate key, and malformed value in line order; and only
	/// when all of those are clean, every role the document failed to assign.
	[[nodiscard]] auto import_theme(std::string_view aDocument) -> theme_import_result;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_THEME_EXCHANGE_HPP
