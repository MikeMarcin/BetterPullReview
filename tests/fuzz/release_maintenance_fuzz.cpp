#include "bpr/core/release_maintenance.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {
	using namespace bpr::core;

	void require(bool aCondition)
	{
		if (!aCondition) {
			std::abort();
		}
	}

	auto event_for(const release_maintenance_state& aState, std::uint8_t aKind, std::uint8_t aValue) -> release_maintenance_event
	{
		auto context = aState.Pending.value_or(aState.Prepared.value_or(release_maintenance_context{ }));
		if ((aKind & 16U) != 0U) {
			context.Authority.Session = application_session_id{ aValue };
		}
		switch (aKind % 9U) {
		case 0:
			return release_maintenance_intent{ aState.Authority, static_cast<release_maintenance_command>(aValue), aState.Pending ? aState.Pending : aState.Prepared };
		case 1: {
			const release_diagnostic_snapshot snapshot{ .Revision = release_maintenance_revision{ aValue }, .Digest = { aValue }, .Fields = aValue, .Records = static_cast<std::uint32_t>(aValue) * 8U, .Bytes = static_cast<std::uint64_t>(aValue) * 2048U };
			return release_maintenance_prepared{ context, snapshot };
		}
		case 2: {
			const release_maintenance_inventory inventory{ .Revision = release_maintenance_revision{ aValue }, .Digest = { aValue }, .Categories = aValue, .Entries = static_cast<std::uint32_t>(aValue) * 32U, .UnknownEntries = static_cast<std::uint32_t>(aKind & 1U), .Complete = (aKind & 2U) != 0U };
			return release_maintenance_prepared{ context, inventory };
		}
		case 3: return release_maintenance_commit_started{ context };
		case 4: return release_maintenance_completed{ context };
		case 5: return release_maintenance_failed{ context, static_cast<release_maintenance_reason>(aValue) };
		case 6: return release_maintenance_cancelled{ context };
		case 7: return release_maintenance_invalidated{ context.Authority };
		default:
			return release_maintenance_authority_changed{ { bpr_profile_id{ aValue }, application_session_id{ 2U }, release_maintenance_root_id{ 3U } }, { true, true, true, true } };
		}
	}

	void verify_transition(release_maintenance_state& aState, const release_maintenance_event& aEvent)
	{
		const auto before = aState;
		auto       replay = before;
		const auto output = transition(aState, aEvent);
		require(output == transition(replay, aEvent) && aState == replay);
		require(output.Accepted || (aState == before && !output.Request));
		if (aState.Prepared) {
			if (const auto* snapshot = std::get_if<release_diagnostic_snapshot>(&aState.Prepared->Selection)) {
				require(snapshot->Fields != 0U && (snapshot->Fields & ~release_diagnostic_default_fields) == 0U);
				require(snapshot->Records <= release_diagnostic_record_limit && snapshot->Bytes <= release_diagnostic_byte_limit);
			}
		}
		if (output.Request && !output.Request->Cancel) {
			require(aState.Pending == output.Request->Context && output.Request->Context.Authority == aState.Authority);
			if (!std::holds_alternative<std::monostate>(output.Request->Context.Selection)) {
				require(before.Prepared && before.Prepared->Selection == output.Request->Context.Selection);
			}
		}
	}
}

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, std::size_t aSize) -> int
{
	auto state = bpr::core::make_release_maintenance_state(
		{ bpr::core::bpr_profile_id{ 1U }, bpr::core::application_session_id{ 2U }, bpr::core::release_maintenance_root_id{ 3U } },
		{ true, true, true, true });
	const auto size = std::min<std::size_t>(aSize, 256U);
	for (std::size_t index = 0; index + 1U < size; index += 2U) {
		verify_transition(state, event_for(state, aData[index], aData[index + 1U]));
	}
	verify_transition(state, bpr::core::release_maintenance_shutdown{ });
	return 0;
}
