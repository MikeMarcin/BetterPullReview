#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_EXPORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_EXPORT_HPP

#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>

/// \file
/// \brief Saving a frozen round's document to a file the reviewer chose
/// (RFC-BPR-0056, BPR-1159).
/// \details The destination is one path the reviewer picked; nothing else is
/// written, read or listed. A save never replaces other bytes unless the
/// reviewer confirmed replacing exactly those bytes, and a file that already
/// holds the same bytes is left untouched. A save records only that these
/// bytes were written to this file: not that anyone received or read them.
namespace bpr::application {
	enum class cycle_export_failure : std::uint8_t {
		/// Another file appeared at the destination while saving.
		exists,
		/// The file changed after the reviewer confirmed replacing it.
		changed,
		/// The destination is a link, a directory or another special file.
		not_regular,
		/// The destination's folder is missing or unwritable, or I/O failed.
		unavailable,
		/// The file there is larger than would ever be read back.
		too_large,
		cancelled,
		/// [[BPR-1239]]. The file there is marked read-only, so replacing it
		/// is refused; its attribute is never cleared to make the save succeed.
		read_only,
		/// [[BPR-1239]]. Another program still has the file there open past a
		/// brief wait, so it cannot be replaced.
		in_use,
	};
	[[nodiscard]] auto stable_name(cycle_export_failure aValue) noexcept -> std::string_view;

	/// \brief One destination file the reviewer chose.
	/// \details Writes go through a temporary file beside the destination
	/// that is flushed and then moved into place, so an interrupted save
	/// leaves either the whole document or no document at the destination.
	class cycle_export_destination
	{
	public:
		virtual ~cycle_export_destination() = default;

		/// \brief The regular file there now, by digest and size; nothing
		/// when there is none. A link or special file is `not_regular`.
		virtual auto observe(std::uint64_t aMaximumBytes, std::stop_token aStop) -> std::expected<std::optional<cycle_object_ref>, cycle_export_failure> = 0;
		/// \brief Puts these bytes there only if nothing is there.
		virtual auto create(std::span<const std::uint8_t> aBytes, std::stop_token aStop) -> std::expected<void, cycle_export_failure> = 0;
		/// \brief Replaces the file only while it still holds `aExpected`.
		virtual auto replace(std::span<const std::uint8_t> aBytes, const cycle_object_ref& aExpected, std::stop_token aStop) -> std::expected<void, cycle_export_failure> = 0;
	};

	/// \brief What a save did. A conflict writes nothing and names the bytes
	/// there, so the reviewer can confirm replacing exactly them.
	struct cycle_round_save
	{
		core::cycle_round_save_action   Action = core::cycle_round_save_action::conflict;
		std::optional<cycle_object_ref> Existing;
		cycle_object_ref                Written;
	};
	/// \brief Saves document bytes to the destination. `aConfirmed` is the
	/// existing file's digest the reviewer agreed to replace.
	[[nodiscard]] auto save_round_document(cycle_export_destination& aDestination, std::span<const std::uint8_t> aBytes,
										   const std::optional<core::cycle_material_digest>& aConfirmed, std::stop_token aStop) -> std::expected<cycle_round_save, cycle_export_failure>;
}

#endif
