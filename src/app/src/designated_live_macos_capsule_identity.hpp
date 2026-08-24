#ifndef BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_CAPSULE_IDENTITY_HPP
#define BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_CAPSULE_IDENTITY_HPP

/// \file
/// \brief Nested capsule identity, retained across the check/use window.
///
/// \details BPR-0262 fact_2 requires stage one and the application to fail
/// closed on nested-resource substitution. The recursive check that exists runs
/// inside a Python verifier which has fully exited before the child is resumed,
/// and the supervisor's own post-verification checks cover only the top-level
/// application executable. Nothing re-examined a nested dylib, framework,
/// plugin, or `qt.conf` between the verifier's exit and `dyld` loading it.
///
/// This closes the interval the supervisor controls: capture through the root
/// descriptor retained before the verifier starts, then revalidate through that
/// same descriptor before spawn and immediately before resume. A substitution
/// anywhere in that pre-resume interval is refused.
///
/// Two things it deliberately does not claim. It cannot cover the interval
/// after the resume — that is what the read-only mount in [[BPR-0353]] is for,
/// and the two are meant to hold together rather than either alone. And it is
/// not a second inventory: the inventory decides what the capsule *should*
/// contain, and this decides only that nothing changed while the supervisor was
/// looking away.
///
/// Everything is asked through a retained directory descriptor. A path
/// re-resolved from its text can be answered by a different directory than the
/// one that was verified; `openat` from a descriptor opened `O_NOFOLLOW` cannot.

#if defined(__APPLE__)

	#include <sys/stat.h>

	#include <cstdint>
	#include <filesystem>
	#include <optional>
	#include <string>
	#include <vector>

namespace bpr::app::designated_live_capsule_identity {
	/// \brief One capsule entry as it stood when it was captured.
	struct entry
	{
		/// Path relative to the retained capsule root, never absolute: an
		/// absolute path is a fresh resolution, which is the thing being
		/// avoided.
		std::string   Relative;
		std::uint64_t Device              = 0U;
		std::uint64_t Inode               = 0U;
		std::uint64_t Size                = 0U;
		std::int64_t  ModifiedSeconds     = 0;
		std::int64_t  ModifiedNanoseconds = 0;
		std::uint32_t Mode                = 0U;
		/// Present for regular files. A symlink carries its target instead, and
		/// a directory carries neither.
		std::optional<std::string> Sha256;
		std::optional<std::string> SymlinkTarget;

		auto operator==(const entry&) const -> bool = default;
	};

	/// \brief Every entry beneath a retained capsule root, in a stable order.
	///
	/// \details Returns nothing when the walk cannot be completed exactly —
	/// an unreadable entry, a kind that has no business in a capsule, or a
	/// symlink that escapes the root. A partial answer here would be worse than
	/// none, because the caller's next move is to trust it.
	[[nodiscard]] auto capture(int aRootDescriptor) -> std::optional<std::vector<entry>>;

	/// \brief Whether the capsule still reads exactly as captured.
	///
	/// \details Re-walks through the same retained descriptor and requires an
	/// identical result. Any difference — content, inode, size, timestamp,
	/// mode, symlink target, an entry gained or lost — is a refusal.
	[[nodiscard]] auto unchanged(
		int                       aRootDescriptor,
		const std::vector<entry>& aCaptured) -> bool;

	/// \brief Opens a capsule root for retention, refusing a symlinked path.
	[[nodiscard]] auto open_root(const std::filesystem::path& aRoot)
		-> std::optional<int>;
}

#endif // defined(__APPLE__)

#endif // BPR_SRC_APP_SRC_DESIGNATED_LIVE_MACOS_CAPSULE_IDENTITY_HPP
