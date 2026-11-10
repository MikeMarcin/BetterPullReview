#ifndef BPR_LOGGING_QUEUED_DIAGNOSTIC_RECORD_HPP
#define BPR_LOGGING_QUEUED_DIAGNOSTIC_RECORD_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <quill/DeferredFormatCodec.h>
#include <quill/bundled/fmt/format.h>
#include <string_view>
#include <type_traits>

namespace bpr::adapters::logging::detail {
	inline constexpr std::size_t maximum_queued_record_bytes = 16U * 1024U - 1U + 21U;
	using diagnostic_record_charge                           = std::array<char, 16U>;

	// A fixed private transport field, never part of the persisted JSON. Its
	// bytes use Quill's ordinary string codec, independently of deferred decode.
	inline auto encode_record_charge(std::size_t aBytes) noexcept -> diagnostic_record_charge
	{
		diagnostic_record_charge result;
		result.fill('0');
		std::array<char, 16U> digits{ };
		const auto            converted = std::to_chars(digits.data(), digits.data() + digits.size(), aBytes, 16);
		const auto            count     = static_cast<std::size_t>(converted.ptr - digits.data());
		std::copy_n(digits.begin(), count, result.end() - static_cast<std::ptrdiff_t>(count));
		return result;
	}
	inline auto decode_record_charge(std::string_view aStatement) noexcept -> std::optional<std::size_t>
	{
		if (aStatement.size() < diagnostic_record_charge{ }.size()) {
			return std::nullopt;
		}
		const auto field = aStatement.substr(0U, diagnostic_record_charge{ }.size());
		if (!std::ranges::all_of(field, [](char aCharacter) { return (aCharacter >= '0' && aCharacter <= '9') || (aCharacter >= 'a' && aCharacter <= 'f'); })) {
			return std::nullopt;
		}
		std::size_t value  = 0U;
		const auto  parsed = std::from_chars(field.data(), field.data() + field.size(), value, 16);
		if (parsed.ec != std::errc{ } || parsed.ptr != field.data() + field.size() || value == 0U || value > maximum_queued_record_bytes) {
			return std::nullopt;
		}
		return value;
	}

	inline auto copy_record_bytes(std::string_view aBytes) -> std::unique_ptr<const char[]>
	{
		auto data = std::make_unique<char[]>(aBytes.size());
		std::ranges::copy(aBytes, data.get());
		return data;
	}
	struct diagnostic_record_bytes
	{
		explicit diagnostic_record_bytes(std::string_view aBytes)
			: Data{ copy_record_bytes(aBytes) }
			, Size{ aBytes.size() }
		{ }
		std::unique_ptr<const char[]> Data;
		std::size_t                   Size;
	};
	// Only this small immutable owning handle enters Quill's 64KiB queue.
	// The exact-size buffer has no spare string capacity; its logical byte
	// charge remains pending until the sink, AFTER Quill clears its arg store.
	struct queued_diagnostic_record
	{
		std::shared_ptr<const diagnostic_record_bytes> Bytes;
	};
	static_assert(std::is_nothrow_move_constructible_v<queued_diagnostic_record>);
	static_assert(std::is_nothrow_copy_constructible_v<queued_diagnostic_record>);
	static_assert(std::is_nothrow_destructible_v<queued_diagnostic_record>);

	struct pending_diagnostic_records
	{
		std::atomic<std::size_t> Events{ 0U };
		std::atomic<std::size_t> Bytes{ 0U };
		// The implementation's frontend mutex serializes reservations. Only the
		// backend sink releases them; it never blocks frontend admission.
		auto reserve(std::size_t aBytes, bool aPriority) noexcept -> bool
		{
			const auto eventLimit = aPriority ? 4096U : 3584U;
			const auto byteLimit  = (aPriority ? 8U : 7U) * 1024U * 1024U;
			if (aBytes == 0U || aBytes > maximum_queued_record_bytes || Events.load() >= eventLimit || Bytes.load() > byteLimit - aBytes) {
				return false;
			}
			Events.fetch_add(1U);
			Bytes.fetch_add(aBytes);
			return true;
		}
		auto release(std::size_t aBytes) noexcept -> bool
		{
			if (aBytes == 0U || Events.load() == 0U || Bytes.load() < aBytes) {
				return false;
			}
			Events.fetch_sub(1U);
			Bytes.fetch_sub(aBytes);
			return true;
		}
	};
	// Known charges are released even when deferred decode produced a placeholder.
	// An unknown charge is left pending until BOTH logger retirements complete.
	struct consumed_diagnostic_record
	{
		std::optional<std::string_view> Statement;
		bool                            Released = false;
	};
	inline auto consume_queued_record(pending_diagnostic_records& aPending, std::string_view aStatement) noexcept -> consumed_diagnostic_record
	{
		const auto charge = decode_record_charge(aStatement);
		if (!charge || !aPending.release(*charge)) {
			return { };
		}
		const auto statement = aStatement.substr(diagnostic_record_charge{ }.size());
		if (statement.size() != *charge + 1U || statement.back() != '\n') {
			return { std::nullopt, true };
		}
		return { statement, true };
	}

	// The same linked accounting path is used by the production sink/close and
	// the real-backend failure/retirement controls. No test-only product seam.
	auto consume_record_for_sink(pending_diagnostic_records& aPending, std::atomic<std::uint64_t>& aDropped, std::string_view aStatement) noexcept -> std::optional<std::string_view>;
	auto retire_queued_records(pending_diagnostic_records& aPending, std::atomic<std::uint64_t>& aDropped) noexcept -> bool;

}

template <>
struct quill::Codec<bpr::adapters::logging::detail::queued_diagnostic_record> : quill::DeferredFormatCodec<bpr::adapters::logging::detail::queued_diagnostic_record>
{ };

template <>
struct fmtquill::formatter<bpr::adapters::logging::detail::queued_diagnostic_record>
{
	constexpr auto parse(format_parse_context& aContext) { return aContext.begin(); }
	auto           format(const bpr::adapters::logging::detail::queued_diagnostic_record& aRecord, format_context& aContext) const
	{
		if (!aRecord.Bytes) {
			throw fmtquill::format_error{ "Missing diagnostic record ownership" };
		}
		return fmtquill::format_to(aContext.out(), "{}", std::string_view{ aRecord.Bytes->Data.get(), aRecord.Bytes->Size });
	}
};

namespace bpr::adapters::logging::detail {
	inline auto queued_record_frame_bytes() -> std::size_t
	{
		quill::detail::SizeCacheVector cache;
		const diagnostic_record_charge charge{ };
		// Pinned Quill13's ordinary log header is four uint64 words. Both
		// arguments have fixed encoded sizes on the running target ABI.
		return 4U * sizeof(std::uint64_t)
			+ quill::Codec<std::string_view>::compute_encoded_size(cache, std::string_view{ charge.data(), charge.size() })
			+ quill::Codec<queued_diagnostic_record>::compute_encoded_size(cache, queued_diagnostic_record{ });
	}
}

#endif
