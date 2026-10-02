#include "bpr/core/review_cycle_command.hpp"

#include <algorithm>
#include <array>

namespace bpr::core {
	namespace {
		using namespace std::string_view_literals;

		constexpr auto typed = std::array{
			cycle_command::list,
			cycle_command::open,
			cycle_command::show,
			cycle_command::capture,
			cycle_command::note,
			cycle_command::review,
			cycle_command::freeze,
			cycle_command::export_round,
			cycle_command::compare,
			cycle_command::response_import,
			cycle_command::round_grant,
			cycle_command::round_revoke,
			cycle_command::round_read,
			cycle_command::round_page,
			cycle_command::round_source,
			cycle_command::response_append,
			cycle_command::policy,
		};
		constexpr auto common       = std::array{ "--json"sv, "--non-interactive"sv, "--schema-version"sv, "--data-root"sv, "--profile"sv, "--operation-id"sv };
		constexpr auto flags        = std::array{ "--json"sv, "--non-interactive"sv, "--again"sv, "--changed-only"sv, "--read-only"sv };
		constexpr auto none         = std::array<std::string_view, 0>{ };
		constexpr auto list_options = std::array{ "--cursor"sv };
		constexpr auto capture      = std::array{ "--again"sv };
		constexpr auto note         = std::array{ "--file"sv, "--lines"sv, "--body-file"sv };
		constexpr auto review       = std::array{ "--include"sv, "--exclude"sv, "--earlier"sv, "--later"sv, "--action"sv, "--goal-file"sv, "--context"sv, "--context-lines"sv, "--drop-context"sv, "--expected-revision"sv };
		constexpr auto freeze       = std::array{ "--expected-revision"sv };
		constexpr auto exporting    = std::array{ "--round"sv, "--output"sv, "--format"sv, "--replace"sv };
		constexpr auto compare      = std::array{ "--from"sv, "--to"sv, "--changed-only"sv, "--cursor"sv, "--path"sv };
		constexpr auto response     = std::array{ "--round"sv, "--file"sv };
		constexpr auto record       = std::array{ "--round"sv, "--state"sv, "--digest"sv };
		constexpr auto grant        = std::array{ "--round"sv, "--read-only"sv };
		constexpr auto round_only   = std::array{ "--round"sv };
		constexpr auto reading      = std::array{ "--round"sv, "--page-bytes"sv, "--budget-bytes"sv };
		constexpr auto paging       = std::array{ "--round"sv, "--cursor"sv };
		constexpr auto sourcing     = std::array{ "--round"sv, "--source"sv, "--range"sv, "--cursor"sv };
		constexpr auto policy       = std::array{ "--classification"sv, "--channel"sv, "--permission"sv };
	}

	auto name(const cycle_command aCommand) noexcept -> std::string_view
	{
		switch (aCommand) {
		case cycle_command::list:            return "list";
		case cycle_command::open:            return "open";
		case cycle_command::show:            return "show";
		case cycle_command::capture:         return "capture";
		case cycle_command::note:            return "note";
		case cycle_command::review:          return "review";
		case cycle_command::freeze:          return "freeze";
		case cycle_command::export_round:    return "export";
		case cycle_command::compare:         return "compare";
		case cycle_command::response_import: return "response import";
		case cycle_command::export_record:   return "export record";
		case cycle_command::round_grant:     return "round grant";
		case cycle_command::round_revoke:    return "round revoke";
		case cycle_command::round_read:      return "round read";
		case cycle_command::round_page:      return "round page";
		case cycle_command::round_source:    return "round source";
		case cycle_command::response_append: return "response append";
		case cycle_command::policy:          return "policy";
		}
		return "unknown";
	}

	auto parse_cycle_command(const std::string_view aName) noexcept -> std::optional<cycle_command>
	{
		const auto found = std::ranges::find(typed, aName, [](const cycle_command aCommand) { return name(aCommand); });
		return found == typed.end() ? std::nullopt : std::optional{ *found };
	}

	auto typed_cycle_commands() noexcept -> std::span<const cycle_command>
	{
		return typed;
	}

	auto cycle_command_options(const cycle_command aCommand) noexcept -> std::span<const std::string_view>
	{
		switch (aCommand) {
		case cycle_command::list:            return list_options;
		case cycle_command::capture:         return capture;
		case cycle_command::note:            return note;
		case cycle_command::review:          return review;
		case cycle_command::freeze:          return freeze;
		case cycle_command::export_round:    return exporting;
		case cycle_command::compare:         return compare;
		case cycle_command::response_import: return response;
		case cycle_command::export_record:   return record;
		case cycle_command::round_grant:     return grant;
		case cycle_command::round_revoke:    return round_only;
		case cycle_command::round_read:      return reading;
		case cycle_command::round_page:      return paging;
		case cycle_command::round_source:    return sourcing;
		case cycle_command::response_append: return response;
		case cycle_command::policy:          return policy;
		case cycle_command::open:
		case cycle_command::show:            return none;
		}
		return none;
	}

	auto cycle_option_is_flag(const std::string_view aOption) noexcept -> bool
	{
		return std::ranges::find(flags, aOption) != flags.end();
	}

	auto common_cycle_options() noexcept -> std::span<const std::string_view>
	{
		return common;
	}

	auto may_write(const cycle_command aCommand) noexcept -> bool
	{
		switch (aCommand) {
		case cycle_command::list:
		case cycle_command::show:
		case cycle_command::compare:
		case cycle_command::round_read:
		case cycle_command::round_page:
		case cycle_command::round_source: return false;
		default:                          return true;
		}
	}

	auto authority_of(const cycle_command aCommand) noexcept -> cycle_command_authority
	{
		switch (aCommand) {
		case cycle_command::round_read:
		case cycle_command::round_page:
		case cycle_command::round_source:
		case cycle_command::response_append: return cycle_command_authority::granted_consumer;
		default:                             return cycle_command_authority::reviewer;
		}
	}

	auto touches_round_draft(const cycle_command aCommand) noexcept -> bool
	{
		return aCommand == cycle_command::review || aCommand == cycle_command::freeze;
	}

	auto valid_cycle_operation_id(const std::string_view aOperation) noexcept -> bool
	{
		return !aOperation.empty() && aOperation.size() <= maximum_cycle_operation_id_bytes && std::ranges::all_of(aOperation, [](const char aCharacter) {
			return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-';
		});
	}

	auto admit_owner_command(const cycle_command aCommand, const cycle_draft_editor aEditor) noexcept -> cycle_owner_admission
	{
		if (!touches_round_draft(aCommand) || aEditor == cycle_draft_editor::settled) {
			return cycle_owner_admission::serve;
		}
		return aEditor == cycle_draft_editor::composing ? cycle_owner_admission::busy : cycle_owner_admission::save_first;
	}
}
