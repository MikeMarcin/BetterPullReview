#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_BUNDLE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_BUNDLE_HPP

#include "bpr/core/review_cycle_round.hpp"
#include "bpr/core/review_cycle_zip.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Portable review bundles (`.bpr-review.zip`, RFC-BPR-0056,
/// BPR-1160), as pure byte functions.
/// \details A bundle holds `manifest.json`, `round.json` (the authoritative
/// envelope), `review.md` (the readable document, derived from the round) and
/// optional `objects/<sha256>` attachments: the exact bytes of the round's
/// selected sources, named by their digest. The manifest lists every other
/// member with its type, size and SHA-256 and is itself the only unlisted
/// member. Opening one validates everything as untrusted data before
/// anything is returned: container limits (`review_cycle_zip.hpp`), portable
/// member names with no traversal, drive, UNC, control, encoded traversal or
/// reserved device name, no two names another platform would merge, exactly
/// the listed members, strict JSON, the round's own checks, a `review.md`
/// that is exactly the document the round renders to, and attachments that
/// are exactly bytes the round names. Nothing is extracted or executed.
namespace bpr::core {
	/// An archive larger than this is not read.
	inline constexpr std::uint64_t maximum_bundle_archive_bytes = 512U * 1024U * 1024U;

	/// \brief A validated bundle: the round, its document and its attachments.
	struct cycle_opened_bundle
	{
		cycle_decoded_round Round;
		std::string         Review;
		/// Attachments by SHA-256, in name order.
		std::vector<std::pair<cycle_material_digest, std::string>> Objects;
	};

	enum class cycle_bundle_problem : std::uint8_t {
		/// The container was refused; see `Zip`.
		archive,
		/// A member name that is not a portable relative name.
		member_name,
		/// Two members with one name, or names another platform would merge.
		alias,
		/// `manifest.json`, `round.json` or `review.md` is absent.
		missing_member,
		/// A member this bundle version does not define.
		unexpected_member,
		/// A member the manifest does not list, or a listed member absent.
		unlisted_member,
		/// The manifest is not strict JSON of the supported version and shape.
		manifest,
		/// A member whose size, SHA-256 or type differs from the manifest.
		member_mismatch,
		/// The round envelope was refused; see `Round`.
		round,
		/// `review.md` is not the document the round renders to.
		derived_mismatch,
		/// An attachment whose bytes are not its name's digest.
		object_mismatch,
		/// An attachment the round does not name.
		unreferenced_object,
	};
	[[nodiscard]] auto stable_name(cycle_bundle_problem aValue) noexcept -> std::string_view;

	struct cycle_bundle_error
	{
		cycle_bundle_problem                    Problem = cycle_bundle_problem::archive;
		std::optional<cycle_zip_problem>        Zip;
		std::optional<cycle_round_decode_error> Round;
		/// The member at fault, as named in the archive.
		std::string Member;
	};

	/// \brief True for a name that is safe on every supported platform, after
	/// percent-decoding (at most eight levels): relative, `/`-separated, no
	/// empty, `.` or `..` segment, no control character, backslash or colon,
	/// no segment ending in a dot or space, and no reserved device name.
	[[nodiscard]] auto portable_member_name(std::string_view aName) -> bool;

	/// \brief The bundle for a round: deterministic for a given round.
	[[nodiscard]] auto build_round_bundle(const cycle_decoded_round& aRound) -> std::string;
	/// \brief Validates an untrusted bundle and returns its content, or the
	/// first problem found. Nothing is returned in part.
	[[nodiscard]] auto open_round_bundle(std::span<const std::uint8_t> aArchive) -> std::expected<cycle_opened_bundle, cycle_bundle_error>;
}

#endif
