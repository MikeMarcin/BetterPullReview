#include "bpr/core/review_cycle_command.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle command failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto takes(const cycle_command aCommand, const std::string_view aOption) -> bool
	{
		const auto options = cycle_command_options(aCommand);
		return std::ranges::find(options, aOption) != options.end();
	}

	void names_round_trip()
	{
		for (const auto command : typed_cycle_commands()) {
			expect(parse_cycle_command(name(command)) == command, "every typed command parses by its own name");
		}
		expect(parse_cycle_command("response import") == cycle_command::response_import, "a two-word command");
		expect(parse_cycle_command("export") == cycle_command::export_round, "export is typed as export");
		// The record a terminal makes after writing an export is not something a person types.
		expect(!parse_cycle_command("export record"), "export record is not typed");
		expect(std::ranges::find(typed_cycle_commands(), cycle_command::export_record) == typed_cycle_commands().end(), "nor listed");
		expect(!parse_cycle_command("publish") && !parse_cycle_command("") && !parse_cycle_command("Show"), "unknown names are refused exactly");
	}

	void options_are_closed()
	{
		expect(takes(cycle_command::freeze, "--expected-revision") && !takes(cycle_command::freeze, "--yes"), "freeze names a revision and has no bypass");
		expect(takes(cycle_command::review, "--goal-file") && !takes(cycle_command::review, "--force"), "review edits are explicit");
		expect(cycle_command_options(cycle_command::show).empty() && cycle_command_options(cycle_command::open).empty(), "show and open take only the common options");
		expect(takes(cycle_command::export_record, "--digest") && takes(cycle_command::export_record, "--state"), "a record names the bytes and what happened to them");
		// [[BPR-1279]]. A rebind names where to and the check it confirms; nothing else stands in for that.
		expect(takes(cycle_command::rebind, "--to") && takes(cycle_command::rebind, "--confirm") && cycle_command_options(cycle_command::rebind).size() == 2U, "rebind names a location and a confirmation");
		expect(!takes(cycle_command::rebind, "--yes") && !takes(cycle_command::rebind, "--force") && !cycle_option_is_flag("--confirm"), "and a confirmation is a value, never a flag");
		expect(parse_cycle_command("rebind") == cycle_command::rebind && last_cycle_command == cycle_command::rebind, "rebind is typed and the registry's last");
		expect(may_write(cycle_command::rebind) && authority_of(cycle_command::rebind) == cycle_command_authority::reviewer, "a rebind writes, on the reviewer's authority");
		expect(admit_owner_command(cycle_command::rebind, cycle_draft_editor::composing) == cycle_owner_admission::serve, "and is not held by a draft");
		const auto common = common_cycle_options();
		expect(std::ranges::find(common, std::string_view{ "--operation-id" }) != common.end(), "every command takes an operation identity");
		expect(cycle_option_is_flag("--json") && cycle_option_is_flag("--again") && !cycle_option_is_flag("--round"), "flags take no value");
	}

	void classifies_effects()
	{
		expect(!may_write(cycle_command::list) && !may_write(cycle_command::show) && !may_write(cycle_command::compare), "reads change nothing");
		expect(may_write(cycle_command::note) && may_write(cycle_command::freeze) && may_write(cycle_command::export_record), "writes are writes");
		expect(touches_round_draft(cycle_command::review) && touches_round_draft(cycle_command::freeze), "review and freeze read the draft");
		expect(!touches_round_draft(cycle_command::note) && !touches_round_draft(cycle_command::export_round), "a note or a frozen round is not the draft");
	}

	void operation_identities()
	{
		expect(valid_cycle_operation_id("a") && valid_cycle_operation_id("retry-7-B"), "letters, digits and hyphens");
		expect(valid_cycle_operation_id(std::string(maximum_cycle_operation_id_bytes, 'x')), "at the bound");
		expect(!valid_cycle_operation_id(std::string(maximum_cycle_operation_id_bytes + 1U, 'x')), "over the bound");
		expect(!valid_cycle_operation_id("") && !valid_cycle_operation_id("a b") && !valid_cycle_operation_id("a/b") && !valid_cycle_operation_id("é"), "nothing else");
	}

	void admission_beside_an_open_draft()
	{
		// Committed text is saved first, so a freeze can neither omit it nor overwrite it unseen.
		expect(admit_owner_command(cycle_command::freeze, cycle_draft_editor::unsaved) == cycle_owner_admission::save_first, "freeze saves the author's text first");
		expect(admit_owner_command(cycle_command::review, cycle_draft_editor::unsaved) == cycle_owner_admission::save_first, "so does reading the draft");
		// Composing text is not text yet: it is neither saved nor read past.
		expect(admit_owner_command(cycle_command::freeze, cycle_draft_editor::composing) == cycle_owner_admission::busy, "composition holds a freeze");
		expect(admit_owner_command(cycle_command::review, cycle_draft_editor::composing) == cycle_owner_admission::busy, "and a draft read");
		expect(admit_owner_command(cycle_command::freeze, cycle_draft_editor::settled) == cycle_owner_admission::serve, "a settled draft is served");
		// Commands that do not touch the draft are not held by it.
		for (const auto editor : { cycle_draft_editor::unsaved, cycle_draft_editor::composing }) {
			expect(admit_owner_command(cycle_command::note, editor) == cycle_owner_admission::serve, "a note is independent of the draft");
			expect(admit_owner_command(cycle_command::export_round, editor) == cycle_owner_admission::serve, "a frozen round is immutable");
			expect(admit_owner_command(cycle_command::capture, editor) == cycle_owner_admission::serve, "capture reads the folder");
		}
	}
}

auto main() -> int
{
	names_round_trip();
	options_are_closed();
	classifies_effects();
	operation_identities();
	admission_beside_an_open_draft();
	std::cout << "review cycle command tests passed\n";
	return EXIT_SUCCESS;
}
