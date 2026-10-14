/// \file
/// \brief Asks candidate L one C1-C6 question about one checkout and prints
/// the answer as NUL-terminated fields, for the macOS evidence course
/// (`libgit2_course.py`) until RFC-BPR-0083's harness runs L itself.
/// \details Usage: `libgit2_probe <index|format|head|discover|ignored|filter>
/// <root>` or `libgit2_probe reference <root> <name>`. `ignored` and `filter`
/// read questions from standard input as NUL-terminated records whose first
/// byte is `d` (directory) or `f` (file). The first field is `ok` or
/// `refused` followed by the reason.

#include "libgit2_candidate.hpp"

#include <cstdio>
#include <expected>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::tests::workspace_contract;

	void field(const std::string_view aText)
	{
		std::cout.write(aText.data(), static_cast<std::streamsize>(aText.size()));
		std::cout.put('\0');
	}

	void flag(const bool aValue)
	{
		field(aValue ? "1" : "0");
	}

	void optional_field(const std::optional<std::string>& aValue)
	{
		flag(aValue.has_value());
		field(aValue.value_or(std::string()));
	}

	auto questions() -> std::vector<path_question>
	{
		const std::string          input{ std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>() };
		std::vector<path_question> result;
		std::size_t                start = 0;
		while (start < input.size()) {
			const std::size_t end    = input.find('\0', start);
			const std::string record = input.substr(start, end == std::string::npos ? std::string::npos : end - start);
			if (!record.empty()) {
				result.push_back({ .Path = record.substr(1), .Directory = record.front() == 'd' });
			}
			if (end == std::string::npos) {
				break;
			}
			start = end + 1;
		}
		return result;
	}

	template <typename TValue>
	auto refused(const std::expected<TValue, candidate_refusal>& aAnswer) -> bool
	{
		if (aAnswer) {
			field("ok");
			return false;
		}
		field("refused");
		field(aAnswer.error());
		return true;
	}

	auto run(git_metadata_candidate& aCandidate, const std::string_view aQuestion, const std::filesystem::path& aRoot, const std::span<char*> aRest) -> int
	{
		if (aQuestion == "index") {
			const auto answer = aCandidate.index(aRoot);
			if (!refused(answer)) {
				flag(answer->Partial);
				field(std::to_string(answer->Entries.size()));
				for (const index_answer_entry& entry : answer->Entries) {
					field(entry.Path);
					field(std::to_string(entry.Mode));
					field(entry.Object);
					field(std::to_string(entry.Stage));
					flag(entry.SkipWorktree);
					flag(entry.IntentToAdd);
				}
			}
			return 0;
		}
		if (aQuestion == "format") {
			const auto answer = aCandidate.format(aRoot);
			if (!refused(answer)) {
				field(answer->ObjectFormat);
				field(answer->ReferenceFormat);
				flag(answer->IgnoreCase);
			}
			return 0;
		}
		if (aQuestion == "head") {
			const auto answer = aCandidate.head(aRoot);
			if (!refused(answer)) {
				optional_field(answer->Symbolic);
				optional_field(answer->Commit);
			}
			return 0;
		}
		if (aQuestion == "reference" && aRest.size() == 1) {
			const auto answer = aCandidate.reference(aRoot, aRest.front());
			if (!refused(answer)) {
				optional_field(*answer);
			}
			return 0;
		}
		if (aQuestion == "discover") {
			const auto answer = aCandidate.discover(aRoot);
			if (!refused(answer)) {
				field(answer->GitDirectory.string());
				field(answer->CommonDirectory.string());
				flag(answer->LinkedWorktree);
			}
			return 0;
		}
		if (aQuestion == "ignored") {
			const auto answer = aCandidate.ignored(aRoot, questions());
			if (!refused(answer)) {
				for (const bool ignored : *answer) {
					flag(ignored);
				}
			}
			return 0;
		}
		if (aQuestion == "filter") {
			const auto answer = aCandidate.filter(aRoot, questions());
			if (!refused(answer)) {
				for (const attribute_answer& value : *answer) {
					field(value);
				}
			}
			return 0;
		}
		std::cerr << "usage: libgit2_probe <index|format|head|discover|ignored|filter> <root> | reference <root> <name>\n";
		return 2;
	}
}

auto main(int aCount, char** aArguments) -> int
{
	const std::span<char*> arguments(aArguments, static_cast<std::size_t>(aCount));
	if (arguments.size() < 3) {
		std::cerr << "usage: libgit2_probe <question> <root> [name]\n";
		return 2;
	}
	const auto candidate = make_libgit2_candidate();
	const int  status    = run(*candidate, arguments[1], arguments[2], arguments.subspan(3));
	std::cout.flush();
	return status;
}
