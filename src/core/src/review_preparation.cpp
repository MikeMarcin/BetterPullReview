#include "bpr/core/review_preparation.hpp"

#include "bpr/core/sha256.hpp"
#include <algorithm>
#include <limits>
namespace bpr::core {
	auto valid_writing_utf8(std::string_view aText) noexcept -> bool
	{
		for (std::size_t i = 0; i < aText.size();) {
			const auto lead = static_cast<unsigned char>(aText[i++]);
			if (lead == 0) {
				return false;
			}
			if (lead < 0x80) {
				continue;
			}
			const auto count = lead >= 0xc2 && lead <= 0xdf ? 1U : lead >= 0xe0 && lead <= 0xef ? 2U
				: lead >= 0xf0 && lead <= 0xf4                                                  ? 3U
																								: 4U;
			if (count == 4 || count > aText.size() - i) {
				return false;
			}
			std::uint32_t point = lead & (0x7fU >> count);
			for (unsigned j = 0; j < count; ++j) {
				const auto byte = static_cast<unsigned char>(aText[i++]);
				if ((byte & 0xc0) != 0x80) {
					return false;
				}
				point = (point << 6) | (byte & 0x3f);
			}
			if (point < (count == 1 ? 0x80U : count == 2 ? 0x800U
														 : 0x10000U)
				|| point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) {
				return false;
			}
		}
		return true;
	}
	auto valid(const writing_hook_settings& aSettings) noexcept -> bool
	{
		return aSettings.Revision > 0 && aSettings.Revision < static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
			&& std::ranges::all_of(aSettings.Overrides, [](const auto& aText) { return aText.size() <= maximum_writing_instruction_bytes && valid_writing_utf8(aText); });
	}

	auto writing_digest(std::string_view aText) noexcept -> std::array<std::uint8_t, 32> { return sha256({ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() }); }
	auto writing_boundary(std::string_view aText, std::size_t aOffset) noexcept -> bool
	{
		return aOffset <= aText.size() && (aOffset == aText.size() || (static_cast<unsigned char>(aText[aOffset]) & 0xc0U) != 0x80U)
			&& !(aOffset > 0 && aOffset < aText.size() && aText[aOffset - 1] == '\r' && aText[aOffset] == '\n');
	}
	auto writing_blank(std::string_view aText) noexcept -> bool
	{
		if (!valid_writing_utf8(aText)) {
			return false;
		}
		for (std::size_t i = 0; i < aText.size();) {
			const auto lead  = static_cast<unsigned char>(aText[i++]);
			auto       point = static_cast<std::uint32_t>(lead);
			if (lead >= 0x80) {
				const auto count = lead < 0xe0 ? 1U : lead < 0xf0 ? 2U
																  : 3U;
				point            = lead & (0x7fU >> count);
				for (unsigned n = 0; n < count; ++n) {
					point = (point << 6U) | (static_cast<unsigned char>(aText[i++]) & 0x3fU);
				}
			}
			if (!((point >= 9 && point <= 13) || point == 32 || point == 0x85 || point == 0xa0 || point == 0x1680 || (point >= 0x2000 && point <= 0x200a) || point == 0x2028 || point == 0x2029 || point == 0x202f || point == 0x205f || point == 0x3000)) {
				return false;
			}
		}
		return true;
	}
	auto scoped_writing_state::begin(writing_target aTarget, std::string aRaw, writing_selection aSelection) -> std::expected<writing_request, writing_refusal>
	{
		Pending.reset();
		if (Sequence == std::numeric_limits<std::uint64_t>::max() || !valid(aTarget.Owner) || aTarget.Draft.value() == 0
			|| aTarget.Hook > writing_hook::comment
			|| (aTarget.Hook == writing_hook::comment) != (aTarget.Stop.value() != 0)
			|| aTarget.Window.Profile != aTarget.Owner.Profile || aTarget.Window.Window.value() == 0 || aTarget.Window.Generation.value() == 0
			|| aTarget.Session.value() == 0 || aTarget.Agent.value() == 0 || aTarget.HookRevision == 0 || aTarget.TextGeneration == 0 || aTarget.EditorGeneration == 0
			|| aRaw.size() > maximum_tour_field_bytes || !valid_writing_utf8(aRaw) || aSelection.Spans > 1 || aSelection.Anchor < 0 || aSelection.Cursor < 0
			|| aSelection.Begin > aSelection.End || !writing_boundary(aRaw, aSelection.Begin) || !writing_boundary(aRaw, aSelection.End)
			|| (aSelection.Spans == 1 && (aSelection.Begin == aSelection.End || aSelection.Anchor == aSelection.Cursor))) {
			return std::unexpected(writing_refusal::invalid_capture);
		}
		const auto scope = aSelection.Spans == 1 ? writing_scope::selection : writing_blank(aRaw) ? writing_scope::empty_field
																								  : writing_scope::field;
		if (scope != writing_scope::selection) {
			aSelection.Begin = 0;
			aSelection.End   = aRaw.size();
		}
		writing_request request{ ++Sequence, std::move(aTarget), scope, aSelection, writing_digest(aRaw), std::move(aRaw) };
		Pending = request;
		return request;
	}
	auto scoped_writing_state::complete_outline(std::uint64_t aRequest, const writing_target& aCurrent) -> bool
	{
		if (!Pending || Pending->Id != aRequest) {
			return false;
		}
		const bool accepted = Pending->Target.Hook == writing_hook::outline && Pending->Target == aCurrent;
		Pending.reset();
		return accepted;
	}
	auto scoped_writing_state::complete(std::uint64_t aRequest, const writing_target& aCurrent, std::string_view aRaw, std::string aReplacement) -> std::expected<writing_edit, writing_refusal>
	{
		if (!Pending || Pending->Id != aRequest) {
			return std::unexpected(writing_refusal::duplicate);
		}
		auto request = std::move(*Pending);
		Pending.reset();
		if (request.Target.Hook == writing_hook::outline || request.Target != aCurrent || request.RawDigest != writing_digest(aRaw) || request.Raw != aRaw) {
			return std::unexpected(writing_refusal::stale);
		}
		auto text = splice_writing_result(aRaw, request.Selection.Begin, request.Selection.End, aReplacement);
		if (!text) {
			return std::unexpected(text.error());
		}
		return writing_edit{ std::move(request), std::move(aReplacement), std::move(*text) };
	}
	auto splice_writing_result(std::string_view aRaw, std::size_t aBegin, std::size_t aEnd, std::string_view aReplacement) -> std::expected<std::string, writing_refusal>
	{
		if (aBegin > aEnd || aEnd > aRaw.size() || !writing_boundary(aRaw, aBegin) || !writing_boundary(aRaw, aEnd)) {
			return std::unexpected(writing_refusal::invalid_capture);
		}
		if (!valid_writing_utf8(aReplacement) || aReplacement.size() > maximum_tour_field_bytes || aRaw.size() - (aEnd - aBegin) > maximum_tour_field_bytes - aReplacement.size()) {
			return std::unexpected(writing_refusal::invalid_result);
		}
		if (aRaw.substr(aBegin, aEnd - aBegin) == aReplacement) {
			return std::unexpected(writing_refusal::unchanged);
		}
		return std::string{ aRaw.substr(0, aBegin) } + std::string{ aReplacement } + std::string{ aRaw.substr(aEnd) };
	}
	auto writing_quote_range(std::string_view aRaw, std::string_view aQuote) -> std::expected<std::pair<std::size_t, std::size_t>, writing_refusal>
	{
		if (aQuote.empty() || aQuote.size() > aRaw.size() || !valid_writing_utf8(aRaw) || !valid_writing_utf8(aQuote)) {
			return std::unexpected(aQuote.empty() || !valid_writing_utf8(aQuote) || !valid_writing_utf8(aRaw) ? writing_refusal::invalid_capture : writing_refusal::not_found);
		}
		std::size_t begin = std::string_view::npos, count = 0;
		for (auto at = aRaw.find(aQuote); at != std::string_view::npos; at = aRaw.find(aQuote, at + 1)) {
			if (++count > 1) {
				return std::unexpected(writing_refusal::ambiguous);
			}
			begin = at;
		}
		if (count == 0) {
			return std::unexpected(writing_refusal::not_found);
		}
		if (!writing_boundary(aRaw, begin) || !writing_boundary(aRaw, begin + aQuote.size())) {
			return std::unexpected(writing_refusal::invalid_capture);
		}
		return std::pair{ begin, begin + aQuote.size() };
	}
}
