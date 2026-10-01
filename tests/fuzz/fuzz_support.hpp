#ifndef BPR_TESTS_FUZZ_FUZZ_SUPPORT_HPP
#define BPR_TESTS_FUZZ_FUZZ_SUPPORT_HPP

// [[BPR-0970]]. Shared pieces of the parser and mapping fuzz targets. A target
// reads its input through `fuzz_input`, which splits one libFuzzer buffer into
// a selector, a few small parameters and the bytes handed to the production
// entry point, so one corpus file reproduces one call exactly.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>

namespace bpr::fuzz {
	/// \brief Ends the run as a crash when a property of the production result
	/// does not hold. libFuzzer then saves the input that showed it.
	inline auto require(const bool aCondition) -> void
	{
		if (!aCondition) {
			std::abort();
		}
	}

	/// \brief A deterministic non-zero identity distinguished by one byte.
	template <typename Identity>
	constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xa5U);
		return Identity{ bytes };
	}

	/// \brief Reads small parameters from the front of an input and leaves the
	/// rest as the payload. Reading past the end yields zero, never a fault.
	class fuzz_input
	{
	public:
		fuzz_input(const std::uint8_t* aData, const std::size_t aSize) noexcept
			: Bytes{ aData, aSize }
		{
		}

		[[nodiscard]] auto byte() noexcept -> std::uint8_t
		{
			if (Bytes.empty()) {
				return 0U;
			}
			const auto value = Bytes.front();
			Bytes            = Bytes.subspan(1U);
			return value;
		}

		[[nodiscard]] auto u16() noexcept -> std::uint16_t
		{
			const auto high = static_cast<unsigned int>(byte());
			return static_cast<std::uint16_t>((high << 8U) | static_cast<unsigned int>(byte()));
		}

		/// \brief The rest of the input as bytes.
		[[nodiscard]] auto rest() const noexcept -> std::span<const std::uint8_t>
		{
			return Bytes;
		}

		/// \brief The rest of the input as text, byte for byte.
		[[nodiscard]] auto text() const noexcept -> std::string_view
		{
			return { reinterpret_cast<const char*>(Bytes.data()), Bytes.size() };
		}

		/// \brief Splits the rest at the first occurrence of `aSeparator`; the
		/// second part is empty when it does not occur.
		[[nodiscard]] auto split(const std::string_view aSeparator) const noexcept
			-> std::array<std::string_view, 2>
		{
			const auto whole = text();
			const auto at    = whole.find(aSeparator);
			if (at == std::string_view::npos) {
				return { whole, std::string_view{ } };
			}
			return { whole.substr(0U, at), whole.substr(at + aSeparator.size()) };
		}

	private:
		std::span<const std::uint8_t> Bytes;
	};

	/// \brief The separator a multi-part input uses between its parts. It is
	/// unlikely in any document a target reads, and the seed corpora use it.
	inline constexpr std::string_view part_separator = "\n\x1e\x1e\n";
}

#endif
