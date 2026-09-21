#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
	void require(bool aCondition)
	{
		if (!aCondition) {
			std::abort();
		}
	}

	void inspect(std::string_view aInput)
	{
		using namespace bpr;
		const auto result = application::prepare_release_diagnostics(aInput, core::release_maintenance_revision{ 1U });
		const auto replay = application::prepare_release_diagnostics(aInput, core::release_maintenance_revision{ 1U });
		require(result.has_value() == replay.has_value());
		if (result) {
			require(result->Bytes == replay->Bytes && result->Descriptor == replay->Descriptor);
			require(result->Bytes.size() == result->Descriptor.Bytes && result->Bytes.size() <= core::release_diagnostic_byte_limit);
			require(result->Descriptor.Records <= core::release_diagnostic_record_limit);
			require(result->Descriptor.Digest == core::sha256({ reinterpret_cast<const std::uint8_t*>(result->Bytes.data()), result->Bytes.size() }));
			require(result->Bytes.find("BPR_PRIVATE_CANARY") == std::string::npos);
		}
	}
}

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, std::size_t aSize) -> int
{
	const std::string_view input{ reinterpret_cast<const char*>(aData), std::min<std::size_t>(aSize, 16384U) };
	inspect(input);
	// Exercise valid metadata with arbitrary, never-exportable detail bytes as
	// well as the completely arbitrary framing above. Embedded newlines remain
	// untrusted framing, not an instruction to copy the prose or the canary.
	std::string framed{ "t v=1 op=github.pull_request outcome=failed request=7 http_status=403 summary=sanitized dropped=0 detail=BPR_PRIVATE_CANARY " };
	framed.append(input).push_back('\n');
	inspect(framed);
	return 0;
}
