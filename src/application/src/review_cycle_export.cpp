#include "bpr/application/review_cycle_export.hpp"

#include "bpr/core/sha256.hpp"

namespace bpr::application {
	auto stable_name(const cycle_export_failure aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_export_failure::exists:      return "exists";
		case cycle_export_failure::changed:     return "changed";
		case cycle_export_failure::not_regular: return "not_regular";
		case cycle_export_failure::unavailable: return "unavailable";
		case cycle_export_failure::too_large:   return "too_large";
		case cycle_export_failure::cancelled:   return "cancelled";
		}
		return "unavailable";
	}

	auto save_round_document(cycle_export_destination& aDestination, const std::span<const std::uint8_t> aBytes, const std::optional<core::cycle_material_digest>& aConfirmed,
							 std::stop_token aStop) -> std::expected<cycle_round_save, cycle_export_failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(cycle_export_failure::cancelled);
		}
		const auto written  = cycle_object_ref{ .Digest = core::cycle_material_digest{ core::sha256(aBytes) }, .Size = aBytes.size() };
		const auto existing = aDestination.observe(core::maximum_round_document_file_bytes, aStop);
		if (!existing) {
			return std::unexpected(existing.error());
		}
		const auto digest = *existing ? std::optional{ (*existing)->Digest } : std::nullopt;
		const auto action = core::decide_round_save(digest, written.Digest, aConfirmed);
		auto       result = cycle_round_save{ .Action = action, .Existing = *existing, .Written = written };
		switch (action) {
		case core::cycle_round_save_action::create: {
			if (auto created = aDestination.create(aBytes, aStop); !created) {
				return std::unexpected(created.error());
			}
			return result;
		}
		case core::cycle_round_save_action::replace: {
			if (auto replaced = aDestination.replace(aBytes, **existing, aStop); !replaced) {
				return std::unexpected(replaced.error());
			}
			return result;
		}
		case core::cycle_round_save_action::already_saved:
		case core::cycle_round_save_action::conflict:      return result;
		}
		return result;
	}
}
