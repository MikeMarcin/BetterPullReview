#ifndef BPR_PLATFORM_DURABLE_VOLUME_IDENTITY_HPP
#define BPR_PLATFORM_DURABLE_VOLUME_IDENTITY_HPP

// [[BPR-0794]], [[BPR-2118]]. The identity of an open file or folder that
// stays the same when its volume is mounted again.
//
// On macOS `st_dev` names a mount, not a volume. The kernel numbers APFS
// devices as volumes mount, so a restart, or a disk or disk image attached in
// another order, can renumber the Data volume while no file changes. An
// identity that is written to disk and read back by a later launch therefore
// carries the volume's APFS UUID (`ATTR_VOL_UUID`) in place of `st_dev`. The
// retained Git cache manifest ([[BPR-0794]]) and the agent workspace
// ownership journal, with the guard and directory identities a retained
// conversation keeps ([[BPR-2118]]), use this one reading.
//
// The result is four numbers: the caller's kind tag, the volume, a second
// volume word, and the inode. On macOS the two volume words are the UUID's
// first and last eight bytes, big-endian. Elsewhere on Unix they are `st_dev`
// and zero, exactly the identity those callers stored before, because Linux
// `st_dev` names the block device and does not move on a restart.
//
// A check that compares two live `stat` results within one process keeps
// comparing `st_dev`: within one mount it is exact, and cheaper.
//
// Header-only and free of Qt. It reads; it decides nothing.

#if defined(__unix__) || defined(__APPLE__)
	#include <sys/stat.h>
	#include <sys/types.h>

	#include <array>
	#include <cstdint>
	#include <optional>
#endif
#if defined(__APPLE__)
	#include <sys/attr.h>
	#include <unistd.h>

	#include <algorithm>
	#include <cstddef>
#endif

namespace bpr::platform::durable_volume_identity {
#if defined(__unix__) || defined(__APPLE__)
	/// \brief A kind tag, two volume words and an inode.
	using identity = std::array<std::uint64_t, 4U>;

	#if defined(__APPLE__)
	/// \brief The APFS volume UUID of the volume holding \p aDescriptor, as
	/// two big-endian words.
	///
	/// \details No value for a zero UUID, a short attribute buffer, a failed
	/// read, or a descriptor whose device and inode differ between the
	/// `fstat` before and the one after the read.
	[[nodiscard]] inline auto volume_uuid(const int aDescriptor) noexcept -> std::optional<std::array<std::uint64_t, 2U>>
	{
		struct stat before{ };
		if (::fstat(aDescriptor, &before) != 0) {
			return std::nullopt;
		}
		auto attributes        = attrlist{ };
		attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
		attributes.volattr     = ATTR_VOL_INFO | ATTR_VOL_UUID;
		struct volume_attributes
		{
			std::uint32_t                 Length = 0;
			std::array<std::uint8_t, 16U> Uuid{ };
		} volume;
		static_assert(sizeof(volume) == 20U);
		struct stat after{ };
		if (::fgetattrlist(aDescriptor, &attributes, &volume, sizeof(volume), 0) != 0
			|| volume.Length != sizeof(volume)
			|| std::ranges::all_of(volume.Uuid, [](const auto aByte) { return aByte == 0U; })
			|| ::fstat(aDescriptor, &after) != 0 || after.st_dev != before.st_dev || after.st_ino != before.st_ino) {
			return std::nullopt;
		}
		auto words = std::array<std::uint64_t, 2U>{ };
		for (std::size_t index = 0U; index < 16U; ++index) {
			auto& word = words[index / 8U];
			word       = (word << 8U) | volume.Uuid[index];
		}
		return words;
	}
	#endif

	/// \brief The durable identity of \p aDescriptor, tagged \p aKind.
	///
	/// \details The caller has already checked type, owner and link count on
	/// this descriptor; this adds only the volume and inode.
	[[nodiscard]] inline auto of_descriptor(const int aDescriptor, const std::uint64_t aKind) noexcept -> std::optional<identity>
	{
		struct stat information{ };
		if (::fstat(aDescriptor, &information) != 0) {
			return std::nullopt;
		}
	#if defined(__APPLE__)
		const auto uuid = volume_uuid(aDescriptor);
		if (!uuid) {
			return std::nullopt;
		}
		return identity{ aKind, (*uuid)[0], (*uuid)[1], static_cast<std::uint64_t>(information.st_ino) };
	#else
		return identity{ aKind, static_cast<std::uint64_t>(information.st_dev), 0U, static_cast<std::uint64_t>(information.st_ino) };
	#endif
	}
#endif
}

#endif // BPR_PLATFORM_DURABLE_VOLUME_IDENTITY_HPP
