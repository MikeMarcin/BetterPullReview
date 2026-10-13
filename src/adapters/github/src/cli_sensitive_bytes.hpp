#ifndef BPR_CLI_SENSITIVE_BYTES_HPP
#define BPR_CLI_SENSITIVE_BYTES_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace bpr::adapters::github::detail {

	/// \brief Fixed, noncopyable storage for one CLI credential or sensitive read chunk.
	/// \details Owned bytes are overwritten on destruction. Qt, the OS and stock
	/// children may make memory copies; this does not promise erasure of those copies.
	class cli_sensitive_bytes final
	{
	public:
		static constexpr std::size_t capacity                              = 8193U;
		cli_sensitive_bytes()                                              = default;
		cli_sensitive_bytes(const cli_sensitive_bytes&)                    = delete;
		cli_sensitive_bytes(cli_sensitive_bytes&&)                         = delete;
		auto operator=(const cli_sensitive_bytes&) -> cli_sensitive_bytes& = delete;
		auto operator=(cli_sensitive_bytes&&) -> cli_sensitive_bytes&      = delete;
		~cli_sensitive_bytes() { clear(); }

		[[nodiscard]] auto append(std::string_view aBytes) noexcept -> bool
		{
			if (aBytes.size() > capacity - Count) {
				return false;
			}
			std::copy(aBytes.begin(), aBytes.end(), Bytes.begin() + static_cast<std::ptrdiff_t>(Count));
			Count += aBytes.size();
			return true;
		}
		[[nodiscard]] auto view() const noexcept -> std::string_view { return { Bytes.data(), Count }; }
		[[nodiscard]] auto finish_credential() noexcept -> bool
		{
			if (Count == 0U || Bytes[Count - 1U] != '\n') {
				return false;
			}
			Bytes[--Count] = '\0';
			return Count != 0U && Count <= 8192U && std::ranges::all_of(view(), [](unsigned char aByte) { return aByte > 32U && aByte < 127U; });
		}
		void remove_prefix() noexcept
		{
			if (Count != 0U) {
				std::move(Bytes.begin() + 1, Bytes.begin() + static_cast<std::ptrdiff_t>(Count), Bytes.begin());
				Bytes[--Count] = 0;
			}
		}
		void clear() noexcept
		{
			volatile char* cursor = Bytes.data();
			for (std::size_t index = 0; index < capacity; ++index) {
				cursor[index] = '\0';
			}
			Count = 0U;
		}

	private:
		std::array<char, capacity> Bytes{ };
		std::size_t                Count = 0U;
	};
}
#endif
