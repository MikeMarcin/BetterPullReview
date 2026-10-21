// [[BPR-1455]] and [[BPR-1456]]. RFC-BPR-0083's differential harness: each
// corpus repository is built by the installed Git, and Git (isolated from
// every system and global file) and each candidate implementation of the Git
// metadata components C1-C6 answer the same questions. Every difference must
// be classified under H1 in the registry below, or the row fails. The canary
// course then checks that no candidate executes, opens outside its scope or
// writes. Tests run Git; the product never does.

#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_capture.hpp"
#include "bpr/application/review_cycle_words.hpp"
#include "bpr/core/review_cycle_ignore.hpp"
#include "cycle_course.hpp"
#include "git_metadata_candidate.hpp"
#include "git_metadata_oracle.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if !defined(_WIN32)
	#include <cerrno>
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::attribute_answer;
	using tests::workspace_contract::candidate_refusal;
	using tests::workspace_contract::discovery_answer;
	using tests::workspace_contract::format_answer;
	using tests::workspace_contract::git_metadata_candidate;
	using tests::workspace_contract::git_oracle;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::head_answer;
	using tests::workspace_contract::index_answer;
	using tests::workspace_contract::index_answer_entry;
	using tests::workspace_contract::path_question;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::write_file;

	// ---------------------------------------------------------------------
	// H1 classification registry.
	// ---------------------------------------------------------------------

	/// \brief Every difference a candidate shows against Git, keyed by
	/// candidate, component, case and (optionally) the path, reference or
	/// field. `fix` is a reader bug fixed with a test (kept here only while a
	/// fix is pending, so it never applies today); `refusal` is an explicit
	/// refusal with a reason; `deliberate` is a documented choice. `silent`
	/// records a candidate's silent divergence that BPR cannot fix in it: the
	/// row still passes, but under H1 the candidate is not eligible for that
	/// component, and the decision record says so.
	struct classification
	{
		std::string_view Candidate;
		std::string_view Component;
		std::string_view Case;
		/// Empty: any subject in the case.
		std::string_view Subject;
		std::string_view Kind;
		std::string_view Reason;
		/// When supplied, both observed answers must match before classification.
		std::string_view OracleAnswer{ };
		std::string_view CandidateAnswer{ };
	};

	/// Components that read the repository format first, and so refuse
	/// together when it is refused (C4 opens the Git directories only).
	constexpr auto format_readers = std::to_array<std::string_view>({ "C1", "C2", "C3", "C5", "C6", "C5-capture" });

	auto build_registry() -> std::vector<classification>
	{
		auto       entries = std::vector<classification>{ };
		const auto all     = [&entries](const std::string_view aCase, const std::string_view aKind, const std::string_view aReason) {
			for (const auto component : format_readers) {
				entries.push_back({ "K", component, aCase, "", aKind, aReason });
			}
		};
		// Fix 1 and Q2 (a).
		all("reftable", "refusal", "Q2 (a): a reftable checkout is refused with its own named reason (fix 1, BPR-1461); whether to read it is C3's evaluation");
		// Recorded repository-format choices (BPR-1454 ledger).
		all("format-compat-object-format", "deliberate", "extensions.compatObjectFormat, which Git accepts, is refused by name as an extension these readers do not read");
		all("format-negative-version", "deliberate", "a negative core.repositoryformatversion, which Git reads as version 0, is refused by name");
		all("format-missing-version", "deliberate",
			"a version 1 extension with no core.repositoryformatversion is refused as a version 0 extension; Git refuses only an explicit version 0 and reads this one");
		// C1: a split index.
		for (const auto component : { std::string_view{ "C1" }, std::string_view{ "C5-capture" } }) {
			entries.push_back({ "K", component, "split-index", "", "refusal",
								"the shared index is never opened: a split part that replaces shared entries (stripped names, written depending on stat timing) is refused as an index form these readers do not read, and otherwise its own entries are read and marked partial (capture coverage partial)" });
		}
		// C2, C5: includes are not followed (fix 2).
		for (const auto* name : { "config-include", "config-include-if" }) {
			for (const auto component : { std::string_view{ "C2" }, std::string_view{ "C5" }, std::string_view{ "C5-capture" } }) {
				entries.push_back({ "K", component, name, "", "deliberate", "config includes are not followed (fix 2); an include that sets core.ignoreCase is a recorded divergence" });
			}
		}
		// C3 (BPR-1454 review leftovers).
		entries.push_back({ "K", "C3", "reference-directory", "refs/heads/feature", "refusal",
							"a loose reference that is a directory is reported as unreadable rather than falling back to packed-refs, as fix 7 reports any loose failure but missing" });
		// C4.
		for (const auto* name : { "config-bad-ignorecase", "format-v0-refstorage" }) {
			entries.push_back({ "K", "C4", name, "", "deliberate",
								"discovery opens the Git directories and reads no config; Git dies on the config before printing them, and every config-reading component refuses as Git does" });
		}
		entries.push_back({ "K", "C4", "format-compat-object-format", "", "deliberate",
							"discovery reads no config; Git 2.55 built without Rust refuses the exact compatObjectFormat=sha256 configuration before printing directories (BPR-1525); config-reading components still refuse",
							"refused: rev-parse exit 128: compatibility hash algorithm support requires Rust", "answers" });
		// C5 and C6.
		for (const auto* name : { "ignorecase-true", "ignorecase-false" }) {
			for (const auto component : { std::string_view{ "C5" }, std::string_view{ "C5-capture" } }) {
				entries.push_back({ "K", component, name, "config/local/a/key.pem", "deliberate",
									"config/local**/*.pem: capture preserves Git 2.50.1's conservative exclusion; Git 2.55 retains one literal-prefix byte before wildmatch and keeps this path (BPR-1525); both core.ignoreCase settings are probed",
									"kept", component == "C5" ? "ignored" : "excluded" });
			}
		}
		entries.push_back({ "K", "C5", "ignorecase-latin1", "\xE9t\xE9.txt", "deliberate",
							"these readers fold ASCII case only; Git under a UTF-8 locale on macOS also folds this Latin-1 byte through the C library (under LC_ALL=C it does not)" });
		entries.push_back({ "K", "C6", "attributes-deleted-from-worktree", "a.bin", "deliberate",
							"capture reads the attributes of the working tree it captures; Git falls back to the index's copy of a .gitattributes missing from the worktree" });
		for (const auto* name : { "ignore-over-1mib", "ignore-over-100000-rules" }) {
			for (const auto component : { std::string_view{ "C5" }, std::string_view{ "C5-capture" } }) {
				entries.push_back({ "K", component, name, "", "refusal",
									"a rule file past capture's bound (1 MiB or 100,000 rules) refuses the capture by name instead of being dropped (BPR-1455 fix); Git reads up to 100 MB" });
			}
		}
		// [[BPR-1458]]. libgit2 1.9.7 (L), run when the evaluation-only target
		// exists. Its refusals are explicit; its silent divergences make it
		// ineligible for the components they touch.
		const auto every   = std::to_array<std::string_view>({ "C1", "C2", "C3", "C4", "C5", "C6" });
		const auto library = [&entries, &every](const std::string_view aCase, const std::string_view aKind, const std::string_view aReason) {
			for (const auto component : every) {
				entries.push_back({ "L", component, aCase, "", aKind, aReason });
			}
		};
		for (const auto* name : { "sha256-index-v2", "sha256-index-v4", "config-syntax" }) {
			library(name, "refusal", "libgit2 1.9.7's shipped build has no SHA-256 (only its experimental build reads it), so it refuses a SHA-256 repository: H3 fails");
		}
		library("reftable", "refusal", "libgit2 1.9.7 has no reftable support and refuses extensions.refStorage");
		library("format-compat-object-format", "refusal", "libgit2 refuses extensions.compatObjectFormat as an unsupported extension");
		library("format-v0-refstorage", "silent", "libgit2 reads a version 0 repository naming extensions.refStorage as a files repository; Git refuses it");
		library("config-bad-ignorecase", "silent", "libgit2 reads a config Git refuses outright (core.ignorecase = maybe) and answers every question");
		for (const auto component : { std::string_view{ "C1" }, std::string_view{ "C6" } }) {
			entries.push_back({ "L", component, "split-index", "", "refusal", "libgit2 refuses the mandatory link extension of a split index" });
			entries.push_back({ "L", component, "sparse-index-cone", "", "refusal", "libgit2 refuses the mandatory sdir extension of a sparse index" });
		}
		entries.push_back({ "L", "C3", "head-symbolic-too-deep", "", "silent", "libgit2 follows symbolic references past Git's limit of five reads and resolves a reference Git calls unresolvable" });
		for (const auto* name : { "ignorecase-true", "ignorecase-false" }) {
			entries.push_back({ "L", "C5", name, "config/local/a/key.pem", "silent",
								"libgit2 matches an anchored pattern whole, without Git's literal-prefix rule, so config/local**/*.pem keeps a path Git ignores (the defect BPR-1454 fixed in K)" });
		}
		for (const auto* name : { "attributes", "ignorecase-true", "ignorecase-false" }) {
			entries.push_back({ "L", "C6", name, "dir", "silent", "libgit2 applies a directory-only attribute pattern to a directory path that Git leaves unspecified" });
			entries.push_back({ "L", "C6", name, "quoted name.txt", "silent", "libgit2 does not read a C-quoted attribute pattern, which Git does" });
		}
		entries.push_back({ "L", "C5", "ignorecase-latin1", "\xE9t\xE9.txt", "deliberate",
							"in this C-locale process libgit2 folds ASCII only, like K; in a process that has called setlocale with a UTF-8 locale (as a Qt application does) it folds Latin-1 through the C library" });
		return entries;
	}

	auto registry() -> const std::vector<classification>&
	{
		static const auto entries = build_registry();
		return entries;
	}

	// ---------------------------------------------------------------------
	// Results.
	// ---------------------------------------------------------------------

	enum class verdict : std::uint8_t {
		agree,
		diverge,
		refused,
		skipped,
	};

	auto verdict_name(const verdict aVerdict) -> std::string_view
	{
		switch (aVerdict) {
		case verdict::agree:   return "agree";
		case verdict::diverge: return "diverge";
		case verdict::refused: return "refused";
		case verdict::skipped: return "skipped";
		}
		return "unknown";
	}

	/// One difference on one subject, or a whole-component refusal.
	struct finding
	{
		std::string Subject;
		std::string Oracle;
		std::string Candidate;
	};

	struct result
	{
		std::string          Candidate;
		std::string          Component;
		std::string          Case;
		verdict              Verdict  = verdict::agree;
		std::size_t          Compared = 0;
		std::string          Detail;
		std::vector<finding> Findings;
	};

	/// Printable, tab- and newline-free text for one report field.
	auto printable(const std::string_view aText) -> std::string
	{
		auto out = std::string{ };
		for (const auto character : aText) {
			const auto value = static_cast<unsigned char>(character);
			if (value < 0x20U || value >= 0x7FU || character == '\\') {
				constexpr auto digits = std::string_view{ "0123456789abcdef" };
				out += "\\x";
				out += digits[value >> 4U];
				out += digits[value & 0x0FU];
			} else {
				out += character;
			}
		}
		return out;
	}

	class report
	{
	public:
		/// \brief Records a result, classifying each finding. Returns false
		/// when something is unclassified.
		void add(result aResult)
		{
			auto line = std::ostringstream{ };
			line << "GMR result candidate=" << aResult.Candidate << " component=" << aResult.Component << " case=" << aResult.Case << " verdict=" << verdict_name(aResult.Verdict)
				 << " compared=" << aResult.Compared;
			if (!aResult.Detail.empty()) {
				line << " detail=\"" << printable(aResult.Detail) << '"';
			}
			std::cout << line.str() << '\n';
			const auto key = std::tuple{ aResult.Candidate, aResult.Component };
			auto&      row = Summary[key];
			++row[static_cast<std::size_t>(aResult.Verdict)];
			if (aResult.Verdict == verdict::agree || aResult.Verdict == verdict::skipped) {
				Rows.push_back({ aResult.Candidate, aResult.Component, aResult.Case, "", std::string{ verdict_name(aResult.Verdict) }, "", "", aResult.Detail });
				return;
			}
			for (const auto& item : aResult.Findings) {
				const auto* found = classify(aResult.Candidate, aResult.Component, aResult.Case, item);
				const auto  kind  = found != nullptr ? std::string{ found->Kind } : std::string{ "UNCLASSIFIED" };
				const auto  why   = found != nullptr ? std::string{ found->Reason } : std::string{ };
				if (found == nullptr) {
					++Unclassified;
				}
				std::cout << "GMR " << verdict_name(aResult.Verdict) << " candidate=" << aResult.Candidate << " component=" << aResult.Component << " case=" << aResult.Case
						  << " subject=\"" << printable(item.Subject) << "\" class=" << kind << " oracle=\"" << printable(item.Oracle) << "\" answer=\"" << printable(item.Candidate) << '"'
						  << (why.empty() ? std::string{ } : " reason=\"" + why + "\"") << '\n';
				Rows.push_back({ aResult.Candidate, aResult.Component, aResult.Case, item.Subject, std::string{ verdict_name(aResult.Verdict) }, kind, why, "oracle=" + item.Oracle + " answer=" + item.Candidate });
			}
		}

		void note_unchanged(const std::string& aCandidate, const std::string& aCase, const std::vector<std::string>& aChanges)
		{
			if (aChanges.empty()) {
				++Unchanged;
				return;
			}
			for (const auto& change : aChanges) {
				std::cout << "GMR write candidate=" << aCandidate << " case=" << aCase << " change=\"" << printable(change) << "\"\n";
				++Writes;
			}
		}

		[[nodiscard]] auto passed() const -> bool
		{
			return Unclassified == 0 && Writes == 0;
		}

		void summarize(const std::optional<std::filesystem::path>& aTsv) const
		{
			std::cout << "GMR summary: candidate component agree diverge refused skipped\n";
			for (const auto& [key, counts] : Summary) {
				std::cout << "GMR summary " << std::get<0>(key) << ' ' << std::get<1>(key) << ' ' << counts[0] << ' ' << counts[1] << ' ' << counts[2] << ' ' << counts[3] << '\n';
			}
			std::cout << "GMR summary unclassified=" << Unclassified << " unchanged-checkouts=" << Unchanged << " writes=" << Writes << '\n';
			for (const auto& entry : registry()) {
				if (!Used.contains(&entry) && entry.Candidate == "K") {
					std::cout << "GMR unused-classification candidate=" << entry.Candidate << " component=" << entry.Component << " case=" << entry.Case << " subject=\"" << printable(entry.Subject) << "\"\n";
				}
			}
			if (aTsv) {
				auto out = std::ofstream{ *aTsv, std::ios::binary | std::ios::trunc };
				out << "candidate\tcomponent\tcase\tsubject\tverdict\tclass\treason\tdetail\n";
				for (const auto& row : Rows) {
					for (std::size_t index = 0; index < row.size(); ++index) {
						out << printable(row[index]) << (index + 1 == row.size() ? '\n' : '\t');
					}
				}
			}
		}

	private:
		auto classify(const std::string_view aCandidate, const std::string_view aComponent, const std::string_view aCase, const finding& aFinding) -> const classification*
		{
			const classification* general = nullptr;
			for (const auto& entry : registry()) {
				if (entry.Candidate != aCandidate || entry.Component != aComponent || entry.Case != aCase) {
					continue;
				}
				if ((!entry.OracleAnswer.empty() && entry.OracleAnswer != aFinding.Oracle)
					|| (!entry.CandidateAnswer.empty() && entry.CandidateAnswer != aFinding.Candidate)) {
					continue;
				}
				if (entry.Subject == aFinding.Subject) {
					Used.insert(&entry);
					return &entry;
				}
				if (entry.Subject.empty() && general == nullptr) {
					general = &entry;
				}
			}
			if (general != nullptr) {
				Used.insert(general);
			}
			return general;
		}

		std::map<std::tuple<std::string, std::string>, std::array<std::size_t, 4>> Summary;
		std::vector<std::array<std::string, 8>>                                    Rows;
		std::set<const classification*>                                            Used;
		std::size_t                                                                Unclassified = 0;
		std::size_t                                                                Unchanged    = 0;
		std::size_t                                                                Writes       = 0;
	};

	// ---------------------------------------------------------------------
	// Checkout snapshots: byte-identical, unmodified, no lock files.
	// ---------------------------------------------------------------------

	struct snapshot_entry
	{
		int           Type     = 0;
		std::uint64_t Size     = 0;
		std::uint32_t Mode     = 0;
		std::int64_t  Modified = 0;
		std::string   Bytes;
		auto          operator<=>(const snapshot_entry&) const = default;
	};

	using snapshot = std::map<std::string, snapshot_entry>;

	auto take_snapshot(const std::filesystem::path& aRoot) -> snapshot
	{
		auto result = snapshot{ };
#if !defined(_WIN32)
		auto error = std::error_code{ };
		for (auto walk = std::filesystem::recursive_directory_iterator{ aRoot, std::filesystem::directory_options::none, error }; walk != std::filesystem::recursive_directory_iterator{ };
			 walk.increment(error)) {
			if (error) {
				break;
			}
			const auto& path = walk->path();
			struct stat status{ };
			if (::lstat(path.c_str(), &status) != 0) {
				continue;
			}
	#if defined(__APPLE__)
			const auto modified = status.st_mtimespec;
	#else
			const auto modified = status.st_mtim;
	#endif
			auto entry = snapshot_entry{ .Type     = static_cast<int>(status.st_mode & S_IFMT),
										 .Size     = static_cast<std::uint64_t>(status.st_size),
										 .Mode     = static_cast<std::uint32_t>(status.st_mode),
										 .Modified = (static_cast<std::int64_t>(modified.tv_sec) * 1'000'000'000) + modified.tv_nsec };
			if (S_ISREG(status.st_mode)) {
				entry.Bytes = read_file(path);
			} else if (S_ISLNK(status.st_mode)) {
				entry.Bytes = std::filesystem::read_symlink(path, error).string();
			}
			result.emplace(std::filesystem::relative(path, aRoot).generic_string(), std::move(entry));
		}
#else
		static_cast<void>(aRoot);
#endif
		return result;
	}

	auto compare_snapshots(const snapshot& aBefore, const snapshot& aAfter) -> std::vector<std::string>
	{
		auto changes = std::vector<std::string>{ };
		for (const auto& [path, entry] : aAfter) {
			if (path.ends_with(".lock")) {
				changes.push_back("lock file " + path);
			}
			const auto found = aBefore.find(path);
			if (found == aBefore.end()) {
				changes.push_back("added " + path);
			} else if (found->second != entry) {
				changes.push_back("modified " + path);
			}
		}
		for (const auto& [path, entry] : aBefore) {
			if (!aAfter.contains(path)) {
				changes.push_back("removed " + path);
			}
		}
		return changes;
	}

	// ---------------------------------------------------------------------
	// Corpus.
	// ---------------------------------------------------------------------

	using version = std::pair<int, int>;

	auto version_text(const version aVersion) -> std::string
	{
		return std::to_string(aVersion.first) + "." + std::to_string(aVersion.second);
	}

	/// What a case built, and what to ask beyond the files on disk.
	struct case_setup
	{
		std::filesystem::path Root;
		/// Root-relative paths that need not exist (a trailing `/` asks about a
		/// directory).
		std::vector<std::string> ExtraPaths;
		std::vector<std::string> ExtraReferences;
		/// Nothing on disk is asked about, only `ExtraPaths`.
		bool OnlyExtraPaths = false;
		/// Most paths asked about, for the large case.
		std::size_t PathLimit = 0;
		/// Skip the capture cross-check (a refused or huge repository).
		bool SkipCapture = false;
	};

	struct build_context
	{
		const git_runner&     Git;
		const git_oracle&     Oracle;
		std::filesystem::path Directory;
		version               Version;
		bool                  CaseInsensitive = false;
	};

	struct corpus_case
	{
		std::string_view Name;
		/// The oldest Git that can build it, with the feature named.
		version          Needs{ 0, 0 };
		std::string_view Feature;
		/// Builds the case; returns a skip reason when it cannot be built here.
		std::function<std::optional<std::string>(const build_context&, case_setup&)> Build;
		bool                                                                         Large = false;
	};

	void commit_all(const git_runner& aGit, const std::filesystem::path& aRepo, const std::string_view aMessage = "fixture")
	{
		aGit(aRepo, { "add", "-A" });
		aGit(aRepo, { "commit", "--quiet", "--no-verify", "--allow-empty", "-m", aMessage });
	}

	void init(const git_runner& aGit, const std::filesystem::path& aRepo, const std::initializer_list<std::string_view> aArguments = { "init", "--quiet" })
	{
		std::filesystem::create_directories(aRepo);
		aGit(aRepo, aArguments);
	}

	void append(const std::filesystem::path& aPath, const std::string_view aText)
	{
		write_file(aPath, read_file(aPath) + std::string{ aText });
	}

	/// A small tree with nested directories and shared prefixes.
	void plain_tree(const std::filesystem::path& aRepo)
	{
		write_file(aRepo / "README.md", "readme\n");
		write_file(aRepo / "src" / "main.cpp", "int main() {}\n");
		write_file(aRepo / "src" / "main.hpp", "#pragma once\n");
		write_file(aRepo / "src" / "detail" / "impl.cpp", "// impl\n");
		write_file(aRepo / "src" / "detail" / "impl2.cpp", "// impl2\n");
		write_file(aRepo / "docs" / "guide.md", "guide\n");
		write_file(aRepo / "a" / "b" / "c" / "deep.txt", "deep\n");
	}

	auto standard_repository(const build_context& aContext, case_setup& aSetup, const std::initializer_list<std::string_view> aInit = { "init", "--quiet" }) -> void
	{
		aSetup.Root = aContext.Directory;
		init(aContext.Git, aSetup.Root, aInit);
		plain_tree(aSetup.Root);
		commit_all(aContext.Git, aSetup.Root);
	}

	auto ignore_rules_text() -> std::string
	{
		return "# comment\n"
			   "\\#hash.txt\n"
			   "\\!bang.txt\n"
			   "*.log\n"
			   "!keep.log\n"
			   "build/\n"
			   "/root-only.txt\n"
			   "docs/*.tmp\n"
			   "**/cache\n"
			   "logs/**\n"
			   "x/**/y.txt\n"
			   "trail.txt   \n"
			   "esc\\ \n"
			   "a[[:space:]]b\n"
			   "x[![:space:]]y\n"
			   "[[:digit:]][[:upper:]]\n"
			   "[[:alpha:]][[:punct:]]z\n"
			   "q[[:bogus:]]q\n"
			   "config/local**/*.pem\n"
			   "Secret.env\n"
			   "dironly/\n"
			   "neg/*\n"
			   "!neg/kept.txt\n"
			   "*.[oa]\n"
			   "f?o.txt\n"
			   "[]]x\n"
			   "[!a-c]z.txt\n";
	}

	void ignore_tree(const std::filesystem::path& aRepo)
	{
		for (const auto* name : { "#hash.txt", "!bang.txt", "debug.log", "keep.log", "root-only.txt", "trail.txt", "esc ", "a b", "x y", "xzy", "7Q", "b,z", "qxq", "secret.env",
								  "Secret.env.not", "lib.o", "lib.a", "lib.c", "foo.txt", "f/o.txt", "]x", "dz.txt", "az.txt", "plain.txt" }) {
			write_file(aRepo / name, "x\n");
		}
		write_file(aRepo / "build" / "out.bin", "x\n");
		write_file(aRepo / "sub" / "build" / "out.bin", "x\n");
		write_file(aRepo / "sub" / "root-only.txt", "x\n");
		write_file(aRepo / "docs" / "a.tmp", "x\n");
		write_file(aRepo / "docs" / "deep" / "a.tmp", "x\n");
		write_file(aRepo / "p" / "q" / "cache" / "c.bin", "x\n");
		write_file(aRepo / "logs" / "one" / "l.txt", "x\n");
		write_file(aRepo / "x" / "y.txt", "x\n");
		write_file(aRepo / "x" / "m" / "n" / "y.txt", "x\n");
		write_file(aRepo / "config" / "local" / "a" / "key.pem", "x\n");
		write_file(aRepo / "config" / "localish" / "key.pem", "x\n");
		write_file(aRepo / "dironly" / "f.txt", "x\n");
		write_file(aRepo / "nested" / "dironly", "a file, not a directory\n");
		write_file(aRepo / "neg" / "kept.txt", "x\n");
		write_file(aRepo / "neg" / "gone.txt", "x\n");
		// A nested rule file: it re-includes logs and adds its own rules.
		write_file(aRepo / "sub" / ".gitignore", "!*.log\nlocal/\n/anchored.txt\n");
		write_file(aRepo / "sub" / "debug.log", "x\n");
		write_file(aRepo / "sub" / "local" / "f.txt", "x\n");
		write_file(aRepo / "sub" / "anchored.txt", "x\n");
		write_file(aRepo / "sub" / "deeper" / "anchored.txt", "x\n");
		// A rule under an ignored directory is never read.
		write_file(aRepo / "build" / ".gitignore", "!out.bin\n");
	}

	auto attribute_rules_text() -> std::string
	{
		return "[attr]lfsmacro filter=lfs diff=lfs merge=lfs -text\n"
			   "[attr]nofilter -filter\n"
			   "[attr]chain lfsmacro\n"
			   "*.bin filter=lfs\n"
			   "*.dat filter\n"
			   "*.raw -filter\n"
			   "special.raw !filter\n"
			   "*.mac lfsmacro\n"
			   "*.nof nofilter\n"
			   "*.chn chain\n"
			   "docs/** filter=docs\n"
			   "/anchored.txt filter=anchored\n"
			   "\"quoted name.txt\" filter=quoted\n"
			   "dir/ filter=dironly\n"
			   "*.BIG filter=upper\n"
			   "invalid name! filter=bad\n"
			   "ws.txt\tfilter=tab\n"
			   "multi.txt filter=a filter=b\n";
	}

	void attribute_tree(const std::filesystem::path& aRepo)
	{
		for (const auto* name : { "a.bin", "a.dat", "a.raw", "special.raw", "a.mac", "a.nof", "a.chn", "anchored.txt", "quoted name.txt", "a.big", "ws.txt", "multi.txt", "none.txt" }) {
			write_file(aRepo / name, "x\n");
		}
		write_file(aRepo / "docs" / "d.txt", "x\n");
		write_file(aRepo / "deep" / "anchored.txt", "x\n");
		write_file(aRepo / "dir" / "f.txt", "x\n");
		write_file(aRepo / "sub" / ".gitattributes", "*.bin -filter\n[attr]local filter=local\n*.loc local\n*.sub filter=sub\n");
		write_file(aRepo / "sub" / "a.bin", "x\n");
		write_file(aRepo / "sub" / "a.loc", "x\n");
		write_file(aRepo / "sub" / "a.sub", "x\n");
		write_file(aRepo / "sub" / "a.mac", "x\n");
	}

	auto corpus() -> std::vector<corpus_case>
	{
		auto       cases        = std::vector<corpus_case>{ };
		const auto indexVersion = [](const std::string_view aVersion) {
			return [aVersion](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
				standard_repository(aContext, aSetup);
				aContext.Git(aSetup.Root, { "update-index", "--index-version", aVersion });
				return std::nullopt;
			};
		};
		cases.push_back({ .Name = "sha1-index-v2", .Build = indexVersion("2") });
		cases.push_back({ .Name  = "sha1-index-v3",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "update-index", "--index-version", "3" });
							  aContext.Git(aSetup.Root, { "update-index", "--skip-worktree", "docs/guide.md" });
							  return std::nullopt;
						  } });
		cases.push_back({ .Name = "sha1-index-v4", .Build = indexVersion("4") });
		cases.push_back({ .Name  = "sha256-index-v2",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup, { "init", "--quiet", "--object-format=sha256" });
							  aContext.Git(aSetup.Root, { "update-index", "--index-version", "2" });
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "sha256-index-v4",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup, { "init", "--quiet", "--object-format=sha256" });
							  aContext.Git(aSetup.Root, { "update-index", "--index-version", "4", "--skip-worktree", "README.md" });
							  return std::nullopt;
						  } });
		cases.push_back({ .Name    = "many-files",
						  .Needs   = { 2, 40 },
						  .Feature = "feature.manyFiles with index.skipHash",
						  .Build   = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  aSetup.Root = aContext.Directory;
							  init(aContext.Git, aSetup.Root);
							  aContext.Git(aSetup.Root, { "config", "feature.manyFiles", "true" });
							  plain_tree(aSetup.Root);
							  commit_all(aContext.Git, aSetup.Root);
							  return std::nullopt;
						  } });
		cases.push_back({ .Name    = "skip-hash",
						  .Needs   = { 2, 40 },
						  .Feature = "index.skipHash",
						  .Build   = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  aSetup.Root = aContext.Directory;
							  init(aContext.Git, aSetup.Root);
							  aContext.Git(aSetup.Root, { "config", "index.skipHash", "true" });
							  plain_tree(aSetup.Root);
							  commit_all(aContext.Git, aSetup.Root);
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "split-index",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "update-index", "--split-index" });
							  write_file(aSetup.Root / "added-after-split.txt", "x\n");
							  aContext.Git(aSetup.Root, { "add", "added-after-split.txt" });
							  return std::nullopt;
						  } });
		cases.push_back({ .Name    = "sparse-index-cone",
						  .Needs   = { 2, 36 },
						  .Feature = "a cone sparse checkout with a sparse index",
						  .Build   = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "sparse-checkout", "init", "--cone", "--sparse-index" });
							  aContext.Git(aSetup.Root, { "sparse-checkout", "set", "src" });
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "index-entries",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  const auto& git  = aContext.Git;
							  const auto& root = aSetup.Root;
							  write_file(root / "conflict.txt", "base\n");
							  commit_all(git, root, "base");
							  git(root, { "checkout", "--quiet", "-b", "side" });
							  write_file(root / "conflict.txt", "side\n");
							  commit_all(git, root, "side");
							  git(root, { "checkout", "--quiet", "main" });
							  write_file(root / "conflict.txt", "main\n");
							  commit_all(git, root, "main");
							  const auto merged = git.status(root, { "merge", "--no-edit", "side" }, std::nullopt, aContext.Directory.parent_path() / "merge.out");
							  expect(merged == 1, "the merge conflicts");
							  write_file(root / "intent.txt", "intent\n");
							  git(root, { "add", "-N", "intent.txt" });
							  git(root, { "update-index", "--skip-worktree", "src/main.cpp" });
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "linked-worktree",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  const auto main = aContext.Directory / "main";
							  init(aContext.Git, main);
							  plain_tree(main);
							  write_file(main / ".gitignore", "*.log\n");
							  commit_all(aContext.Git, main);
							  aContext.Git(main, { "worktree", "add", "--quiet", "-b", "linked", (aContext.Directory / "linked").string() });
							  aSetup.Root = aContext.Directory / "linked";
							  write_file(aSetup.Root / "debug.log", "x\n");
							  write_file(aSetup.Root / "kept.txt", "x\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name    = "relative-worktree",
						  .Needs   = { 2, 48 },
						  .Feature = "worktree add --relative-paths (extensions.relativeWorktrees)",
						  .Build   = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  const auto main = aContext.Directory / "main";
							  init(aContext.Git, main);
							  plain_tree(main);
							  commit_all(aContext.Git, main);
							  aContext.Git(main, { "worktree", "add", "--quiet", "--relative-paths", "-b", "relative", "../relative" });
							  aSetup.Root = aContext.Directory / "relative";
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "head-packed-only",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "branch", "other" });
							  aContext.Git(aSetup.Root, { "tag", "-a", "-m", "annotated", "v1" });
							  aContext.Git(aSetup.Root, { "tag", "light" });
							  aContext.Git(aSetup.Root, { "pack-refs", "--all" });
							  expect(!std::filesystem::exists(aSetup.Root / ".git" / "refs" / "heads" / "main"), "main is packed only");
							  aSetup.ExtraReferences = { "refs/tags/v1", "refs/tags/light", "refs/heads/missing" };
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "head-loose",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "pack-refs", "--all" });
							  write_file(aSetup.Root / "README.md", "second\n");
							  commit_all(aContext.Git, aSetup.Root, "second");
							  // Loose main now differs from packed main.
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "head-symbolic",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "symbolic-ref", "refs/heads/alias", "refs/heads/main" });
							  aContext.Git(aSetup.Root, { "symbolic-ref", "HEAD", "refs/heads/alias" });
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "head-symbolic-two-levels",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "symbolic-ref", "refs/heads/alias1", "refs/heads/main" });
							  aContext.Git(aSetup.Root, { "symbolic-ref", "refs/heads/alias2", "refs/heads/alias1" });
							  aContext.Git(aSetup.Root, { "symbolic-ref", "HEAD", "refs/heads/alias2" });
							  return std::nullopt;
						  } });
		// Git reads a name and its symbolic targets at most five times.
		cases.push_back({ .Name  = "head-symbolic-too-deep",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "symbolic-ref", "refs/heads/deep1", "refs/heads/main" });
							  aContext.Git(aSetup.Root, { "symbolic-ref", "refs/heads/deep2", "refs/heads/deep1" });
							  aContext.Git(aSetup.Root, { "symbolic-ref", "refs/heads/deep3", "refs/heads/deep2" });
							  aContext.Git(aSetup.Root, { "symbolic-ref", "refs/heads/deep4", "refs/heads/deep3" });
							  aContext.Git(aSetup.Root, { "symbolic-ref", "refs/heads/deep5", "refs/heads/deep4" });
							  aContext.Git(aSetup.Root, { "symbolic-ref", "HEAD", "refs/heads/deep4" });
							  aSetup.ExtraReferences = { "refs/heads/deep4", "refs/heads/deep5" };
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "head-detached",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "checkout", "--quiet", "--detach" });
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "head-unborn",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  aSetup.Root = aContext.Directory;
							  init(aContext.Git, aSetup.Root);
							  plain_tree(aSetup.Root);
							  aContext.Git(aSetup.Root, { "add", "src" });
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "reference-directory",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "branch", "feature" });
							  aContext.Git(aSetup.Root, { "pack-refs", "--all" });
							  // A directory where the loose reference would be; Git reads packed-refs.
							  std::filesystem::create_directories(aSetup.Root / ".git" / "refs" / "heads" / "feature");
							  aSetup.ExtraReferences = { "refs/heads/feature" };
							  return std::nullopt;
						  } });
		cases.push_back({ .Name    = "reftable",
						  .Needs   = { 2, 45 },
						  .Feature = "the reftable reference backend",
						  .Build   = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup, { "init", "--quiet", "--ref-format=reftable" });
							  return std::nullopt;
						  } });
		// Config syntax.
		cases.push_back({ .Name  = "config-syntax",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup, { "init", "--quiet", "--object-format=sha256" });
							  auto       config = read_file(aSetup.Root / ".git" / "config");
							  const auto at     = config.find("objectformat = sha256");
							  config.insert(at + std::string_view{ "objectformat = sha256" }.size(), " ; inline comment");
							  config += "[CORE]\n\tIgnoreCase = \"tr\"\\\n\"ue\" # quoted, continued and commented\n";
							  config += "[core \"sub\"]\n\tignorecase = false\n";
							  config += "[remote \"origin\\\"quoted\"]\n\turl = \"a\\tb\\\\c\"\n";
							  config += "[core.subsection]\n\tignorecase = false\n";
							  write_file(aSetup.Root / ".git" / "config", config);
							  write_file(aSetup.Root / ".gitignore", "SECRET.env\n");
							  write_file(aSetup.Root / "secret.env", "x\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "config-include",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  write_file(aSetup.Root / ".git" / "included.config", "[core]\n\tignorecase = true\n");
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  append(aSetup.Root / ".git" / "config", "[include]\n\tpath = included.config\n");
							  write_file(aSetup.Root / ".gitignore", "Secret.env\n");
							  write_file(aSetup.Root / "secret.env", "x\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "config-include-if",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  write_file(aSetup.Root / ".git" / "conditional.config", "[core]\n\tignorecase = true\n");
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  append(aSetup.Root / ".git" / "config", "[includeIf \"gitdir:" + aSetup.Root.generic_string() + "/\"]\n\tpath = conditional.config\n");
							  write_file(aSetup.Root / ".gitignore", "Secret.env\n");
							  write_file(aSetup.Root / "secret.env", "x\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "config-include-object-format",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  // Git reads repository-format keys without includes.
							  standard_repository(aContext, aSetup);
							  write_file(aSetup.Root / ".git" / "format.config", "[extensions]\n\tobjectformat = sha256\n");
							  append(aSetup.Root / ".git" / "config", "[include]\n\tpath = format.config\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "config-worktree",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.repositoryformatversion", "1" });
							  aContext.Git(aSetup.Root, { "config", "extensions.worktreeConfig", "true" });
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  aContext.Git(aSetup.Root, { "config", "--worktree", "core.ignorecase", "true" });
							  write_file(aSetup.Root / ".gitignore", "Secret.env\n");
							  write_file(aSetup.Root / "secret.env", "x\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "explicit-directory-query",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  write_file(aSetup.Root / ".gitignore", "absent-build/\n");
							  expect(!std::filesystem::exists(aSetup.Root / "absent-build"), "the explicit directory starts absent before fixture preparation");
							  aSetup.ExtraPaths = { "absent-build/" };
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "ignorecase-true",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "true" });
							  write_file(aSetup.Root / ".gitignore", ignore_rules_text());
							  write_file(aSetup.Root / ".gitattributes", attribute_rules_text());
							  ignore_tree(aSetup.Root);
							  attribute_tree(aSetup.Root);
							  aSetup.ExtraPaths = { "SRC/MAIN.CPP", "BUILD/", "Sub/Local/f.txt", "X Y", "DEBUG.LOG", "A.BIN", "DOCS/x.txt" };
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "ignorecase-false",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  write_file(aSetup.Root / ".gitignore", ignore_rules_text());
							  write_file(aSetup.Root / ".gitattributes", attribute_rules_text());
							  ignore_tree(aSetup.Root);
							  attribute_tree(aSetup.Root);
							  aSetup.ExtraPaths = { "SRC/MAIN.CPP", "BUILD/", "Sub/Local/f.txt", "DEBUG.LOG", "A.BIN" };
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "case-distinct-files",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  if (aContext.CaseInsensitive) {
								  return std::string{ "this file system folds case: Name.txt and name.txt cannot both exist" };
							  }
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  write_file(aSetup.Root / ".gitignore", "Name.txt\n");
							  write_file(aSetup.Root / "Name.txt", "x\n");
							  write_file(aSetup.Root / "name.txt", "x\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "ignore-info-exclude",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  write_file(aSetup.Root / ".git" / "info" / "exclude", "*.secret\n!keep.secret\nprivate/\n");
							  write_file(aSetup.Root / ".gitignore", "!*.secret\n");
							  write_file(aSetup.Root / "a.secret", "x\n");
							  write_file(aSetup.Root / "keep.secret", "x\n");
							  write_file(aSetup.Root / "private" / "p.txt", "x\n");
							  write_file(aSetup.Root / "sub" / "b.secret", "x\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "ignore-crlf",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  write_file(aSetup.Root / ".gitignore", "\xEF\xBB\xBF*.log\r\nbuild/\r\n!keep.log\r\nspace.txt \r\n");
							  write_file(aSetup.Root / ".gitattributes", "*.bin filter=lfs\r\n*.dat filter=crlf \r\n");
							  for (const auto* name : { "a.log", "keep.log", "space.txt", "a.bin", "a.dat" }) {
								  write_file(aSetup.Root / name, "x\n");
							  }
							  write_file(aSetup.Root / "build" / "x.o", "x\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "ignore-non-utf8",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  write_file(aSetup.Root / ".gitignore", "caf\xE9.txt\n\xFF*\n[\xE0-\xEF]x\nd\xC3\xA9j\xC3\xA0/\n");
							  write_file(aSetup.Root / ".gitattributes", "caf\xE9.bin filter=latin\n\xFE* filter=high\n");
							  // APFS refuses names that are not UTF-8, so these are asked of
							  // Git and the candidates without files.
							  aSetup.ExtraPaths = { "caf\xE9.txt", "caf\xC3\xA9.txt", "\xFF\xFE.bin", "\xE5x", "\xF5x", "d\xC3\xA9j\xC3\xA0/f.txt", "caf\xE9.bin", "\xFE.dat", "plain.txt" };
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "ignorecase-latin1",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "true" });
							  write_file(aSetup.Root / ".gitignore", "\xC9t\xC9.txt\nAbc\xD0*\n");
							  write_file(aSetup.Root / ".gitattributes", "\xC9*.bin filter=latin\n");
							  aSetup.ExtraPaths     = { "\xE9t\xE9.txt", "\xC9t\xC9.txt", "abc\xF0x", "abc\xD0x", "\xE9x.bin", "\xC9x.bin" };
							  aSetup.OnlyExtraPaths = true;
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "attributes",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  write_file(aSetup.Root / ".gitattributes", attribute_rules_text());
							  write_file(aSetup.Root / ".git" / "info" / "attributes", "*.inf filter=info\n*.raw filter=info-wins\n[attr]infomacro filter=infomacro\n*.im infomacro\n");
							  attribute_tree(aSetup.Root);
							  for (const auto* name : { "a.inf", "a.im" }) {
								  write_file(aSetup.Root / name, "x\n");
							  }
							  return std::nullopt;
						  } });
		// [[BPR-1692]]. RFC-BPR-0089's linguist-generated corpus, the same as
		// tests/core/generated_files_tests.cpp's: nested files, macros, `**`,
		// anchored, directory and quoted patterns, and the three ways to say no.
		cases.push_back({ .Name  = "linguist-generated",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  write_file(aSetup.Root / ".gitattributes",
										 "[attr]generated linguist-generated\n"
										 "gen/** linguist-generated\n"
										 "*.min.js linguist-generated=true\n"
										 "/anchored.gen linguist-generated\n"
										 "build/ linguist-generated\n"
										 "vendor/*.js generated\n"
										 "docs/** -linguist-generated\n"
										 "keep.json linguist-generated=false\n"
										 "\"quoted name.txt\" linguist-generated\n");
							  write_file(aSetup.Root / "gen" / ".gitattributes",
										 "item_007.txt -linguist-generated\nspecial.txt !linguist-generated\n[attr]local linguist-generated\n*.loc local\n");
							  write_file(aSetup.Root / "nested" / ".gitattributes", "f.txt linguist-generated\n");
							  for (const auto* name : { "gen/item_000.txt", "gen/sub/deep.txt", "gen/item_007.txt", "gen/special.txt", "gen/x.loc", "a/b.min.js",
														"anchored.gen", "sub/anchored.gen", "build/out.txt", "vendor/lib.js", "vendor/deep/lib.js", "docs/a/b.md",
														"keep.json", "quoted name.txt", "nested/f.txt", "f.txt", "src/app.cpp" }) {
								  write_file(aSetup.Root / name, "x\n");
							  }
							  aSetup.ExtraPaths = { "GEN/item_001.txt", "build/" };
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "attributes-deleted-from-worktree",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  write_file(aSetup.Root / ".gitattributes", "*.bin filter=lfs\n");
							  write_file(aSetup.Root / "a.bin", "x\n");
							  commit_all(aContext.Git, aSetup.Root, "attributes");
							  // Git falls back to the index's copy for a file not in the worktree.
							  std::filesystem::remove(aSetup.Root / ".gitattributes");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "adversarial-bpr0970",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  auto stars = std::string{ };
							  for (int index = 0; index < 3000; ++index) {
								  stars += "*a";
							  }
							  write_file(aSetup.Root / ".gitignore", "*a*a*a*a*a*a*a*a*a*a*a*a*b\n**/*a*a*a*a*a*a*a*a*a*a*a*a*b\n" + stars + "b\n[[:alpha:]\n[z-a]x\n\\\n");
							  write_file(aSetup.Root / std::string(250U, 'a'), "x\n");
							  write_file(aSetup.Root / (std::string(200U, 'a') + "b"), "x\n");
							  aSetup.ExtraPaths = { std::string(400U, 'a'), std::string(200U, 'a') + "/" + std::string(200U, 'a'), std::string(6000U, 'a'), std::string(6000U, 'a') + "b" };
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "format-compat-object-format",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.repositoryformatversion", "1" });
							  aContext.Git(aSetup.Root, { "config", "extensions.compatObjectFormat", "sha256" });
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "format-negative-version",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.repositoryformatversion", "-1" });
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "format-v0-refstorage",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  // refStorage is a version 1 extension: Git refuses it here.
							  standard_repository(aContext, aSetup);
							  append(aSetup.Root / ".git" / "config", "[extensions]\n\trefStorage = files\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "format-missing-version",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  write_file(aSetup.Root / ".git" / "config", "[core]\n\tbare = false\n[extensions]\n\tobjectformat = sha1\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "config-form-feed",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  // A key these readers never use, whose value starts with a form
							  // feed: not white space to Git, which reads the config.
							  standard_repository(aContext, aSetup);
							  append(aSetup.Root / ".git" / "config", "[remote \"x\"]\n\turl = \fabc\n\tpushurl = \vabc\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "config-nul",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  append(aSetup.Root / ".git" / "config", std::string{ "[remote \"x\"]\n\turl = a" } + '\0' + "b\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "config-bad-ignorecase",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  // Git dies on a key it parses as a boolean; so do the readers.
							  standard_repository(aContext, aSetup);
							  append(aSetup.Root / ".git" / "config", "[core]\n\tignorecase =\vtrue\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "ignore-over-1mib",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  // A comment pads the file past capture's 1 MiB bound; Git reads it.
							  write_file(aSetup.Root / ".gitignore", "# " + std::string(1100U * 1024U, 'p') + "\nsecret.env\n");
							  write_file(aSetup.Root / "secret.env", "x\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name  = "ignore-over-100000-rules",
						  .Build = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  standard_repository(aContext, aSetup);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  auto rules = std::string{ };
							  for (int index = 0; index < 100'001; ++index) {
								  rules += "r" + std::to_string(index) + "\n";
							  }
							  write_file(aSetup.Root / ".gitignore", rules + "secret.env\n");
							  write_file(aSetup.Root / "secret.env", "x\n");
							  return std::nullopt;
						  } });
		cases.push_back({ .Name    = "large-250000",
						  .Feature = "250,000 index entries",
						  .Build   = [](const build_context& aContext, case_setup& aSetup) -> std::optional<std::string> {
							  aSetup.Root = aContext.Directory;
							  init(aContext.Git, aSetup.Root);
							  aContext.Git(aSetup.Root, { "config", "core.ignorecase", "false" });
							  for (int directory = 0; directory < 500; ++directory) {
								  for (int file = 0; file < 500; ++file) {
									  write_file(aSetup.Root / ("d" + std::to_string(directory)) / ("f" + std::to_string(file)), "x\n");
								  }
							  }
							  write_file(aSetup.Root / ".gitignore", "d1/f1*\n");
							  commit_all(aContext.Git, aSetup.Root);
							  aSetup.PathLimit   = 2000;
							  aSetup.SkipCapture = true;
							  return std::nullopt;
						  },
						  .Large = true });
		return cases;
	}

	// ---------------------------------------------------------------------
	// Questions and comparisons.
	// ---------------------------------------------------------------------

	/// The paths asked about, and which of them were found on disk (the rest
	/// need not exist, and on a file system that folds case may name another).
	auto questions_for(const case_setup& aSetup, std::set<std::string>& aOnDisk) -> std::vector<path_question>
	{
		auto questions = std::vector<path_question>{ };
		if (!aSetup.OnlyExtraPaths) {
			auto error = std::error_code{ };
			for (auto walk = std::filesystem::recursive_directory_iterator{ aSetup.Root, std::filesystem::directory_options::none, error };
				 walk != std::filesystem::recursive_directory_iterator{ }; walk.increment(error)) {
				if (error) {
					break;
				}
				const auto relative = std::filesystem::relative(walk->path(), aSetup.Root).generic_string();
				if (relative == ".git" || relative.starts_with(".git/")) {
					if (walk->is_directory(error) && !walk->is_symlink(error)) {
						walk.disable_recursion_pending();
					}
					continue;
				}
				const auto directory = walk->is_directory(error) && !walk->is_symlink(error);
				questions.push_back({ .Path = relative, .Directory = directory });
				aOnDisk.insert(relative);
				if (aSetup.PathLimit != 0 && questions.size() >= aSetup.PathLimit) {
					break;
				}
			}
		}
		for (const auto& extra : aSetup.ExtraPaths) {
			const auto directory = extra.ends_with('/');
			questions.push_back({ .Path = directory ? extra.substr(0, extra.size() - 1) : extra, .Directory = directory });
		}
		std::ranges::sort(questions, { }, &path_question::Path);
		return questions;
	}

	auto entry_text(const index_answer_entry& aEntry) -> std::string
	{
		auto text = std::ostringstream{ };
		text << std::oct << aEntry.Mode << std::dec << ' ' << aEntry.Object << ' ' << static_cast<int>(aEntry.Stage) << (aEntry.SkipWorktree ? " skip-worktree" : "")
			 << (aEntry.IntentToAdd ? " intent-to-add" : "");
		return text.str();
	}

	auto optional_text(const std::optional<std::string>& aText) -> std::string
	{
		return aText ? *aText : std::string{ "(none)" };
	}

	auto identity_text(const std::optional<tests::workspace_contract::file_identity>& aIdentity) -> std::string
	{
		return aIdentity ? std::to_string(aIdentity->Device) + ":" + std::to_string(aIdentity->Object) : std::string{ "(none)" };
	}

	/// Folds a candidate's and Git's answers into one result.
	template <typename TAnswer, typename TCompare>
	auto compare(const std::string& aCandidate, const std::string& aComponent, const std::string& aCase, const std::expected<TAnswer, candidate_refusal>& aOracle,
				 const std::expected<TAnswer, candidate_refusal>& aAnswer, TCompare aCompare) -> result
	{
		auto outcome = result{ .Candidate = aCandidate, .Component = aComponent, .Case = aCase };
		if (!aOracle && !aAnswer) {
			outcome.Detail = "both refuse: git " + aOracle.error() + "; candidate " + aAnswer.error();
			return outcome;
		}
		if (!aAnswer) {
			outcome.Verdict = verdict::refused;
			outcome.Detail  = aAnswer.error();
			outcome.Findings.push_back({ .Subject = "", .Oracle = "answers", .Candidate = "refused: " + aAnswer.error() });
			return outcome;
		}
		if (!aOracle) {
			outcome.Verdict = verdict::diverge;
			outcome.Findings.push_back({ .Subject = "", .Oracle = "refused: " + aOracle.error(), .Candidate = "answers" });
			return outcome;
		}
		aCompare(*aOracle, *aAnswer, outcome);
		if (!outcome.Findings.empty()) {
			outcome.Verdict = verdict::diverge;
		}
		return outcome;
	}

	void compare_index(const index_answer& aOracle, const index_answer& aAnswer, result& aResult)
	{
		auto oracle = std::map<std::pair<std::string, int>, const index_answer_entry*>{ };
		auto answer = std::map<std::pair<std::string, int>, const index_answer_entry*>{ };
		for (const auto& entry : aOracle.Entries) {
			oracle[{ entry.Path, entry.Stage }] = &entry;
		}
		for (const auto& entry : aAnswer.Entries) {
			answer[{ entry.Path, entry.Stage }] = &entry;
		}
		auto keys = std::set<std::pair<std::string, int>>{ };
		for (const auto& [key, entry] : oracle) {
			keys.insert(key);
		}
		for (const auto& [key, entry] : answer) {
			keys.insert(key);
		}
		aResult.Compared = keys.size();
		for (const auto& key : keys) {
			const auto left  = oracle.find(key);
			const auto right = answer.find(key);
			const auto lhs   = left == oracle.end() ? std::string{ "(absent)" } : entry_text(*left->second);
			const auto rhs   = right == answer.end() ? std::string{ "(absent)" } : entry_text(*right->second);
			if (lhs != rhs) {
				aResult.Findings.push_back({ .Subject = key.first, .Oracle = lhs, .Candidate = rhs + (aAnswer.Partial ? " (answer marked partial)" : "") });
			}
		}
	}

	void compare_format(const format_answer& aOracle, const format_answer& aAnswer, result& aResult)
	{
		aResult.Compared = 3;
		if (aOracle.ObjectFormat != aAnswer.ObjectFormat) {
			aResult.Findings.push_back({ .Subject = "object-format", .Oracle = aOracle.ObjectFormat, .Candidate = aAnswer.ObjectFormat });
		}
		if (aOracle.ReferenceFormat != aAnswer.ReferenceFormat) {
			aResult.Findings.push_back({ .Subject = "ref-format", .Oracle = aOracle.ReferenceFormat, .Candidate = aAnswer.ReferenceFormat });
		}
		if (aOracle.IgnoreCase != aAnswer.IgnoreCase) {
			aResult.Findings.push_back({ .Subject = "ignorecase", .Oracle = aOracle.IgnoreCase ? "true" : "false", .Candidate = aAnswer.IgnoreCase ? "true" : "false" });
		}
	}

	void compare_head(const head_answer& aOracle, const head_answer& aAnswer, result& aResult)
	{
		aResult.Compared += 2;
		if (aOracle.Symbolic != aAnswer.Symbolic) {
			aResult.Findings.push_back({ .Subject = "HEAD symbolic", .Oracle = optional_text(aOracle.Symbolic), .Candidate = optional_text(aAnswer.Symbolic) });
		}
		if (aOracle.Commit != aAnswer.Commit) {
			aResult.Findings.push_back({ .Subject = "HEAD commit", .Oracle = optional_text(aOracle.Commit), .Candidate = optional_text(aAnswer.Commit) });
		}
	}

	void compare_discovery(const discovery_answer& aOracle, const discovery_answer& aAnswer, result& aResult)
	{
		aResult.Compared           = 3;
		const auto gitDirectory    = aAnswer.GitDirectoryIdentity ? aAnswer.GitDirectoryIdentity : tests::workspace_contract::identity_of(aAnswer.GitDirectory);
		const auto commonDirectory = aAnswer.CommonDirectoryIdentity ? aAnswer.CommonDirectoryIdentity : tests::workspace_contract::identity_of(aAnswer.CommonDirectory);
		if (!aOracle.GitDirectoryIdentity || gitDirectory != aOracle.GitDirectoryIdentity) {
			aResult.Findings.push_back({ .Subject = "git-dir", .Oracle = aOracle.GitDirectory.string() + " " + identity_text(aOracle.GitDirectoryIdentity), .Candidate = identity_text(gitDirectory) });
		}
		if (!aOracle.CommonDirectoryIdentity || commonDirectory != aOracle.CommonDirectoryIdentity) {
			aResult.Findings.push_back(
				{ .Subject = "common-dir", .Oracle = aOracle.CommonDirectory.string() + " " + identity_text(aOracle.CommonDirectoryIdentity), .Candidate = identity_text(commonDirectory) });
		}
		if (aOracle.LinkedWorktree != aAnswer.LinkedWorktree) {
			aResult.Findings.push_back({ .Subject = "linked", .Oracle = aOracle.LinkedWorktree ? "true" : "false", .Candidate = aAnswer.LinkedWorktree ? "true" : "false" });
		}
	}

	template <typename TValue>
	auto per_path(const std::vector<path_question>& aQuestions)
	{
		return [&aQuestions](const std::vector<TValue>& aOracle, const std::vector<TValue>& aAnswer, result& aResult) {
			aResult.Compared = aQuestions.size();
			if (aOracle.size() != aAnswer.size()) {
				aResult.Findings.push_back({ .Subject = "", .Oracle = std::to_string(aOracle.size()) + " answers", .Candidate = std::to_string(aAnswer.size()) + " answers" });
				return;
			}
			for (std::size_t index = 0; index < aQuestions.size(); ++index) {
				if (aOracle[index] != aAnswer[index]) {
					if constexpr (std::is_same_v<TValue, bool>) {
						aResult.Findings.push_back({ .Subject = aQuestions[index].Path, .Oracle = aOracle[index] ? "ignored" : "kept", .Candidate = aAnswer[index] ? "ignored" : "kept" });
					} else {
						aResult.Findings.push_back({ .Subject = aQuestions[index].Path, .Oracle = aOracle[index], .Candidate = aAnswer[index] });
					}
				}
			}
		};
	}

	/// The public path: a real capture's untracked-file exclusions against
	/// Git's ignore answers.
	auto capture_cross_check(const std::string& aCase, const case_setup& aSetup, const std::vector<path_question>& aQuestions, const std::set<std::string>& aOnDisk,
							 const std::expected<std::vector<bool>, candidate_refusal>& aIgnored,
							 const std::expected<index_answer, candidate_refusal>&      aIndex) -> result
	{
		auto outcome = result{ .Candidate = "K", .Component = "C5-capture", .Case = aCase };
		if (!aIgnored || !aIndex) {
			outcome.Verdict = verdict::skipped;
			outcome.Detail  = "git refuses the repository";
			return outcome;
		}
		auto       source = adapters::workspace::make_local_workspace_source();
		auto       cycle  = tests::workspace_contract::course{ *source, aSetup.Root };
		const auto taken  = cycle.capture(*source);
		if (taken.Status != application::cycle_capture_status::captured) {
			outcome.Verdict = verdict::refused;
			outcome.Detail  = application::cycle_capture_words(taken);
			outcome.Findings.push_back({ .Subject = "", .Oracle = "captures", .Candidate = "refused: " + outcome.Detail });
			return outcome;
		}
		const auto manifest = cycle.manifest(taken);
		auto       tracked  = std::set<std::string>{ };
		for (const auto& entry : aIndex->Entries) {
			auto path = entry.Path;
			if (path.ends_with('/')) {
				path.pop_back();
			}
			tracked.insert(path);
		}
		const auto tracksBeneath = [&tracked](const std::string& aPath) {
			const auto prefix = aPath + "/";
			const auto found  = tracked.lower_bound(prefix);
			return found != tracked.end() && found->starts_with(prefix);
		};
		const auto excludedAt = [&manifest](const std::string& aPath) {
			return std::ranges::any_of(manifest.Exclusions, [&aPath](const core::manifest_exclusion& aExclusion) {
				return aExclusion.Reason == core::workspace_exclusion_reason::ignored && (aExclusion.Path == aPath || aPath.starts_with(aExclusion.Path + "/"));
			});
		};
		const auto defaultExcluded = [](const std::string& aPath) {
			auto start = std::size_t{ 0 };
			for (;;) {
				const auto slash = aPath.find('/', start);
				if (core::excluded_by_default(std::string_view{ aPath }.substr(start, slash == std::string::npos ? std::string::npos : slash - start))) {
					return true;
				}
				if (slash == std::string::npos) {
					return false;
				}
				start = slash + 1;
			}
		};
		for (std::size_t index = 0; index < aQuestions.size(); ++index) {
			const auto& question = aQuestions[index];
			if (!aOnDisk.contains(question.Path) || tracked.contains(question.Path) || defaultExcluded(question.Path)) {
				continue;
			}
			if (question.Directory && tracksBeneath(question.Path)) {
				continue;
			}
			// A tracked ancestor's untracked child is still untracked; only the
			// path's own index presence matters here.
			++outcome.Compared;
			const auto gitIgnores = (*aIgnored)[index];
			const auto excluded   = excludedAt(question.Path);
			const auto captured   = !question.Directory && core::find_entry(manifest, question.Path) != nullptr;
			if (gitIgnores != excluded || (gitIgnores && captured)) {
				outcome.Findings.push_back({ .Subject = question.Path, .Oracle = gitIgnores ? "ignored" : "kept", .Candidate = std::string{ excluded ? "excluded" : "not excluded" } + (captured ? ", captured" : "") });
			}
		}
		if (!outcome.Findings.empty()) {
			outcome.Verdict = verdict::diverge;
		}
		return outcome;
	}

	struct harness_options
	{
		bool                                 Large = false;
		std::optional<std::string>           Only;
		std::optional<std::filesystem::path> Tsv;
		bool                                 SkipCanary = false;
	};

	/// \brief Questions a candidate is not asked because answering them is
	/// known to exceed H6 by orders of magnitude, so the run can finish. Each
	/// names the separate measurement that shows it; the decision record
	/// counts it as that candidate's H6 failure.
	auto exceeds_h6(const std::string_view aCandidate, const std::string_view aComponent, const std::string_view aCase) -> std::optional<std::string_view>
	{
		if (aCandidate == "L" && aCase == "adversarial-bpr0970" && (aComponent == "C5" || aComponent == "C6")) {
			return "not run: libgit2 1.9.7's wildmatch is exponential in the number of `*` (wildmatch.c returns WM_NOMATCH where Git aborts); `*a` x14 took 4.8 s and x18 did not finish in "
				   "tool_libgit2_probe, against well under 1 ms for K";
		}
		return std::nullopt;
	}

	auto case_insensitive(const std::filesystem::path& aScratch) -> bool
	{
		write_file(aScratch / "CaseProbe", "x");
		return std::filesystem::exists(aScratch / "caseprobe");
	}

	void run_corpus(const harness_options& aOptions, const git_runner& aGit, const git_oracle& aOracle, const std::filesystem::path& aScratch,
					const std::vector<std::unique_ptr<git_metadata_candidate>>& aCandidates, report& aReport)
	{
		const auto folding = case_insensitive(aScratch);
		for (const auto& item : corpus()) {
			if ((item.Large && !aOptions.Large) || (aOptions.Only && *aOptions.Only != item.Name)) {
				continue;
			}
			const auto caseName = std::string{ item.Name };
			const auto skip     = [&](const std::string& aReason) {
				for (const auto& candidate : aCandidates) {
					for (const auto* component : { "C1", "C2", "C3", "C4", "C5", "C6" }) {
						aReport.add({ .Candidate = candidate->name(), .Component = component, .Case = caseName, .Verdict = verdict::skipped, .Detail = aReason });
					}
				}
			};
			if (aOracle.version() < item.Needs) {
				skip("needs Git " + version_text(item.Needs) + " for " + std::string{ item.Feature } + "; found " + version_text(aOracle.version()));
				continue;
			}
			auto       setup   = case_setup{ };
			const auto context = build_context{ .Git = aGit, .Oracle = aOracle, .Directory = aScratch / "corpus" / caseName, .Version = aOracle.version(), .CaseInsensitive = folding };
			if (const auto reason = item.Build(context, setup)) {
				skip(*reason);
				continue;
			}
			// check-ignore infers type from the filesystem. A trailing slash is
			// not a substitute: logs/ also matches logs/** where logs does not.
			// Back every declared directory with fixture bytes before any query
			// or snapshot, so both readers receive the same typed question.
			for (const auto& path : setup.ExtraPaths) {
				if (path.ends_with('/')) {
					const auto directory = setup.Root / path;
					std::filesystem::create_directories(directory);
					expect(std::filesystem::is_directory(directory), "an explicit directory question names a fixture directory");
				}
			}
			auto       onDisk     = std::set<std::string>{ };
			const auto questions  = questions_for(setup, onDisk);
			auto       references = std::vector<std::string>{ "HEAD" };
			{
				const auto [status, text] = aOracle.ask(setup.Root, { "for-each-ref", "--format=%(refname)" });
				auto stream               = std::istringstream{ text };
				for (auto line = std::string{ }; std::getline(stream, line);) {
					if (!line.empty()) {
						references.push_back(line);
					}
				}
				references.insert(references.end(), setup.ExtraReferences.begin(), setup.ExtraReferences.end());
				std::ranges::sort(references);
				const auto [first, last] = std::ranges::unique(references);
				references.erase(first, last);
			}
			// Git's answers first, then the candidates' between two snapshots.
			const auto oracleIndex      = aOracle.index(setup.Root);
			const auto oracleFormat     = aOracle.format(setup.Root);
			const auto oracleHead       = aOracle.head(setup.Root);
			const auto oracleDiscovery  = aOracle.discover(setup.Root);
			const auto oracleIgnored    = aOracle.ignored(setup.Root, questions);
			const auto oracleFilter     = aOracle.filter(setup.Root, questions);
			const auto oracleGenerated  = aOracle.linguist_generated(setup.Root, questions);
			auto       oracleReferences = std::vector<std::expected<std::optional<std::string>, candidate_refusal>>{ };
			for (const auto& name : references) {
				oracleReferences.push_back(aOracle.reference(setup.Root, name));
			}
			for (const auto& candidate : aCandidates) {
				const auto before = take_snapshot(context.Directory);
				const auto name   = candidate->name();
				aReport.add(compare(name, "C1", caseName, oracleIndex, candidate->index(setup.Root), compare_index));
				aReport.add(compare(name, "C2", caseName, oracleFormat, candidate->format(setup.Root), compare_format));
				auto headResult = compare(name, "C3", caseName, oracleHead, candidate->head(setup.Root), compare_head);
				if (headResult.Verdict != verdict::refused) {
					for (std::size_t index = 0; index < references.size(); ++index) {
						const auto answer = candidate->reference(setup.Root, references[index]);
						++headResult.Compared;
						if (answer.has_value() != oracleReferences[index].has_value() || (answer && *answer != *oracleReferences[index])) {
							const auto lhs = oracleReferences[index] ? optional_text(*oracleReferences[index]) : "refused: " + oracleReferences[index].error();
							const auto rhs = answer ? optional_text(*answer) : "refused: " + answer.error();
							headResult.Findings.push_back({ .Subject = references[index], .Oracle = lhs, .Candidate = rhs });
						}
					}
					if (!headResult.Findings.empty()) {
						headResult.Verdict = verdict::diverge;
					}
				}
				aReport.add(std::move(headResult));
				aReport.add(compare(name, "C4", caseName, oracleDiscovery, candidate->discover(setup.Root), compare_discovery));
				if (const auto reason = exceeds_h6(name, "C5", caseName)) {
					aReport.add({ .Candidate = name, .Component = "C5", .Case = caseName, .Verdict = verdict::skipped, .Detail = std::string{ *reason } });
				} else {
					aReport.add(compare(name, "C5", caseName, oracleIgnored, candidate->ignored(setup.Root, questions), per_path<bool>(questions)));
				}
				if (const auto reason = exceeds_h6(name, "C6", caseName)) {
					aReport.add({ .Candidate = name, .Component = "C6", .Case = caseName, .Verdict = verdict::skipped, .Detail = std::string{ *reason } });
				} else {
					aReport.add(compare(name, "C6", caseName, oracleFilter, candidate->filter(setup.Root, questions), per_path<attribute_answer>(questions)));
				}
				// [[BPR-1692]]. RFC-BPR-0089's head reader of linguist-generated,
				// over the attribute cases, against `git check-attr`.
				if (name == "K" && (caseName == "linguist-generated" || caseName == "attributes" || caseName == "ignorecase-false")) {
					aReport.add(compare(name, "C6-generated", caseName, oracleGenerated, candidate->linguist_generated(setup.Root, questions), per_path<attribute_answer>(questions)));
				}
				if (name == "K") {
					if (setup.SkipCapture) {
						aReport.add({ .Candidate = name, .Component = "C5-capture", .Case = caseName, .Verdict = verdict::skipped, .Detail = "not captured in this case" });
					} else {
						aReport.add(capture_cross_check(caseName, setup, questions, onDisk, oracleIgnored, oracleIndex));
					}
				}
				aReport.note_unchanged(name, caseName, compare_snapshots(before, take_snapshot(context.Directory)));
			}
			// Corpus repositories are large in places; free the space as we go.
			auto error = std::error_code{ };
			std::filesystem::remove_all(context.Directory, error);
		}
	}

	// ---------------------------------------------------------------------
	// Canary course (BPR-1456).
	// ---------------------------------------------------------------------

#if !defined(_WIN32)
	/// Records whether anything opened a FIFO for reading while it watched.
	/// A blocked reader is released by the watcher's own open, so nothing
	/// hangs.
	class fifo_watch
	{
	public:
		explicit fifo_watch(std::filesystem::path aFifo)
			: Fifo{ std::move(aFifo) }
			, Watcher{ [this](const std::stop_token& aStop) { watch(aStop); } }
		{
		}

		auto opened() -> bool
		{
			Watcher.request_stop();
			Watcher.join();
			return Opened.load();
		}

	private:
		void watch(const std::stop_token& aStop)
		{
			while (!aStop.stop_requested()) {
				const auto descriptor = ::open(Fifo.c_str(), O_WRONLY | O_NONBLOCK | O_CLOEXEC);
				if (descriptor >= 0) {
					Opened.store(true);
					::close(descriptor);
				}
				std::this_thread::sleep_for(std::chrono::microseconds{ 200 });
			}
		}

		std::filesystem::path Fifo;
		std::atomic<bool>     Opened{ false };
		std::jthread          Watcher;
	};
#endif

	auto run_canary(const git_runner& aGit, const std::filesystem::path& aScratch, const std::vector<std::unique_ptr<git_metadata_candidate>>& aCandidates) -> bool
	{
#if defined(_WIN32)
		static_cast<void>(aGit);
		static_cast<void>(aScratch);
		static_cast<void>(aCandidates);
		std::cout << "GMR canary skipped: the FIFO and symlink canaries are POSIX\n";
		return true;
#else
		const auto base    = aScratch / "canary";
		const auto outside = base / "outside";
		const auto repo    = base / "checkout";
		std::filesystem::create_directories(outside);
		const auto log    = outside / "canary.log";
		const auto logger = outside / "logger.sh";
		write_file(logger, "#!/bin/sh\necho \"$0 $*\" >> '" + log.string() + "'\nexit 0\n");
		std::filesystem::permissions(logger, std::filesystem::perms::owner_all);
		const auto fifo = outside / "include.fifo";
		expect(::mkfifo(fifo.c_str(), 0600) == 0, "make the include FIFO");
		write_file(outside / "evil.gitignore", "visible.txt\n*.txt\n");
		write_file(outside / "evil.gitattributes", "*.txt filter=outside\n");
		write_file(outside / "evil.config", "[core]\n\tignorecase = true\n");

		init(aGit, repo);
		plain_tree(repo);
		write_file(repo / ".gitignore", "*.log\n");
		write_file(repo / ".gitattributes", "*.txt filter=evil diff=evil\n");
		write_file(repo / "sub" / "visible.txt", "x\n");
		write_file(repo / "top.txt", "x\n");
		commit_all(aGit, repo);
		// Symlinked rule files point outside the root with rules that would
		// change the answers if they were followed.
		expect(tests::workspace_contract::try_symlink(outside / "evil.gitignore", repo / "sub" / ".gitignore", false), "symlinked .gitignore");
		expect(tests::workspace_contract::try_symlink(outside / "evil.gitattributes", repo / "sub" / ".gitattributes", false), "symlinked .gitattributes");
		// Every hook Git might run, and fsmonitor, filter and textconv drivers.
		for (const auto* hook : { "pre-commit", "post-checkout", "post-index-change", "reference-transaction", "fsmonitor-watchman", "post-merge", "pre-auto-gc", "post-rewrite",
								  "push-to-checkout", "sendemail-validate" }) {
			const auto path = repo / ".git" / "hooks" / hook;
			write_file(path, "#!/bin/sh\nexec '" + logger.string() + "' hook " + hook + "\n");
			std::filesystem::permissions(path, std::filesystem::perms::owner_all);
		}
		// Written last and by hand: `git config` itself would open the FIFO.
		append(repo / ".git" / "config", "[core]\n\tignorecase = false\n\tfsmonitor = " + logger.string() + "\n\thooksPath = " + (repo / ".git" / "hooks").string() + "\n\tpager = " + logger.string() + "\n[filter \"evil\"]\n\tclean = " + logger.string() + " clean\n\tsmudge = " + logger.string() + " smudge\n\tprocess = " + logger.string() + " process\n\trequired = true\n[diff \"evil\"]\n\ttextconv = " + logger.string() + " textconv\n[include]\n\tpath = " + fifo.string() + "\n\tpath = " + (outside / "evil.config").string() + "\n");

		const auto questions     = std::vector<path_question>{ { .Path = "sub", .Directory = true }, { .Path = "sub/.gitignore" }, { .Path = "sub/visible.txt" }, { .Path = "top.txt" }, { .Path = "debug.log" } };
		const auto expectIgnored = std::vector<bool>{ false, false, false, false, true };
		const auto expectFilter  = std::vector<attribute_answer>{ "unspecified", "unspecified", "evil", "evil", "unspecified" };

		auto passed = true;
		for (const auto& candidate : aCandidates) {
			const auto name = candidate->name();
			for (const auto* component : { "C1", "C2", "C3", "C4", "C5", "C6" }) {
				const auto before   = take_snapshot(repo);
				auto       watch    = fifo_watch{ fifo };
				auto       answered = std::string{ "answered" };
				auto       followed = false;
				const auto which    = std::string_view{ component };
				if (which == "C1") {
					const auto answer = candidate->index(repo);
					answered          = answer ? "answered" : "refused: " + answer.error();
				} else if (which == "C2") {
					const auto answer = candidate->format(repo);
					answered          = answer ? "answered" : "refused: " + answer.error();
					// The outside include sets ignoreCase; following it changes the answer.
					followed = answer && answer->IgnoreCase;
				} else if (which == "C3") {
					const auto answer = candidate->head(repo);
					const auto named  = candidate->reference(repo, "refs/heads/main");
					answered          = answer && named ? "answered" : "refused";
				} else if (which == "C4") {
					const auto answer = candidate->discover(repo);
					answered          = answer ? "answered" : "refused: " + answer.error();
				} else if (which == "C5") {
					const auto answer = candidate->ignored(repo, questions);
					answered          = answer ? "answered" : "refused: " + answer.error();
					followed          = answer && *answer != expectIgnored;
				} else {
					const auto answer = candidate->filter(repo, questions);
					answered          = answer ? "answered" : "refused: " + answer.error();
					followed          = answer && *answer != expectFilter;
				}
				const auto opened      = watch.opened();
				const auto executed    = std::filesystem::exists(log) && std::filesystem::file_size(log) > 0;
				const auto changes     = compare_snapshots(before, take_snapshot(repo));
				const auto contained   = !opened && !followed;
				const auto componentOk = contained && !executed && changes.empty();
				// K must pass; another candidate's failure is its H2 result,
				// reported here and weighed in the decision record.
				passed = passed && (componentOk || name != "K");
				std::cout << "GMR canary candidate=" << name << " component=" << component << " containment=" << (contained ? "pass" : "FAIL") << " fifo-opened=" << (opened ? "yes" : "no")
						  << " outside-rules-followed=" << (followed ? "yes" : "no") << " no-execution=" << (executed ? "FAIL" : "pass") << " no-write=" << (changes.empty() ? "pass" : "FAIL")
						  << " answer=\"" << printable(answered) << "\"\n";
				for (const auto& change : changes) {
					std::cout << "GMR canary change=\"" << printable(change) << "\"\n";
				}
			}
		}
		// Positive controls: the same canaries fire for real Git, so a pass
		// above is not a dead canary. Git opens the included FIFO, and runs
		// the fsmonitor hook for `status` once the fixture override is undone.
		auto fifoLive = false;
		auto execLive = false;
		{
			auto watch = fifo_watch{ fifo };
			static_cast<void>(aGit.status(repo, { "config", "--get", "core.bare" }, std::filesystem::path{ "/dev/null" }, base / "control.out"));
			fifoLive = watch.opened();
		}
		{
			auto watch = fifo_watch{ fifo };
			static_cast<void>(aGit.status(repo, { "-c", "core.fsmonitor=" + logger.string(), "status", "--porcelain" }, std::filesystem::path{ "/dev/null" }, base / "control.out"));
			static_cast<void>(watch.opened());
			execLive = std::filesystem::exists(log) && std::filesystem::file_size(log) > 0;
		}
		std::cout << "GMR canary-control git fifo=" << (fifoLive ? "fired" : "DEAD") << " fsmonitor=" << (execLive ? "fired" : "DEAD") << '\n';
		return passed && fifoLive && execLive;
#endif
	}

	/// [[BPR-1455]]. The fix for the confidentiality gap the harness found: a
	/// rule file past capture's bound stops capture with a named reason
	/// instead of being dropped, which captured the paths it ignores.
	void capture_refuses_rule_bounds(const git_runner& aGit, const std::filesystem::path& aScratch)
	{
		auto rules = std::string{ };
		for (int index = 0; index <= static_cast<int>(core::maximum_ignore_rules); ++index) {
			rules += "r" + std::to_string(index) + "\n";
		}
		const auto padded = "# " + std::string(1100U * 1024U, 'p') + "\nsecret.env\n";
		struct bound_case
		{
			std::string_view File;
			std::string      Contents;
		};
		for (const auto& [file, contents] : { bound_case{ ".gitignore", padded }, bound_case{ ".gitignore", rules + "secret.env\n" }, bound_case{ "sub/.gitignore", padded },
											  bound_case{ ".gitattributes", "# " + std::string(1100U * 1024U, 'p') + "\n" } }) {
			const auto repo = aScratch / "rule-bound";
			init(aGit, repo);
			write_file(repo / "tracked.txt", "x\n");
			commit_all(aGit, repo);
			write_file(repo / std::string{ file }, contents);
			write_file(repo / "secret.env", "x\n");
			write_file(repo / "sub" / "secret.env", "x\n");
			auto       source = adapters::workspace::make_local_workspace_source();
			auto       cycle  = tests::workspace_contract::course{ *source, repo };
			const auto taken  = cycle.capture(*source);
			const auto named  = taken.Repository && taken.Repository->Reason == core::git_repository_refusal::rule_file_bound && taken.Repository->Key == file;
			std::cout << "GMR fix capture-rule-bound file=" << file << " status=" << static_cast<int>(taken.Status) << " words=\"" << application::cycle_capture_words(taken) << "\"\n";
			expect(taken.Status == application::cycle_capture_status::workspace && named, "a rule file past capture's bound refuses the capture by name");
			auto error = std::error_code{ };
			std::filesystem::remove_all(repo, error);
		}
	}

	auto parse_options(const int aArgc, char** aArgv) -> harness_options
	{
		auto options = harness_options{ };
		for (int index = 1; index < aArgc; ++index) {
			const auto argument = std::string_view{ aArgv[index] };
			if (argument == "--large") {
				options.Large = true;
			} else if (argument == "--no-canary") {
				options.SkipCanary = true;
			} else if (argument == "--case" && index + 1 < aArgc) {
				options.Only = aArgv[++index];
			} else if (argument == "--report" && index + 1 < aArgc) {
				options.Tsv = aArgv[++index];
			}
		}
		return options;
	}
}

auto main(const int aArgc, char** aArgv) -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "git-metadata-harness" };
	const auto                           git = tests::workspace_contract::git_from(aArgc, aArgv);
	if (!git) {
		std::cout << "SKIPPED: no Git in this build; the differential harness did not run\n";
		return EXIT_SUCCESS;
	}
	const auto options = parse_options(aArgc, aArgv);
	tests::workspace_contract::isolate_git_environment(scratch.path());
	const auto version = tests::workspace_contract::installed_git_version(*git, scratch.path());
	std::cout << "GMR git " << version_text(version) << '\n';
	if (version < std::pair{ 2, 45 }) {
		std::cout << "SKIPPED: the oracle needs Git 2.45 (rev-parse --show-ref-format); found " << version_text(version) << '\n';
		return EXIT_SUCCESS;
	}
	const auto oracle     = git_oracle{ *git, scratch.path(), version };
	auto       candidates = std::vector<std::unique_ptr<git_metadata_candidate>>{ };
	candidates.push_back(tests::workspace_contract::make_first_party_candidate());
#if defined(BPR_HAVE_LIBGIT2_CANDIDATE)
	candidates.push_back(tests::workspace_contract::make_libgit2_candidate());
#endif
	if (!options.Only) {
		capture_refuses_rule_bounds(*git, scratch.path());
	}
	auto results = report{ };
	run_corpus(options, *git, oracle, scratch.path(), candidates, results);
	const auto canary = options.SkipCanary || options.Only || run_canary(*git, scratch.path(), candidates);
	results.summarize(options.Tsv);
	std::cout << "GMR canary " << (canary ? "pass" : "FAIL") << '\n';
	expect(results.passed(), "every divergence is classified and no candidate wrote to a checkout");
	expect(canary, "no canary fired");
	std::cout << "git-metadata-harness:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return tests::failing_exit();
}
