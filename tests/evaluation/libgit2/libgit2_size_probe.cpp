/// \file
/// \brief The binary-size measurement for RFC-BPR-0083's cost record
/// ([[BPR-1458]]): one small program built twice, once running only the
/// first-party readers (K) and once also asking candidate L every question
/// (`BPR_SIZE_WITH_LIBGIT2`). The stripped difference is what L adds.

#include "bpr/core/review_cycle_git.hpp"
#include "bpr/core/review_cycle_ignore.hpp"

#if defined(BPR_SIZE_WITH_LIBGIT2)
	#include "libgit2_candidate.hpp"
#endif

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace {
	auto contents(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream stream(aPath, std::ios::binary);
		return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
	}
}

auto main(int aCount, char** aArguments) -> int
{
	const std::span<char*> arguments(aArguments, static_cast<std::size_t>(aCount));
	if (arguments.size() != 2) {
		std::cerr << "usage: libgit2_size_probe <root>\n";
		return 2;
	}
	const std::filesystem::path     root  = arguments[1];
	const std::filesystem::path     git   = root / ".git";
	const std::string               index = contents(git / "index");
	const std::vector<std::uint8_t> bytes(index.begin(), index.end());

	const auto                           format    = bpr::core::read_git_repository_format(contents(git / "config"));
	const auto                           algorithm = bpr::core::git_hash_algorithm::sha1;
	const auto                           parsed    = bpr::core::parse_git_index(bytes, algorithm, 250'000U);
	const auto                           head      = bpr::core::parse_git_head(contents(git / "HEAD"), algorithm);
	bpr::core::workspace_ignore_rules    ignores(false);
	bpr::core::workspace_attribute_rules attributes(false);
	ignores.add("", contents(root / ".gitignore"));
	attributes.add("", contents(root / ".gitattributes"));
	std::cout << "K format " << format.has_value() << " index " << parsed.has_value() << " head " << head.Reference.has_value() << " ignored " << ignores.ignored("a.log", false) << "\n";

#if defined(BPR_SIZE_WITH_LIBGIT2)
	const auto                                                       candidate = bpr::tests::workspace_contract::make_libgit2_candidate();
	const std::vector<bpr::tests::workspace_contract::path_question> paths{ { .Path = "a.log", .Directory = false } };
	std::cout << "L format " << candidate->format(root).has_value() << " index " << candidate->index(root).has_value() << " head " << candidate->head(root).has_value()
			  << " reference " << candidate->reference(root, "refs/heads/main").has_value() << " discover " << candidate->discover(root).has_value() << " ignored "
			  << candidate->ignored(root, paths).has_value() << " filter " << candidate->filter(root, paths).has_value() << "\n";
#endif
	return 0;
}
