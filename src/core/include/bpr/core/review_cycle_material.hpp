#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_MATERIAL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_MATERIAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

/// \file
/// \brief What captured bytes are, decided from the bytes alone
/// (RFC-BPR-0054, BPR-1150).
/// \details Classification never changes a byte. Line endings, byte-order
/// marks and a missing final newline are reported, not normalized; an
/// encoding this build cannot map exactly is reported as unsupported rather
/// than decoded lossily. Nothing here runs a filter, textconv or LFS tool.
namespace bpr::core {
	enum class material_text_kind : std::uint8_t {
		/// No bytes at all.
		empty,
		/// Well-formed UTF-8 with no NUL byte.
		utf8,
		/// Well-formed UTF-8 after a UTF-8 byte-order mark.
		utf8_bom,
		/// A UTF-16 or UTF-32 byte-order mark, or bytes that are not
		/// well-formed UTF-8: file-level review only, no text mapping.
		unsupported_encoding,
		/// A NUL byte in otherwise unmarked content.
		binary,
	};

	enum class material_line_endings : std::uint8_t {
		none,
		lf,
		crlf,
		cr,
		mixed,
	};

	/// \brief One file's byte profile.
	struct material_profile
	{
		material_text_kind    Kind        = material_text_kind::empty;
		material_line_endings LineEndings = material_line_endings::none;
		/// The last byte is LF or CR; false for empty content.
		bool FinalNewline = false;
		/// The bytes are a Git LFS pointer, not the content it names.
		bool          LfsPointer                                 = false;
		std::uint64_t Size                                       = 0;
		auto          operator<=>(const material_profile&) const = default;

		[[nodiscard]] constexpr auto text() const noexcept -> bool
		{
			return Kind == material_text_kind::empty || Kind == material_text_kind::utf8 || Kind == material_text_kind::utf8_bom;
		}
	};

	/// \brief Classifies bytes delivered in any chunking; the result does not
	/// depend on where chunk boundaries fall.
	class material_classifier
	{
	public:
		void               add(std::span<const std::uint8_t> aBytes) noexcept;
		[[nodiscard]] auto finish() const noexcept -> material_profile;

	private:
		static constexpr std::size_t pointer_limit = 1024U;

		void add_byte(std::uint8_t aValue) noexcept;

		std::uint64_t                           Size = 0;
		std::array<std::uint8_t, 4>             Head{ };
		std::array<std::uint8_t, pointer_limit> Prefix{ };
		std::uint8_t                            Last = 0;
		/// Continuation bytes still expected, and the bounds on the next one.
		std::uint8_t Pending     = 0;
		std::uint8_t NextLow     = 0x80U;
		std::uint8_t NextHigh    = 0xBFU;
		bool         InvalidUtf8 = false;
		bool         Nul         = false;
		bool         PreviousCr  = false;
		bool         SawLf       = false;
		bool         SawCrlf     = false;
		bool         SawCr       = false;
	};

	/// \brief Classifies one complete buffer.
	[[nodiscard]] auto classify_material(std::span<const std::uint8_t> aBytes) noexcept -> material_profile;

	/// \brief True when a byte offset falls on a UTF-8 scalar boundary of
	/// well-formed UTF-8 (the end counts). Text ranges are byte offsets and
	/// must satisfy this at both ends.
	[[nodiscard]] auto utf8_boundary(std::span<const std::uint8_t> aText, std::uint64_t aOffset) noexcept -> bool;
	/// \brief The scalar-value column of a byte offset within one line, or
	/// nothing when the offset splits a scalar or the bytes are not UTF-8.
	/// Columns are for presentation; stored anchors use byte offsets.
	[[nodiscard]] auto utf8_column(std::span<const std::uint8_t> aLine, std::uint64_t aOffset) noexcept -> std::optional<std::uint64_t>;
	/// \brief The byte offset of a scalar-value column within one line, or
	/// nothing beyond its end.
	[[nodiscard]] auto utf8_offset(std::span<const std::uint8_t> aLine, std::uint64_t aColumn) noexcept -> std::optional<std::uint64_t>;

	/// \brief Portable Git file modes. Anything else is not a Git entry mode.
	inline constexpr std::uint32_t git_mode_regular    = 0100644U;
	inline constexpr std::uint32_t git_mode_executable = 0100755U;
	inline constexpr std::uint32_t git_mode_symlink    = 0120000U;
	inline constexpr std::uint32_t git_mode_gitlink    = 0160000U;
	inline constexpr std::uint32_t git_mode_directory  = 0040000U;
}

#endif
