#include "designated_live_mutation_canary.hpp"

#include <cstddef>

#if defined(_WIN32)
	#include <io.h>
#else
	#include <unistd.h>
extern char** environ;
#endif

namespace bpr::tests::network_isolation {
	namespace {

		[[nodiscard]] bool is_lower_hex(char aValue)
		{
			return (aValue >= '0' && aValue <= '9') || (aValue >= 'a' && aValue <= 'f');
		}

		[[nodiscard]] int read_bytes(int aDescriptor, char* aBuffer, unsigned aSize) noexcept
		{
#if defined(_WIN32)
			return _read(aDescriptor, aBuffer, aSize);
#else
			return static_cast<int>(read(aDescriptor, aBuffer, aSize));
#endif
		}

		[[nodiscard]] int write_bytes(
			int         aDescriptor,
			const char* aBuffer,
			unsigned    aSize) noexcept
		{
#if defined(_WIN32)
			return _write(aDescriptor, aBuffer, aSize);
#else
			return static_cast<int>(write(aDescriptor, aBuffer, aSize));
#endif
		}

	} // namespace

	int run_designated_live_mutation_canary(
		int  aInputDescriptor,
		int  aOutputDescriptor,
		bool aRequireEmptyEnvironment) noexcept
	{
#if !defined(_WIN32)
		if (aRequireEmptyEnvironment && environ != nullptr && environ[0] != nullptr) {
			return 1;
		}
#else
		if (aRequireEmptyEnvironment) {
			return 1;
		}
#endif
		constexpr char capabilityPrefix[] = "bpr.designated-live-capability/v1\nsession_nonce=";
		constexpr char accepted[]         = "DESIGNATED_LIVE_CAPABILITY_ACCEPTED\n";
		constexpr auto prefixSize         = sizeof(capabilityPrefix) - 1;
		constexpr auto nonceSize          = 64U;
		constexpr auto expectedSize       = prefixSize + nonceSize + 1U;
		char           input[expectedSize + 1U]{ };
		std::size_t    total{ };

		for (;;) {
			const auto count = read_bytes(
				aInputDescriptor,
				input + total,
				static_cast<unsigned>(sizeof(input) - total));
			if (count == 0) {
				break;
			}
			if (count < 0) {
				return 1;
			}
			total += static_cast<std::size_t>(count);
			if (total > expectedSize) {
				return 1;
			}
		}
		if (total != expectedSize || input[expectedSize - 1U] != '\n') {
			return 1;
		}
		for (std::size_t index = 0; index < prefixSize; ++index) {
			if (input[index] != capabilityPrefix[index]) {
				return 1;
			}
		}
		for (std::size_t index = prefixSize; index < prefixSize + nonceSize; ++index) {
			if (!is_lower_hex(input[index])) {
				return 1;
			}
		}
		return write_bytes(aOutputDescriptor, accepted, sizeof(accepted) - 1U) == static_cast<int>(sizeof(accepted) - 1U) ? 0 : 1;
	}

} // namespace bpr::tests::network_isolation

#if !defined(BPR_DESIGNATED_LIVE_CANARY_NO_MAIN)
int main()
{
	return bpr::tests::network_isolation::run_designated_live_mutation_canary(0, 1, false);
}
#endif
