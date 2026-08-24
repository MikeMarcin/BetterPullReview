// The Windows automated-test boundary is an AppContainer, and an owner-private
// catalog object that names only the token user is unreachable from inside one:
// a lowbox access check additionally requires the object's DACL to grant the
// container's own package SID. This exercises the product's own descriptor and
// the product's own validator through a `BPR_SQLITE_TESTING` seam, so a pass is
// evidence about `catalog_database.cpp` rather than about a copy of its rule
// kept in the suite.
//
// The same binary is meaningful either way. Contained it must observe two
// access-allowed entries; uncontained it must observe exactly one, which is the
// negative control that keeps the contained result about the boundary rather
// than about the machine.
#include "bpr/adapters/sqlite/testing.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

#if !defined(NOMINMAX)
	#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace {
	namespace seam = bpr::adapters::sqlite::detail::testing;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	// Asked of the token directly rather than of the seam, so the expected shape
	// and the observed shape do not come from the same source.
	[[nodiscard]] auto process_is_contained() -> bool
	{
		HANDLE token = nullptr;
		expect(
			::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token) != 0,
			"the process token opens for query");
		DWORD      contained = 0U;
		DWORD      returned  = 0U;
		const auto queried   = ::GetTokenInformation(
			token, TokenIsAppContainer, &contained, sizeof(contained), &returned);
		(void)::CloseHandle(token);
		expect(queried != 0, "TokenIsAppContainer answers for this process");
		return contained != 0U;
	}
} // namespace

auto main(int aArgumentCount, char** aArguments) -> int
{
	expect(
		aArgumentCount == 2,
		"a writable scratch parent directory is the only argument");
	const auto contained = process_is_contained();
	const auto scratch   = std::filesystem::path{ aArguments[1] }
		/ ("bpr-private-descriptor-"
		   + std::to_string(static_cast<std::uint64_t>(::GetCurrentProcessId())));

	std::error_code error;
	(void)std::filesystem::remove_all(scratch, error);
	(void)std::filesystem::create_directories(scratch, error);
	expect(!error, "the scratch directory is created below the build tree");

	const auto      probe = seam::probe_private_descriptor(scratch);
	std::error_code ignored;
	(void)std::filesystem::remove_all(scratch, ignored);

	std::cout << "appcontainer-private-descriptor:"
			  << " token_contained=" << (contained ? 1 : 0)
			  << " product_contained=" << (probe.Contained ? 1 : 0)
			  << " created=" << (probe.Created ? 1 : 0)
			  << " usable=" << (probe.Usable ? 1 : 0)
			  << " directory_valid=" << (probe.DirectoryValid ? 1 : 0)
			  << " file_valid=" << (probe.FileValid ? 1 : 0)
			  << " directory_ace_count=" << probe.DirectoryAceCount
			  << " file_ace_count=" << probe.FileAceCount
			  << " last_error=" << probe.LastError << '\n';

	expect(
		probe.Contained == contained,
		"the product reads the containment the token reports");
	expect(probe.Created, "an owner-private catalog directory is created");
	expect(
		probe.Usable,
		"an owner-private catalog file is writable by the process that made it");
	expect(
		probe.DirectoryValid,
		"the product validator accepts the directory descriptor it wrote");
	expect(
		probe.FileValid,
		"the product validator accepts the file descriptor it wrote");
	const auto expected_ace_count = contained ? std::uint32_t{ 2U } : std::uint32_t{ 1U };
	expect(
		probe.DirectoryAceCount == expected_ace_count,
		"the directory grants the token user, and the container only inside one");
	expect(
		probe.FileAceCount == expected_ace_count,
		"the file grants the token user, and the container only inside one");

	std::cout << "appcontainer-private-descriptor-tests:pass\n";
	return EXIT_SUCCESS;
}
