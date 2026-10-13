// [[RFC-BPR-0082]] [[BPR-1466]] [[BPR-1467]]. The diff-engine evaluation
// harness. Every candidate result goes through the core's seam and
// first-party validation; the harness only times, stops and compares.
//
// Modes (each writes JSON lines to --out):
//   measure      corpus runs: time to final result, outcome, validation,
//                approximation, lost intraline emphasis, edit size, oracle
//                agreement and comment flips, digests; interleaved rounds
//   phases       pairing and intraline phase times from a polling timeline
//   intraline    per-pair intraline time over the corpus's similar pairs
//   stops        stop-to-return latency, at the host, including kill and reap
//   overhead     per-launch process overhead of each Git program
//   determinism  repeated runs and cross-version process digests
//   rss          peak resident memory, one child process per comparison
//   single       one comparison in this process; prints its peak memory
//
// Usage: diff_engine_evaluation <mode> --corpus DIR --out FILE
//        [--candidates a,b] [--pairs substring] [--rounds N] [--reps N]

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_compute_components.hpp"
#include "candidates/candidates.hpp"
#include "harness/oracle.hpp"
#include "harness/supervised_git.hpp"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <spawn.h>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

extern char** environ;

namespace {
	using namespace bpr;
	using clock_type = std::chrono::steady_clock;

	// ---------------------------------------------------------------- input

	struct corpus_pair
	{
		std::string                                         Id;
		std::string                                         Class;
		std::string                                         Size;
		std::string                                         Origin;
		std::string                                         OptionName;
		core::diff_options                                  Options;
		std::vector<std::uint8_t>                           Left;
		std::vector<std::uint8_t>                           Right;
		std::string                                         LeftPath;
		std::string                                         RightPath;
		std::optional<std::vector<evaluation::oracle_hunk>> Oracle;
		std::string                                         OracleKind;
	};

	[[nodiscard]] auto read_bytes(const std::filesystem::path& aPath) -> std::vector<std::uint8_t>
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	[[nodiscard]] auto parse_options(const std::string& aName) -> core::diff_options
	{
		auto options = core::diff_options{ };
		if (aName.find("ignore-trailing") != std::string::npos) {
			options.Whitespace = core::whitespace_policy::ignore_trailing;
		}
		if (aName.find("ignore-all") != std::string::npos) {
			options.Whitespace = core::whitespace_policy::ignore_all;
		}
		if (aName.find("normalize-endings") != std::string::npos) {
			options.LineEndings = core::line_ending_policy::normalize;
		}
		return options;
	}

	[[nodiscard]] auto load_corpus(const std::filesystem::path& aDirectory, const std::string& aFilter) -> std::vector<corpus_pair>
	{
		auto file = QFile{ QString::fromStdString((aDirectory / "manifest.json").string()) };
		if (!file.open(QIODevice::ReadOnly)) {
			std::fprintf(stderr, "cannot read the corpus manifest\n");
			std::exit(2);
		}
		const auto document = QJsonDocument::fromJson(file.readAll());
		auto       result   = std::vector<corpus_pair>{ };
		for (const auto value : document.object().value("pairs").toArray()) {
			const auto entry = value.toObject();
			auto       pair  = corpus_pair{
				.Id         = entry.value("id").toString().toStdString(),
				.Class      = entry.value("class").toString().toStdString(),
				.Size       = entry.value("size").toString().toStdString(),
				.Origin     = entry.value("origin").toString().toStdString(),
				.OptionName = entry.value("options").toString().toStdString(),
			};
			if (!aFilter.empty() && pair.Id.find(aFilter) == std::string::npos) {
				continue;
			}
			pair.Options      = parse_options(pair.OptionName);
			pair.LeftPath     = (aDirectory / entry.value("left").toString().toStdString()).string();
			pair.RightPath    = (aDirectory / entry.value("right").toString().toStdString()).string();
			pair.Left         = read_bytes(pair.LeftPath);
			pair.Right        = read_bytes(pair.RightPath);
			const auto oracle = entry.value("oracle").toObject();
			if (!oracle.isEmpty()) {
				pair.OracleKind = oracle.value("kind").toString().toStdString();
				auto hunks      = std::vector<evaluation::oracle_hunk>{ };
				for (const auto hunk : oracle.value("hunks").toArray()) {
					const auto values = hunk.toArray();
					hunks.push_back({ static_cast<std::uint32_t>(values.at(0).toInteger()), static_cast<std::uint32_t>(values.at(1).toInteger()),
									  static_cast<std::uint32_t>(values.at(2).toInteger()), static_cast<std::uint32_t>(values.at(3).toInteger()) });
				}
				pair.Oracle = std::move(hunks);
			}
			result.push_back(std::move(pair));
		}
		return result;
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] auto comparison_key(const core::diff_options& aOptions) -> core::file_comparison_key
	{
		auto blob = [](const std::uint8_t aSeed) {
			std::array<std::uint8_t, 32U> bytes{ };
			for (std::size_t index = 0U; index < 20U; ++index) {
				bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
			}
			return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
		};
		const auto scope       = opaque<core::pull_request_authority_scope_id>(11U);
		const auto incarnation = opaque<core::epoch_catalog_incarnation_id>(12U);
		const auto authority   = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(13U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = scope,
		};
		return core::file_comparison_key{
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = { .Authority = scope, .Incarnation = incarnation, .Kind = core::comparison_anchor_kind::epoch_base, .Epoch = core::epoch_ordinal{ 1 } },
			.RightAnchor = { .Authority = scope, .Incarnation = incarnation, .Kind = core::comparison_anchor_kind::epoch_head, .Epoch = core::epoch_ordinal{ 2 } },
			.LeftBlob    = blob(21U),
			.RightBlob   = blob(51U),
			.File        = opaque<core::file_identity>(31U),
			.Options     = *core::options_digest(aOptions),
			.Algorithm   = core::supported_diff_algorithm_revision,
		};
	}

	[[nodiscard]] auto input_of(const corpus_pair& aPair) -> core::diff_compute_input
	{
		return {
			.Key        = comparison_key(aPair.Options),
			.Options    = aPair.Options,
			.LeftBytes  = std::span<const std::uint8_t>{ aPair.Left },
			.RightBytes = std::span<const std::uint8_t>{ aPair.Right },
		};
	}

	// ---------------------------------------------------------------- output

	class json_line
	{
	public:
		auto add(const char* aKey, const std::string& aValue) -> json_line&
		{
			Object.insert(aKey, QString::fromStdString(aValue));
			return *this;
		}
		auto add(const char* aKey, const double aValue) -> json_line&
		{
			Object.insert(aKey, aValue);
			return *this;
		}
		auto add(const char* aKey, const std::uint64_t aValue) -> json_line&
		{
			Object.insert(aKey, static_cast<qint64>(aValue));
			return *this;
		}
		auto add(const char* aKey, const bool aValue) -> json_line&
		{
			Object.insert(aKey, aValue);
			return *this;
		}
		[[nodiscard]] auto text() const -> std::string
		{
			return QJsonDocument{ Object }.toJson(QJsonDocument::Compact).toStdString();
		}

	private:
		QJsonObject Object;
	};

	class output_file
	{
	public:
		explicit output_file(const std::string& aPath)
			: Stream{ aPath, std::ios::app }
		{
		}
		auto write(const json_line& aLine) -> void
		{
			Stream << aLine.text() << '\n';
			Stream.flush();
		}

	private:
		std::ofstream Stream;
	};

	[[nodiscard]] auto milliseconds_since(const clock_type::time_point aStart) -> double
	{
		return std::chrono::duration<double, std::milli>(clock_type::now() - aStart).count();
	}

	[[nodiscard]] auto milliseconds_between(const clock_type::time_point aStart, const clock_type::time_point aEnd) -> double
	{
		return std::chrono::duration<double, std::milli>(aEnd - aStart).count();
	}

	// ---------------------------------------------------------------- digests

	class fnv
	{
	public:
		auto add(const std::uint64_t aValue) -> void
		{
			for (int shift = 0; shift < 64; shift += 8) {
				Value ^= (aValue >> shift) & 0xFFU;
				Value *= 0x100000001B3ULL;
			}
		}
		[[nodiscard]] auto hex() const -> std::string
		{
			auto text = std::array<char, 17U>{ };
			std::snprintf(text.data(), text.size(), "%016llx", static_cast<unsigned long long>(Value));
			return text.data();
		}

	private:
		std::uint64_t Value = 0xCBF29CE484222325ULL;
	};

	struct result_summary
	{
		std::string                                 Reason;
		bool                                        Published     = false;
		bool                                        Valid         = false;
		bool                                        Approximate   = false;
		std::uint64_t                               EqualLines    = 0U;
		std::uint64_t                               EditSize      = 0U;
		std::uint64_t                               Groups        = 0U;
		std::uint64_t                               SimilarPairs  = 0U;
		std::uint64_t                               ExactPairs    = 0U;
		std::uint64_t                               LostIntraline = 0U;
		std::uint64_t                               Intraline     = 0U;
		std::uint64_t                               MatrixCells   = 0U;
		std::string                                 SegmentsDigest;
		std::string                                 PairsDigest;
		std::string                                 IntralineDigest;
		std::optional<evaluation::oracle_agreement> Agreement;
	};

	[[nodiscard]] auto summarize(const core::diff_compute_outcome& aOutcome, const corpus_pair& aPair) -> result_summary
	{
		auto summary      = result_summary{ .Reason = std::to_string(static_cast<int>(aOutcome.Reason)) };
		summary.Published = aOutcome.Result.has_value();
		if (!aOutcome.Result.has_value()) {
			return summary;
		}
		const auto& diff    = *aOutcome.Result;
		summary.Valid       = core::validate_computed_file_diff(diff).valid();
		summary.Approximate = diff.AlignmentApproximate;
		summary.MatrixCells = diff.MatrixCellsConsumed;
		summary.Groups      = diff.Alignment.Groups.size();
		auto segments       = fnv{ };
		for (const auto& segment : diff.Alignment.Segments) {
			segments.add(static_cast<std::uint64_t>(segment.Kind));
			segments.add((static_cast<std::uint64_t>(segment.Left.Begin) << 32U) | segment.Left.End);
			segments.add((static_cast<std::uint64_t>(segment.Right.Begin) << 32U) | segment.Right.End);
			if (segment.Kind == core::alignment_kind::equal) {
				summary.EqualLines += segment.Left.extent();
			}
		}
		summary.EditSize       = static_cast<std::uint64_t>(diff.Alignment.LeftLineCount) + diff.Alignment.RightLineCount - 2U * summary.EqualLines;
		summary.SegmentsDigest = segments.hex();
		auto pairs             = fnv{ };
		auto withEdits         = std::map<std::string, bool>{ };
		auto edited            = std::vector<bool>(diff.Alignment.Pairs.size(), false);
		auto pairIndex         = std::size_t{ 0U };
		auto intraline         = fnv{ };
		for (const auto& edit : diff.Intraline) {
			while (pairIndex < diff.Alignment.Pairs.size() && diff.Alignment.Pairs[pairIndex].Id != edit.Pair) {
				++pairIndex;
			}
			if (pairIndex < edited.size()) {
				edited[pairIndex] = true;
			}
			intraline.add(pairIndex);
			intraline.add((edit.Left.Begin << 32U) | edit.Left.End);
			intraline.add((edit.Right.Begin << 32U) | edit.Right.End);
		}
		for (std::size_t index = 0U; index < diff.Alignment.Pairs.size(); ++index) {
			const auto& pair = diff.Alignment.Pairs[index];
			pairs.add((static_cast<std::uint64_t>(pair.LeftLine) << 32U) | pair.RightLine);
			pairs.add(static_cast<std::uint64_t>(pair.Kind));
			if (pair.Kind == core::line_pair_kind::similar) {
				++summary.SimilarPairs;
				summary.LostIntraline += edited[index] ? 0U : 1U;
			} else {
				++summary.ExactPairs;
			}
		}
		summary.Intraline       = diff.Intraline.size();
		summary.PairsDigest     = pairs.hex();
		summary.IntralineDigest = intraline.hex();
		if (aPair.Oracle.has_value()) {
			summary.Agreement = evaluation::compare_with_oracle(diff.Alignment, *aPair.Oracle);
		}
		return summary;
	}

	auto add_summary(json_line& aLine, const result_summary& aSummary) -> void
	{
		aLine.add("reason", aSummary.Reason)
			.add("published", aSummary.Published)
			.add("valid", aSummary.Valid)
			.add("approximate", aSummary.Approximate)
			.add("equal_lines", aSummary.EqualLines)
			.add("edit_size", aSummary.EditSize)
			.add("groups", aSummary.Groups)
			.add("similar_pairs", aSummary.SimilarPairs)
			.add("exact_pairs", aSummary.ExactPairs)
			.add("lost_intraline", aSummary.LostIntraline)
			.add("intraline", aSummary.Intraline)
			.add("matrix_cells", aSummary.MatrixCells)
			.add("segments_digest", aSummary.SegmentsDigest)
			.add("pairs_digest", aSummary.PairsDigest)
			.add("intraline_digest", aSummary.IntralineDigest);
		if (aSummary.Agreement.has_value()) {
			aLine.add("oracle_exact", aSummary.Agreement->Exact)
				.add("oracle_hunks", std::uint64_t{ aSummary.Agreement->OracleHunks })
				.add("candidate_hunks", std::uint64_t{ aSummary.Agreement->CandidateHunks })
				.add("comment_flips", aSummary.Agreement->CommentFlips);
		}
	}

	// ---------------------------------------------------------------- process shape

	/// A scratch repository per Git program, holding every corpus blob: a
	/// pull request's comparison diffs two blobs already in its retained
	/// repository, so the process shape is timed blob to blob.
	class blob_store
	{
	public:
		blob_store(const evaluation::git_program& aProgram, const std::filesystem::path& aRoot)
			: Program{ aProgram }
			, Directory{ aRoot / ("blobs-" + aProgram.Version) }
		{
			std::filesystem::create_directories(Directory);
			if (!std::filesystem::exists(Directory / "HEAD")) {
				static_cast<void>(evaluation::run_supervised_git(Program.Path, { "init", "--bare", "--quiet", "--template=", "." }, Directory.string(), nullptr));
			}
		}

		[[nodiscard]] auto blob(const std::string& aPath) -> std::string
		{
			if (const auto found = Blobs.find(aPath); found != Blobs.end()) {
				return found->second;
			}
			const auto run = evaluation::run_supervised_git(Program.Path, { "hash-object", "-w", "--no-filters", aPath }, Directory.string(), nullptr);
			auto       oid = run.Output.value_or("");
			while (!oid.empty() && (oid.back() == '\n' || oid.back() == '\r')) {
				oid.pop_back();
			}
			Blobs.emplace(aPath, oid);
			return oid;
		}

		[[nodiscard]] auto directory() const -> std::string
		{
			return Directory.string();
		}

		[[nodiscard]] auto program() const -> const evaluation::git_program&
		{
			return Program;
		}

	private:
		evaluation::git_program            Program;
		std::filesystem::path              Directory;
		std::map<std::string, std::string> Blobs;
	};

	struct process_spec
	{
		std::size_t               Program   = 0U;
		evaluation::git_algorithm Algorithm = evaluation::git_algorithm::myers;
		bool                      Indent    = false;
	};

	[[nodiscard]] auto parse_process(const std::string& aName, const std::vector<evaluation::git_program>& aPrograms) -> std::optional<process_spec>
	{
		if (!aName.starts_with("p-")) {
			return std::nullopt;
		}
		auto text   = std::string_view{ aName }.substr(2U);
		auto spec   = process_spec{ };
		spec.Indent = text.ends_with("-indent");
		if (spec.Indent) {
			text.remove_suffix(7U);
		}
		const auto dash      = text.rfind('-');
		const auto version   = text.substr(0U, dash);
		const auto algorithm = text.substr(dash + 1U);
		for (const auto candidate : { evaluation::git_algorithm::myers, evaluation::git_algorithm::minimal, evaluation::git_algorithm::patience, evaluation::git_algorithm::histogram }) {
			if (algorithm == evaluation::name(candidate)) {
				spec.Algorithm = candidate;
			}
		}
		for (std::size_t index = 0U; index < aPrograms.size(); ++index) {
			if (aPrograms[index].Version == version) {
				spec.Program = index;
				return spec;
			}
		}
		return std::nullopt;
	}

	struct process_result
	{
		std::optional<core::diff_compute_outcome> Outcome;
		double                                    ProcessMs = 0.0;
		double                                    CoreMs    = 0.0;
	};

	[[nodiscard]] auto line_count(const std::vector<std::uint8_t>& aBytes) -> std::uint32_t
	{
		const auto newlines = static_cast<std::uint32_t>(std::ranges::count(aBytes, std::uint8_t{ '\n' }));
		return newlines + (aBytes.empty() || aBytes.back() == '\n' ? 0U : 1U);
	}

	[[nodiscard]] auto run_process_candidate(blob_store& aStore, const process_spec& aSpec, const corpus_pair& aPair, const std::atomic_bool* aStop = nullptr,
											 evaluation::supervised_run* aRun = nullptr) -> process_result
	{
		auto       result    = process_result{ };
		const auto left      = aStore.blob(aPair.LeftPath);
		const auto right     = aStore.blob(aPair.RightPath);
		auto       arguments = evaluation::git_diff_arguments(aSpec.Algorithm, aSpec.Indent, aPair.Options);
		arguments.push_back(left);
		arguments.push_back(right);
		const auto start = clock_type::now();
		auto       run   = evaluation::run_supervised_git(aStore.program().Path, arguments, aStore.directory(), aStop);
		result.ProcessMs = milliseconds_since(start);
		if (aRun != nullptr) {
			*aRun = run;
		}
		if (!run.Output.has_value()) {
			return result;
		}
		const auto coreStart = clock_type::now();
		const auto hunks     = evaluation::parse_zero_context_hunks(*run.Output);
		auto       matches   = hunks.has_value() ? evaluation::matches_from_hunks(*hunks, line_count(aPair.Left), line_count(aPair.Right)) : std::nullopt;
		if (!matches.has_value()) {
			matches = std::vector<core::diff_match>{ { .Left = UINT32_MAX, .Right = UINT32_MAX } };
		}
		result.Outcome = core::compute_file_diff(input_of(aPair), core::diff_primary_observation{ .Matches = std::move(*matches) });
		result.CoreMs  = milliseconds_since(coreStart);
		return result;
	}

	// ---------------------------------------------------------------- options

	struct arguments
	{
		std::string              Mode;
		std::filesystem::path    Corpus;
		std::string              Out;
		std::vector<std::string> Candidates;
		std::string              PairFilter;
		int                      Rounds = 3;
		int                      Reps   = 3;
		std::string              Single;
	};

	[[nodiscard]] auto split(const std::string& aText) -> std::vector<std::string>
	{
		auto result = std::vector<std::string>{ };
		auto stream = std::stringstream{ aText };
		auto item   = std::string{ };
		while (std::getline(stream, item, ',')) {
			if (!item.empty()) {
				result.push_back(item);
			}
		}
		return result;
	}

	[[nodiscard]] auto parse_arguments(const int aArgc, char** aArgv) -> arguments
	{
		auto result = arguments{ };
		if (aArgc < 2) {
			std::fprintf(stderr, "usage: diff_engine_evaluation <mode> --corpus DIR --out FILE [options]\n");
			std::exit(2);
		}
		result.Mode = aArgv[1];
		for (int index = 2; index + 1 < aArgc; index += 2) {
			const auto key   = std::string_view{ aArgv[index] };
			const auto value = std::string{ aArgv[index + 1] };
			if (key == "--corpus") {
				result.Corpus = value;
			} else if (key == "--out") {
				result.Out = value;
			} else if (key == "--candidates") {
				result.Candidates = split(value);
			} else if (key == "--pairs") {
				result.PairFilter = value;
			} else if (key == "--rounds") {
				result.Rounds = std::max(1, std::atoi(value.c_str()));
			} else if (key == "--reps") {
				result.Reps = std::max(1, std::atoi(value.c_str()));
			} else if (key == "--single") {
				result.Single = value;
			}
		}
		// Git runs in its scratch repository, so every path it is given is
		// absolute.
		if (!result.Corpus.empty()) {
			result.Corpus = std::filesystem::absolute(result.Corpus);
		}
		if (!result.Out.empty()) {
			result.Out = std::filesystem::absolute(result.Out).string();
		}
		if (result.Candidates.empty()) {
			for (const auto& description : evaluation::candidate_descriptions()) {
				result.Candidates.push_back(description.Name);
			}
		}
		return result;
	}

	auto base_line(json_line& aLine, const std::string& aMode, const std::string& aCandidate, const corpus_pair& aPair) -> void
	{
		const auto description = evaluation::find_candidate(aCandidate);
		aLine.add("mode", aMode)
			.add("candidate", aCandidate)
			.add("component", std::to_string(static_cast<int>(description.has_value() ? description->Component : evaluation::component_kind::baseline)))
			.add("shape", std::to_string(static_cast<int>(description.has_value() ? description->Shape : evaluation::shape_kind::baseline)))
			.add("pair", aPair.Id)
			.add("class", aPair.Class)
			.add("size", aPair.Size)
			.add("options", aPair.OptionName)
			.add("oracle_kind", aPair.OracleKind);
	}

	/// Times one in-process candidate: the baseline entry point, or the
	/// candidate's components through the seam.
	[[nodiscard]] auto run_in_process(evaluation::candidate_components* aComponents, const corpus_pair& aPair) -> std::pair<core::diff_compute_outcome, double>
	{
		const auto input   = input_of(aPair);
		const auto start   = clock_type::now();
		auto       outcome = aComponents == nullptr ? core::compute_file_diff(input) : core::compute_file_diff(input, *aComponents);
		return { std::move(outcome), milliseconds_since(start) };
	}

	// ---------------------------------------------------------------- measure

	auto measure(const arguments& aArguments, const std::vector<corpus_pair>& aPairs) -> void
	{
		auto       out      = output_file{ aArguments.Out };
		const auto programs = evaluation::git_programs();
		auto       stores   = std::vector<std::unique_ptr<blob_store>>{ };
		const auto scratch  = std::filesystem::path{ aArguments.Out }.parent_path() / "scratch";
		for (const auto& program : programs) {
			stores.push_back(std::make_unique<blob_store>(program, scratch));
		}
		auto components = std::map<std::string, std::unique_ptr<evaluation::candidate_components>>{ };
		for (const auto& name : aArguments.Candidates) {
			components.emplace(name, evaluation::make_candidate(name));
		}
		for (int round = 0; round < aArguments.Rounds; ++round) {
			for (std::size_t pairIndex = 0U; pairIndex < aPairs.size(); ++pairIndex) {
				const auto& pair = aPairs[pairIndex];
				// Rotate the candidate order every pair and round, so no
				// candidate always runs first or warm.
				const auto count = aArguments.Candidates.size();
				for (std::size_t step = 0U; step < count; ++step) {
					const auto& name = aArguments.Candidates[(step + pairIndex + static_cast<std::size_t>(round)) % count];
					for (int rep = 0; rep < aArguments.Reps; ++rep) {
						auto line = json_line{ };
						base_line(line, "measure", name, pair);
						line.add("round", static_cast<std::uint64_t>(round)).add("rep", static_cast<std::uint64_t>(rep));
						if (const auto spec = parse_process(name, programs); spec.has_value()) {
							const auto result = run_process_candidate(*stores[spec->Program], *spec, pair);
							line.add("time_ms", result.ProcessMs + result.CoreMs).add("process_ms", result.ProcessMs).add("core_ms", result.CoreMs);
							if (result.Outcome.has_value()) {
								add_summary(line, summarize(*result.Outcome, pair));
							} else {
								line.add("reason", std::string{ "process-failure" }).add("published", false).add("valid", false);
							}
						} else {
							auto* candidate          = components.at(name).get();
							const auto [outcome, ms] = run_in_process(candidate, pair);
							line.add("time_ms", ms);
							add_summary(line, summarize(outcome, pair));
						}
						out.write(line);
					}
				}
			}
			std::fprintf(stderr, "measure round %d of %d done\n", round + 1, aArguments.Rounds);
		}
	}

	// ---------------------------------------------------------------- phases

	/// Never stops; records the wall time at each poll, so phase boundaries
	/// (in steps) map to time.
	class timeline final : public core::diff_compute_stop
	{
	public:
		[[nodiscard]] auto stop_requested(const std::uint64_t aSteps) -> bool override
		{
			Samples.emplace_back(aSteps, clock_type::now());
			return false;
		}

		[[nodiscard]] auto time_at(const std::uint64_t aSteps, const clock_type::time_point aEnd) const -> clock_type::time_point
		{
			for (const auto& [steps, time] : Samples) {
				if (steps >= aSteps) {
					return time;
				}
			}
			return aEnd;
		}

		std::vector<std::pair<std::uint64_t, clock_type::time_point>> Samples;
	};

	auto phases(const arguments& aArguments, const std::vector<corpus_pair>& aPairs) -> void
	{
		auto out = output_file{ aArguments.Out };
		for (int round = 0; round < aArguments.Rounds; ++round) {
			for (const auto& pair : aPairs) {
				for (const auto& name : aArguments.Candidates) {
					if (name.starts_with("p-")) {
						continue;
					}
					auto components = evaluation::make_candidate(name);
					auto recorder   = timeline{ };
					recorder.Samples.reserve(1U << 16U);
					const auto control = core::diff_compute_control{ .Stop = &recorder, .CheckInterval = 4'096U };
					const auto start   = clock_type::now();
					recorder.Samples.emplace_back(0U, start);
					auto        slice  = components == nullptr ? core::compute_file_diff(input_of(pair), control) : core::compute_file_diff(input_of(pair), control, *components);
					const auto  end    = clock_type::now();
					const auto& starts = slice.Statistics.PhaseStarts;
					const auto  at     = [&](const core::diff_compute_phase aPhase) { return starts[static_cast<std::size_t>(aPhase)]; };
					auto        line   = json_line{ };
					base_line(line, "phases", name, pair);
					line.add("round", static_cast<std::uint64_t>(round))
						.add("total_ms", milliseconds_between(start, end))
						.add("primary_ms", milliseconds_between(recorder.time_at(at(core::diff_compute_phase::primary), end), recorder.time_at(at(core::diff_compute_phase::refine), end)))
						.add("refine_ms", milliseconds_between(recorder.time_at(at(core::diff_compute_phase::refine), end), recorder.time_at(at(core::diff_compute_phase::moves), end)))
						.add("assemble_ms", milliseconds_between(recorder.time_at(at(core::diff_compute_phase::assemble), end), recorder.time_at(at(core::diff_compute_phase::validate), end)))
						.add("published", slice.Outcome.has_value() && slice.Outcome->Result.has_value());
					out.write(line);
				}
			}
		}
	}

	// ---------------------------------------------------------------- intraline

	/// Every similar pair of the baseline's results, timed alone: the same
	/// two lines compared by the full pipeline with and without the
	/// candidate's intraline component.
	auto intraline(const arguments& aArguments, const std::vector<corpus_pair>& aPairs) -> void
	{
		auto out = output_file{ aArguments.Out };
		for (const auto& pair : aPairs) {
			const auto baseline = core::compute_file_diff(input_of(pair));
			if (!baseline.Result.has_value()) {
				continue;
			}
			const auto& diff  = *baseline.Result;
			auto        taken = std::size_t{ 0U };
			for (const auto& linePair : diff.Alignment.Pairs) {
				if (linePair.Kind != core::line_pair_kind::similar || taken >= 400U) {
					continue;
				}
				++taken;
				const auto lineBytes = [](const core::decoded_text_document& aDocument, const std::uint32_t aLine) {
					const auto& record = aDocument.Lines[aLine];
					auto        bytes  = std::vector<std::uint8_t>{ aDocument.Bytes.begin() + static_cast<std::ptrdiff_t>(record.Content.Begin),
																	aDocument.Bytes.begin() + static_cast<std::ptrdiff_t>(record.Content.End) };
					bytes.push_back('\n');
					return bytes;
				};
				auto single  = corpus_pair{ .Id = pair.Id + "#" + std::to_string(linePair.LeftLine), .Class = pair.Class, .Size = pair.Size, .OptionName = pair.OptionName, .Options = pair.Options };
				single.Left  = lineBytes(*diff.LeftDocument, linePair.LeftLine);
				single.Right = lineBytes(*diff.RightDocument, linePair.RightLine);
				for (const auto& name : aArguments.Candidates) {
					auto components = evaluation::make_candidate(name);
					auto samples    = std::vector<double>{ };
					auto summary    = result_summary{ };
					for (int rep = 0; rep < std::max(aArguments.Reps, 5); ++rep) {
						const auto [outcome, ms] = run_in_process(components.get(), single);
						samples.push_back(ms);
						if (rep == 0) {
							summary = summarize(outcome, single);
						}
					}
					std::ranges::sort(samples);
					auto line = json_line{ };
					base_line(line, "intraline", name, single);
					line.add("source_pair", pair.Id)
						.add("left_bytes", static_cast<std::uint64_t>(single.Left.size()))
						.add("right_bytes", static_cast<std::uint64_t>(single.Right.size()))
						.add("time_ms", samples[samples.size() / 2U]);
					add_summary(line, summary);
					out.write(line);
				}
			}
		}
	}

	// ---------------------------------------------------------------- stops

	class flag_stop final : public core::diff_compute_stop
	{
	public:
		explicit flag_stop(const std::atomic_bool& aFlag) noexcept
			: Flag{ &aFlag }
		{
		}
		[[nodiscard]] auto stop_requested(std::uint64_t /*aSteps*/) -> bool override
		{
			return Flag->load(std::memory_order_acquire);
		}

	private:
		const std::atomic_bool* Flag = nullptr;
	};

	/// The linked shape has no stop hook: its host gives up on the worker
	/// and returns at once, and the worker runs on until it ends.
	struct abandonable_result
	{
		double HostReturnMs      = 0.0;
		double WorkerRemainingMs = 0.0;
		bool   Stopped           = false;
	};

	auto stops(const arguments& aArguments, const std::vector<corpus_pair>& aPairs) -> void
	{
		auto       out      = output_file{ aArguments.Out };
		const auto programs = evaluation::git_programs();
		auto       stores   = std::vector<std::unique_ptr<blob_store>>{ };
		const auto scratch  = std::filesystem::path{ aArguments.Out }.parent_path() / "scratch";
		for (const auto& program : programs) {
			stores.push_back(std::make_unique<blob_store>(program, scratch));
		}
		constexpr auto fractions = std::array{ 0.15, 0.35, 0.55, 0.75, 0.95 };
		for (const auto& pair : aPairs) {
			for (const auto& name : aArguments.Candidates) {
				const auto spec = parse_process(name, programs);
				// The uninterrupted time places the stops.
				auto full = 0.0;
				if (spec.has_value()) {
					full = run_process_candidate(*stores[spec->Program], *spec, pair).ProcessMs;
				} else {
					auto components = evaluation::make_candidate(name);
					full            = run_in_process(components.get(), pair).second;
				}
				for (const auto fraction : fractions) {
					for (int rep = 0; rep < aArguments.Reps; ++rep) {
						auto       flag      = std::atomic_bool{ false };
						auto       stoppedAt = clock_type::time_point{ };
						auto       returned  = clock_type::time_point{ };
						auto       finished  = std::atomic_bool{ false };
						auto       paused    = false;
						auto       remaining = 0.0;
						const auto linked    = name.starts_with("l-");
						const auto delay     = std::chrono::duration<double, std::milli>{ full * fraction };
						if (spec.has_value()) {
							auto worker = std::thread{ [&] {
								auto run = evaluation::supervised_run{ };
								static_cast<void>(run_process_candidate(*stores[spec->Program], *spec, pair, &flag, &run));
								paused   = run.Stopped;
								returned = run.Returned;
								finished.store(true, std::memory_order_release);
							} };
							std::this_thread::sleep_for(delay);
							stoppedAt = clock_type::now();
							flag.store(true, std::memory_order_release);
							worker.join();
						} else if (linked) {
							// Host side: wait for the worker or the stop, whichever
							// comes first; a stop abandons the worker.
							auto mutex      = std::mutex{ };
							auto condition  = std::condition_variable{ };
							auto done       = std::make_shared<std::atomic_bool>(false);
							auto doneAt     = std::make_shared<clock_type::time_point>();
							auto components = std::shared_ptr<evaluation::candidate_components>{ evaluation::make_candidate(name) };
							auto worker     = std::thread{ [components, done, doneAt, &pair, &condition, &mutex] {
								static_cast<void>(core::compute_file_diff(input_of(pair), *components));
								*doneAt = clock_type::now();
								{
									const auto lock = std::scoped_lock{ mutex };
									done->store(true, std::memory_order_release);
								}
								condition.notify_all();
							} };
							std::this_thread::sleep_for(delay);
							stoppedAt = clock_type::now();
							flag.store(true, std::memory_order_release);
							{
								auto lock = std::unique_lock{ mutex };
								paused    = !done->load(std::memory_order_acquire);
							}
							returned = clock_type::now();
							// The measurement still waits for the worker, to record
							// how long the abandoned work ran on.
							worker.join();
							remaining = paused ? milliseconds_between(stoppedAt, *doneAt) : 0.0;
						} else {
							auto components = evaluation::make_candidate(name);
							auto stop       = flag_stop{ flag };
							auto worker     = std::thread{ [&] {
								const auto control = core::diff_compute_control{ .Stop = &stop };
								auto       slice   = components == nullptr ? core::compute_file_diff(input_of(pair), control) : core::compute_file_diff(input_of(pair), control, *components);
								returned           = clock_type::now();
								paused             = !slice.Outcome.has_value();
							} };
							std::this_thread::sleep_for(delay);
							stoppedAt = clock_type::now();
							flag.store(true, std::memory_order_release);
							worker.join();
						}
						auto line = json_line{ };
						base_line(line, "stops", name, pair);
						line.add("fraction", fraction)
							.add("uninterrupted_ms", full)
							.add("stopped", paused)
							.add("stop_to_return_ms", paused ? milliseconds_between(stoppedAt, returned) : 0.0)
							.add("abandoned_work_ms", remaining);
						out.write(line);
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------- overhead

	auto overhead(const arguments& aArguments) -> void
	{
		auto       out     = output_file{ aArguments.Out };
		const auto scratch = std::filesystem::path{ aArguments.Out }.parent_path() / "scratch-overhead";
		std::filesystem::create_directories(scratch);
		std::ofstream{ scratch / "one" } << "a\n";
		std::ofstream{ scratch / "two" } << "b\n";
		for (const auto& program : evaluation::git_programs()) {
			auto       store   = blob_store{ program, scratch };
			const auto one     = store.blob((scratch / "one").string());
			const auto two     = store.blob((scratch / "two").string());
			const auto version = evaluation::run_supervised_git(program.Path, { "--version" }, scratch.string(), nullptr).Output.value_or("");
			for (int rep = 0; rep < std::max(aArguments.Reps, 40); ++rep) {
				auto arguments = evaluation::git_diff_arguments(evaluation::git_algorithm::myers, false, { });
				arguments.push_back(one);
				arguments.push_back(two);
				const auto start        = clock_type::now();
				const auto run          = evaluation::run_supervised_git(program.Path, arguments, store.directory(), nullptr);
				const auto diffMs       = milliseconds_since(start);
				const auto versionStart = clock_type::now();
				static_cast<void>(evaluation::run_supervised_git(program.Path, { "--version" }, scratch.string(), nullptr));
				auto line = json_line{ };
				line.add("mode", std::string{ "overhead" })
					.add("program", program.Version)
					.add("version", version)
					.add("rep", static_cast<std::uint64_t>(rep))
					.add("tiny_diff_ms", diffMs)
					.add("version_ms", milliseconds_since(versionStart))
					.add("ok", run.Output.has_value());
				out.write(line);
			}
		}
	}

	// ---------------------------------------------------------------- determinism

	auto determinism(const arguments& aArguments, const std::vector<corpus_pair>& aPairs) -> void
	{
		auto       out      = output_file{ aArguments.Out };
		const auto programs = evaluation::git_programs();
		auto       stores   = std::vector<std::unique_ptr<blob_store>>{ };
		const auto scratch  = std::filesystem::path{ aArguments.Out }.parent_path() / "scratch";
		for (const auto& program : programs) {
			stores.push_back(std::make_unique<blob_store>(program, scratch));
		}
		for (const auto& pair : aPairs) {
			for (const auto& name : aArguments.Candidates) {
				auto digests = std::vector<std::string>{ };
				for (int run = 0; run < 2; ++run) {
					auto summary = result_summary{ };
					if (const auto spec = parse_process(name, programs); spec.has_value()) {
						const auto result = run_process_candidate(*stores[spec->Program], *spec, pair);
						summary           = result.Outcome.has_value() ? summarize(*result.Outcome, pair) : result_summary{ .Reason = "process-failure" };
					} else {
						auto components = evaluation::make_candidate(name);
						summary         = summarize(run_in_process(components.get(), pair).first, pair);
					}
					digests.push_back(summary.Reason + "/" + summary.SegmentsDigest + "/" + summary.PairsDigest + "/" + summary.IntralineDigest);
				}
				auto line = json_line{ };
				base_line(line, "determinism", name, pair);
				line.add("digest", digests[0]).add("repeatable", digests[0] == digests[1]);
				out.write(line);
			}
		}
	}

	// ---------------------------------------------------------------- memory

	[[nodiscard]] auto peak_kilobytes(const int aWho) -> std::uint64_t
	{
		auto usage = rusage{ };
		::getrusage(aWho, &usage);
#if defined(__APPLE__)
		return static_cast<std::uint64_t>(usage.ru_maxrss) / 1024U;
#else
		return static_cast<std::uint64_t>(usage.ru_maxrss);
#endif
	}

	auto single(const arguments& aArguments, const std::vector<corpus_pair>& aPairs) -> int
	{
		if (aPairs.size() != 1U || aArguments.Candidates.size() != 1U) {
			return 2;
		}
		const auto& pair   = aPairs.front();
		const auto& name   = aArguments.Candidates.front();
		const auto  before = peak_kilobytes(RUSAGE_SELF);
		auto        ok     = false;
		if (const auto spec = parse_process(name, evaluation::git_programs()); spec.has_value()) {
			auto       store  = blob_store{ evaluation::git_programs()[spec->Program], std::filesystem::path{ aArguments.Out }.parent_path() / "scratch" };
			const auto result = run_process_candidate(store, *spec, pair);
			ok                = result.Outcome.has_value() && result.Outcome->Result.has_value();
		} else {
			auto components = evaluation::make_candidate(name);
			ok              = run_in_process(components.get(), pair).first.Result.has_value();
		}
		std::printf("{\"self_before_kb\":%llu,\"self_peak_kb\":%llu,\"children_peak_kb\":%llu,\"ok\":%s}\n", static_cast<unsigned long long>(before),
					static_cast<unsigned long long>(peak_kilobytes(RUSAGE_SELF)), static_cast<unsigned long long>(peak_kilobytes(RUSAGE_CHILDREN)), ok ? "true" : "false");
		return 0;
	}

	auto rss(const arguments& aArguments, const std::vector<corpus_pair>& aPairs, const char* aSelf) -> void
	{
		auto out = output_file{ aArguments.Out };
		for (const auto& pair : aPairs) {
			for (const auto& name : aArguments.Candidates) {
				auto argv = std::vector<std::string>{ aSelf, "single", "--corpus", aArguments.Corpus.string(), "--out", aArguments.Out, "--candidates", name, "--pairs", pair.Id };
				auto raw  = std::vector<char*>{ };
				for (auto& value : argv) {
					raw.push_back(value.data());
				}
				raw.push_back(nullptr);
				auto pipe = std::array<int, 2U>{ };
				static_cast<void>(::pipe(pipe.data()));
				auto actions = posix_spawn_file_actions_t{ };
				posix_spawn_file_actions_init(&actions);
				posix_spawn_file_actions_adddup2(&actions, pipe[1], STDOUT_FILENO);
				posix_spawn_file_actions_addclose(&actions, pipe[0]);
				auto child = pid_t{ 0 };
				if (::posix_spawn(&child, aSelf, &actions, nullptr, raw.data(), environ) != 0) {
					posix_spawn_file_actions_destroy(&actions);
					::close(pipe[0]);
					::close(pipe[1]);
					continue;
				}
				posix_spawn_file_actions_destroy(&actions);
				::close(pipe[1]);
				auto text   = std::string{ };
				auto buffer = std::array<char, 4096U>{ };
				for (;;) {
					const auto count = ::read(pipe[0], buffer.data(), buffer.size());
					if (count <= 0) {
						break;
					}
					text.append(buffer.data(), static_cast<std::size_t>(count));
				}
				::close(pipe[0]);
				auto status = 0;
				static_cast<void>(::waitpid(child, &status, 0));
				const auto reported = QJsonDocument::fromJson(QByteArray::fromStdString(text)).object();
				auto       line     = json_line{ };
				base_line(line, "rss", name, pair);
				line.add("self_before_kb", static_cast<std::uint64_t>(reported.value("self_before_kb").toInteger()))
					.add("self_peak_kb", static_cast<std::uint64_t>(reported.value("self_peak_kb").toInteger()))
					.add("children_peak_kb", static_cast<std::uint64_t>(reported.value("children_peak_kb").toInteger()))
					.add("ok", reported.value("ok").toBool());
				out.write(line);
			}
		}
	}
} // namespace

auto main(const int aArgc, char** aArgv) -> int
{
	// The ordinary entry points are the baseline here, whatever the build
	// tree's selection file names.
	::setenv("BPR_DIFF_EVALUATION_CANDIDATE", "baseline", 1);
	const auto options = parse_arguments(aArgc, aArgv);
	if (options.Mode == "overhead") {
		overhead(options);
		return 0;
	}
	const auto pairs = load_corpus(options.Corpus, options.PairFilter);
	if (options.Mode == "single") {
		auto exact = std::vector<corpus_pair>{ };
		for (const auto& pair : pairs) {
			if (pair.Id == options.PairFilter) {
				exact.push_back(pair);
			}
		}
		return single(options, exact);
	}
	if (options.Mode == "measure") {
		measure(options, pairs);
	} else if (options.Mode == "phases") {
		phases(options, pairs);
	} else if (options.Mode == "intraline") {
		intraline(options, pairs);
	} else if (options.Mode == "stops") {
		stops(options, pairs);
	} else if (options.Mode == "determinism") {
		determinism(options, pairs);
	} else if (options.Mode == "rss") {
		rss(options, pairs, aArgv[0]);
	} else {
		std::fprintf(stderr, "unknown mode %s\n", options.Mode.c_str());
		return 2;
	}
	return 0;
}
