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

	/// \brief Names the document version a theme for import is written in.
	///
	/// \details [[BPR-1479]]. Version 2 adds the `base` line, which names the
	/// built-in variant the palette was made against. A role the document
	/// predates is filled from that variant's built-in palette when it is
	/// imported, so an older theme keeps the colours its own base gives the new
	/// role rather than taking those of whichever theme is on screen (owner,
	/// 2026-10-13).
	///
	/// This version and `baseless_theme_document_version` are the only two this
	/// build interprets. A document of any other version is refused rather than
	/// read leniently: a later version may give an existing identifier a new
	/// meaning, and guessing at that is how a compatibility rule turns into a
	/// silent misreading.
	inline constexpr std::uint32_t theme_document_version = 2U;

	/// \brief Names the document version that carries no base.
	///
	/// \details Every theme exported before [[BPR-1479]] is one, and the
	/// appearance store still writes one per variant slot, because the slot is
	/// that palette's base. A version-1 document has no `base` key; an import
	/// infers its base from the document's own code background.
	inline constexpr std::uint32_t baseless_theme_document_version = 1U;

	/// \brief Names the key of the line that declares a version-2 base.
	///
	/// \details Its value is an `appearance_variant` stable identifier. It is
	/// not an `appearance_role`, so it cannot collide with the vocabulary.
	inline constexpr std::string_view theme_document_base_key = "base";

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

		/// The signature is present and the version is neither
		/// `theme_document_version` nor `baseless_theme_document_version`.
		unsupported_version,

		/// A key outside the closed `appearance_role` vocabulary, or a `base`
		/// key in a version-1 document, which has none. The subject is the
		/// offending key.
		unknown_field,

		/// A key that appears more than once. Refused rather than resolved by
		/// precedence, because a silently overridden value is how a document
		/// hides what it actually sets.
		duplicate_field,

		/// A role of the closed vocabulary that the document never assigns. The
		/// subject is the missing role's stable identifier. A palette is total,
		/// so a partial document is refused rather than completed with defaults
		/// the core does not own. The one exception is a role for which
		/// `defaults_on_import` holds (the label tints, [[BPR-1083]], and the
		/// roles after them): it is imported as `unassigned_appearance_color`,
		/// which the caller fills from the document's base
		/// (`with_base_defaults`). A version-2 document without a `base` line
		/// is refused with this reason and the subject `base`.
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

		/// [[BPR-1479]]. A `base` value that names no `appearance_variant`. The
		/// subject is the offending value.
		unknown_base,
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
	/// \details `Palette` and `Base` hold values exactly when `Rejections` is
	/// empty, so a caller cannot read a partially trusted palette: an import
	/// either produces a total mapping over the closed vocabulary or it produces
	/// reasons.
	///
	/// `Base` is the variant the palette was made against: the document's
	/// `base` line, or for a version-1 document an inference from its own
	/// `code_background`, which is dark when white text contrasts with it more
	/// than black text does and light otherwise. A version-1 document never
	/// infers a high-contrast base. A role the document left out is still
	/// `unassigned_appearance_color` in `Palette`; the caller fills it from
	/// `Base`'s built-in palette with `with_base_defaults`.
	struct theme_import_result
	{
		std::optional<appearance_palette>   Palette;
		std::vector<theme_import_rejection> Rejections;
		std::optional<appearance_variant>   Base;
	};

	/// \brief Writes a palette as a theme document for someone to import.
	///
	/// \details A version-2 document: the signature line, the `base` line
	/// naming `aBase`, then one line per role in vocabulary order, each an
	/// `appearance_role` stable identifier and an eight-digit `#rrggbbaa` value.
	/// Alpha is always written so that no reader has to assume one. The result
	/// is deterministic and imports back to an equal palette and base.
	[[nodiscard]] auto export_theme(
		const appearance_palette& aPalette,
		appearance_variant        aBase) -> std::string;

	/// \brief Writes a palette as a version-1 document, which names no base.
	///
	/// \details The appearance store's form: each stored document sits in one
	/// variant's slot, and the slot says what its base is. Otherwise as above.
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
	/// unknown key, duplicate key, malformed value and unknown base in line
	/// order; and only when all of those are clean, a version-2 document's
	/// missing `base` and every role the document failed to assign.
	[[nodiscard]] auto import_theme(std::string_view aDocument) -> theme_import_result;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_THEME_EXCHANGE_HPP
