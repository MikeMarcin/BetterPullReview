#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_workspace.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle workspace failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename TIdentity>
	auto identity(const std::uint8_t aSeed) -> TIdentity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		return TIdentity{ bytes };
	}

	auto object(const std::uint64_t aObject) -> workspace_object_identity
	{
		return { .Volume = 7, .ObjectLow = aObject };
	}

	auto folder(const std::string& aLocation, const std::uint64_t aObject) -> workspace_root_observation
	{
		return { .Kind = workspace_root_kind::folder, .Location = aLocation, .Root = object(aObject) };
	}

	auto checkout(const std::string& aLocation, const std::uint64_t aObject, const std::uint64_t aRepository, const std::uint64_t aWorktree)
		-> workspace_root_observation
	{
		return { .Kind = workspace_root_kind::git_checkout, .Location = aLocation, .Root = object(aObject), .Repository = object(aRepository), .Worktree = object(aWorktree) };
	}

	auto make_cycle(const std::vector<std::string>& aPaths = { }) -> review_cycle_state
	{
		const auto authority = local_cycle_authority{ .Profile = bpr_profile_id{ 1 }, .Incarnation = identity<cycle_catalog_incarnation>(1) };
		const auto created   = create({ .Authority = authority }, { .Authority = authority, .Title = "Workspace", .Scope = { .Paths = aPaths } });
		expect(created.has_value(), "create");
		return created->Cycle;
	}

	auto run(const review_cycle_state& aCycle, review_cycle_change aChange) -> review_cycle_outcome
	{
		return apply(aCycle, { .Subject = aCycle.Subject, .ExpectedRevision = aCycle.Revision, .Change = std::move(aChange) });
	}

	auto capture(const std::uint64_t aWorkspaceVersion, const std::uint8_t aMaterial) -> record_cycle_capture
	{
		return { .Capture = { .ScopeVersion = 1, .Material = identity<cycle_material_digest>(aMaterial), .Entries = 1, .Coverage = cycle_capture_coverage::complete, .WorkspaceVersion = aWorkspaceVersion } };
	}

	void admits_only_root_relative_paths()
	{
		expect(admit_workspace_path("src/main.cpp") == workspace_path_refusal::none, "ordinary path");
		expect(admit_workspace_path("dir with space/file name.txt") == workspace_path_refusal::none, "spaces admitted as bytes");
		expect(admit_workspace_path("back\\slash") == workspace_path_refusal::none, "a POSIX name byte, not a separator");
		expect(admit_workspace_path(std::string_view{ "raw\xFF"
													  "bytes" })
				   == workspace_path_refusal::none,
			   "non-UTF-8 bytes admitted unnormalized");
		expect(admit_workspace_path("") == workspace_path_refusal::empty, "empty");
		expect(admit_workspace_path("/etc/passwd") == workspace_path_refusal::absolute, "absolute");
		expect(admit_workspace_path("\\\\server\\share") == workspace_path_refusal::absolute, "UNC");
		expect(admit_workspace_path("C:/Windows") == workspace_path_refusal::absolute, "drive");
		expect(admit_workspace_path("c:relative") == workspace_path_refusal::absolute, "drive-relative");
		expect(admit_workspace_path("../outside") == workspace_path_refusal::invalid_segment, "parent segment");
		expect(admit_workspace_path("a/../b") == workspace_path_refusal::invalid_segment, "inner parent segment");
		expect(admit_workspace_path("a/./b") == workspace_path_refusal::invalid_segment, "dot segment");
		expect(admit_workspace_path("a//b") == workspace_path_refusal::invalid_segment, "empty segment");
		expect(admit_workspace_path("a/") == workspace_path_refusal::invalid_segment, "trailing separator");
		expect(admit_workspace_path(std::string_view{ "a\0b", 3 }) == workspace_path_refusal::nul, "NUL");
		expect(admit_workspace_path(".git/config") == workspace_path_refusal::repository_internal, ".git");
		expect(admit_workspace_path("sub/.GIT/hooks/pre-commit") == workspace_path_refusal::repository_internal, ".git in any case");
		expect(admit_workspace_path(".gitignore") == workspace_path_refusal::none, ".gitignore is material");
		expect(admit_workspace_path(std::string(maximum_workspace_path_bytes + 1, 'a')) == workspace_path_refusal::too_long, "too long");
	}

	void labels_and_limits_portability()
	{
		expect(path_portability("src/main.cpp").portable(), "portable");
		expect(path_portability(std::string_view{ "raw\xFF" }).NonUtf8, "non-UTF-8");
		expect(path_portability(std::string_view{ "\xED\xA0\x80" }).NonUtf8, "encoded surrogate is not UTF-8");
		expect(path_portability(std::string_view{ "\xC0\xAF" }).NonUtf8, "overlong form is not UTF-8");
		expect(path_portability("caf\xC3\xA9.txt").portable(), "UTF-8 is portable");
		expect(path_portability("docs/CON").WindowsReservedName, "CON");
		expect(path_portability("com1.txt").WindowsReservedName, "COM1 with extension");
		expect(path_portability("Lpt9").WindowsReservedName, "LPT9 any case");
		expect(!path_portability("console.txt").WindowsReservedName, "prefix is not reserved");
		expect(!path_portability("com10").WindowsReservedName, "COM10 is not reserved");
		expect(path_portability("a:b").WindowsForbiddenCharacter, "colon");
		expect(path_portability("a\\b").WindowsForbiddenCharacter, "backslash");
		expect(path_portability("tab\there").WindowsForbiddenCharacter, "control character");
		expect(path_portability("name.").TrailingDotOrSpace && path_portability("dir /x").TrailingDotOrSpace, "trailing dot or space");

		expect(workspace_path_label("src/main.cpp") == "src/main.cpp", "label unchanged");
		expect(workspace_path_label("caf\xC3\xA9") == "caf\xC3\xA9", "UTF-8 label unchanged");
		expect(workspace_path_label(std::string_view{ "raw\xFF" }) == "raw\\xFF", "invalid byte escaped");
		expect(workspace_path_label("a\\x41") == "a\\x5Cx41", "a literal escape stays distinguishable");
		expect(workspace_path_label("new\nline") == "new\\x0Aline", "control escaped");
	}

	void finds_collisions_without_merging()
	{
		const auto nfc   = std::string{ "caf\xC3\xA9.txt" };
		const auto nfd   = std::string{ "cafe\xCC\x81.txt" };
		const auto paths = std::vector<std::string>{
			"README.md", // 0
			"readme.md", // 1: case
			nfc, // 2
			nfd, // 3: normalization
			"CAF\xC3\x89.txt", // 4: case of 2 and case plus composition of 3
			"src/Main.cpp", // 5
			"src/main.cpp", // 6: case in last segment
			"other.txt", // 7
			"\xCE\xA3\xCE\xB1", // 8: Greek capital sigma, alpha
			"\xCF\x83\xCE\xB1", // 9: Greek small sigma, alpha
			std::string{ "x\xFF" }, // 10
			std::string{ "X\xFF" }, // 11: ASCII case around an invalid byte
		};
		const auto found = find_path_collisions(paths);
		const auto has   = [&found](const std::size_t aFirst, const std::size_t aSecond, const workspace_collision_kind aKind) {
			for (const auto& collision : found) {
				if (collision.First == aFirst && collision.Second == aSecond && collision.Kind == aKind) {
					return true;
				}
			}
			return false;
		};
		expect(has(0, 1, workspace_collision_kind::case_fold), "README case collision");
		expect(has(2, 3, workspace_collision_kind::unicode_normalization), "NFC/NFD collision");
		expect(has(2, 4, workspace_collision_kind::case_fold), "precomposed case collision");
		expect(has(3, 4, workspace_collision_kind::unicode_normalization), "case plus composition is normalization");
		expect(has(5, 6, workspace_collision_kind::case_fold), "nested case collision");
		expect(has(8, 9, workspace_collision_kind::case_fold), "Greek case collision");
		expect(has(10, 11, workspace_collision_kind::case_fold), "invalid bytes kept, ASCII folded");
		expect(found.size() == 7, "no other pair collides");
		expect(workspace_collision_key(nfc) == workspace_collision_key(nfd), "one key for both spellings");
		expect(workspace_collision_key(std::string_view{ "\xC3" }) != workspace_collision_key("\xC3\x83"), "a raw byte never equals a scalar");

		// Identical spellings are one path, not a collision.
		const auto same = std::vector<std::string>{ "a.txt", "a.txt" };
		expect(find_path_collisions(same).empty(), "duplicate is not a collision");
	}

	void verifies_by_identity_not_spelling()
	{
		const auto bound = cycle_workspace_binding{ .Project = identity<workspace_project_id>(1), .Version = 1, .Root = checkout("/work/a/project", 10, 11, 12) };
		expect(valid(bound), "binding valid");
		expect(verify_workspace(bound, checkout("/work/a/project", 10, 11, 12)) == workspace_verification::matches, "same root");
		expect(verify_workspace(bound, checkout("/elsewhere/project", 10, 11, 12)) == workspace_verification::matches, "moved root keeps identity");
		expect(verify_workspace(bound, checkout("/work/a/project", 20, 11, 12)) == workspace_verification::replaced, "replaced root");
		expect(verify_workspace(bound, checkout("/work/b/project", 30, 31, 32)) == workspace_verification::replaced, "same-named other checkout");
		expect(verify_workspace(bound, checkout("/work/a/project", 10, 11, 13)) == workspace_verification::repository_changed, "worktree changed");
		expect(verify_workspace(bound, folder("/work/a/project", 10)) == workspace_verification::kind_changed, "repository removed");
		auto file = folder("/work/a/project", 10);
		file.Kind = workspace_root_kind::file;
		expect(verify_workspace(bound, file) == workspace_verification::kind_changed, "folder became a file");

		expect(!valid(folder("", 1)), "location required");
		expect(!valid(workspace_root_observation{ .Kind = workspace_root_kind::folder, .Location = "/x" }), "root identity required");
		auto half     = checkout("/x", 1, 2, 3);
		half.Worktree = std::nullopt;
		expect(!valid(half), "Git roots name both directories");
		auto plain       = folder("/x", 1);
		plain.Repository = object(2);
		expect(!valid(plain), "plain roots name no repository");
	}

	void resolves_imports_only_through_explicit_mappings()
	{
		const auto imported = identity<workspace_project_id>(5);
		const auto local    = identity<workspace_project_id>(6);
		expect(!resolve_imported_project({ }, imported), "no mapping, no source");
		const auto mappings = std::array{ workspace_import_mapping{ .Imported = identity<workspace_project_id>(9), .Local = local } };
		expect(!resolve_imported_project(mappings, imported), "another project's mapping does not apply");
		const auto mapped = std::array{ workspace_import_mapping{ .Imported = imported, .Local = local } };
		expect(resolve_imported_project(mapped, imported) == local, "explicit mapping resolves");
		expect(!resolve_imported_project(mapped, workspace_project_id{ }), "zero identity never resolves");
	}

	void binds_and_rebinds_explicitly()
	{
		const auto cycle = make_cycle();
		const auto first = run(cycle, bind_cycle_workspace{ .Root = checkout("/work/a/project", 10, 11, 12) });
		expect(first.Refusal == cycle_refusal::none && first.Changed, "first bind needs no confirmation before capture");
		const auto& bound = *first.Accepted.Workspace;
		expect(bound.Version == 1 && bound.Project == derive_workspace_project(cycle.Subject) && !bound.Admission, "version 1, derived project");
		expect(project(first.Accepted).WorkspaceKind == workspace_root_kind::git_checkout && project(first.Accepted).WorkspaceVersion == 1, "projection");

		const auto again = run(first.Accepted, bind_cycle_workspace{ .Root = checkout("/work/a/project", 10, 11, 12) });
		expect(again.Refusal == cycle_refusal::none && !again.Changed, "same root, same location is a no-op");

		// Captures name the binding version they were read under.
		const auto stale = run(first.Accepted, capture(0, 1));
		expect(stale.Refusal == cycle_refusal::stale_workspace, "unbound capture refused once bound");
		const auto captured = run(first.Accepted, capture(1, 1));
		expect(captured.Refusal == cycle_refusal::none && captured.Accepted.Latest->Capture.WorkspaceVersion == 1, "capture under version 1");

		// Another checkout with the same name is not silently the subject.
		const auto other       = checkout("/work/b/project", 30, 31, 32);
		const auto unconfirmed = run(captured.Accepted, bind_cycle_workspace{ .Root = other });
		expect(unconfirmed.Refusal == cycle_refusal::unconfirmed, "rebind needs confirmation");
		const auto unchecked = run(captured.Accepted, bind_cycle_workspace{ .Root = other, .Confirmed = true });
		expect(unchecked.Refusal == cycle_refusal::unconfirmed, "rebind over material needs a check");
		const auto check   = workspace_material_check{ .Compared = 1, .Matching = 0, .Differing = 1 };
		const auto rebound = run(captured.Accepted, bind_cycle_workspace{ .Root = other, .Check = check, .Confirmed = true });
		expect(rebound.Refusal == cycle_refusal::none && rebound.Accepted.Workspace->Version == 2, "confirmed rebind");
		expect(rebound.Accepted.Workspace->Project == bound.Project && rebound.Accepted.Workspace->Admission == check, "project kept, check recorded");
		expect(run(rebound.Accepted, capture(1, 2)).Refusal == cycle_refusal::stale_workspace, "bytes from the old binding refused");
		expect(run(rebound.Accepted, capture(2, 2)).Refusal == cycle_refusal::none, "bytes from the new binding accepted");

		// A moved root keeps its identity but is still an explicit rebind.
		const auto moved = run(captured.Accepted, bind_cycle_workspace{ .Root = checkout("/moved/project", 10, 11, 12) });
		expect(moved.Refusal == cycle_refusal::unconfirmed, "moved root needs confirmation");

		// A folder binding never becomes a file binding.
		auto file = folder("/work/a/project/main.cpp", 10);
		file.Kind = workspace_root_kind::file;
		expect(run(captured.Accepted, bind_cycle_workspace{ .Root = file, .Check = check, .Confirmed = true }).Refusal == cycle_refusal::invalid_workspace,
			   "folder to file refused");

		// Rebinding a bound cycle with no material still needs confirmation.
		const auto bare = run(first.Accepted, bind_cycle_workspace{ .Root = other });
		expect(bare.Refusal == cycle_refusal::unconfirmed, "rebind before capture needs confirmation");
		expect(run(first.Accepted, bind_cycle_workspace{ .Root = other, .Confirmed = true }).Refusal == cycle_refusal::none, "confirmed without material needs no check");
	}

	void binds_single_files_to_the_whole_scope()
	{
		auto file = folder("/work/notes.md", 40);
		file.Kind = workspace_root_kind::file;
		expect(run(make_cycle({ "src" }), bind_cycle_workspace{ .Root = file }).Refusal == cycle_refusal::invalid_workspace, "a file root takes no path scope");
		const auto bound = run(make_cycle(), bind_cycle_workspace{ .Root = file });
		expect(bound.Refusal == cycle_refusal::none, "a file root binds");
		expect(run(bound.Accepted, change_cycle_scope{ .Paths = { "src" } }).Refusal == cycle_refusal::invalid_scope, "and keeps its whole scope");

		// A capture recorded before any binding makes the first bind explicit.
		const auto unbound = run(make_cycle(), capture(0, 3));
		expect(unbound.Refusal == cycle_refusal::none, "capture without a binding");
		expect(run(unbound.Accepted, bind_cycle_workspace{ .Root = folder("/x", 1) }).Refusal == cycle_refusal::unconfirmed, "binding over material needs confirmation");
		expect(run(unbound.Accepted, bind_cycle_workspace{ .Root = folder("/x", 1), .Check = workspace_material_check{ }, .Confirmed = true }).Refusal == cycle_refusal::none,
			   "confirmed bind over material");
	}

	void derives_projects_per_cycle()
	{
		const auto cycle = make_cycle();
		auto       other = cycle.Subject;
		other.Cycle      = review_cycle_id{ 2 };
		expect(derive_workspace_project(cycle.Subject) == derive_workspace_project(cycle.Subject), "deterministic");
		expect(derive_workspace_project(cycle.Subject) != derive_workspace_project(other), "distinct per cycle");
		expect(derive_workspace_project(cycle.Subject).valid(), "valid");
	}
}

int main()
{
	admits_only_root_relative_paths();
	labels_and_limits_portability();
	finds_collisions_without_merging();
	verifies_by_identity_not_spelling();
	resolves_imports_only_through_explicit_mappings();
	binds_and_rebinds_explicitly();
	binds_single_files_to_the_whole_scope();
	derives_projects_per_cycle();
	return EXIT_SUCCESS;
}
