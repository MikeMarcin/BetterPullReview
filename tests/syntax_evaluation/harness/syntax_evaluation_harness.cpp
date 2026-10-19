// [[BPR-1442]]. RFC-BPR-0081's evaluation harness. Never shipped.
//
// Each input goes through the unchanged public `packaged_syntax_classifier`,
// once per engine per round. The C++ definition calls
// `refine_cpp_identifiers`; this file defines that function as a shim that
// forwards to the shipped refinement (compiled here as
// `refine_cpp_identifiers_baseline`) or to the candidate (compiled as
// `refine_cpp_identifiers_candidate`). Both therefore receive the same
// KSyntaxHighlighting spans from the same grammar pass, and both use the
// classifier's own limits and status rules. The shim times the refinement and
// measures its peak heap; rounds alternate the engine order.
//
// Usage:
//   harness --root DIR --index FILE [--classes a,b] [--rounds N]
//           [--metrics FILE] [--spans FILE] [--stop-trials K]
//           [--stop-classes a,b] [--engines baseline,candidate] [--seed N]
//           [--maximum-spans N]   (default 262,144, the product's limit)

#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>

#include <tree_sitter/api.h>

#if defined(__APPLE__)
	#include <malloc/malloc.h>
#elif defined(_WIN32)
	#include <malloc.h>
#else
	#include <malloc.h>
#endif
#if !defined(_WIN32)
	#include <sys/resource.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <new>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace bpr::core {
	auto refine_cpp_identifiers_baseline(std::string_view, application::token_span_list&, std::size_t, std::stop_token)
		-> application::syntax_classification_status;
	auto refine_cpp_identifiers_candidate(std::string_view, application::token_span_list&, std::size_t, std::stop_token)
		-> application::syntax_classification_status;
}

namespace {
	namespace app    = bpr::application;
	namespace core   = bpr::core;
	namespace syntax = bpr::adapters::syntax;
	using clock      = std::chrono::steady_clock;

	// ---------------------------------------------------------------------
	// Heap accounting: net bytes allocated while armed, and its peak.
	// ---------------------------------------------------------------------

	[[nodiscard]] auto block_size(void* aMemory) noexcept -> std::size_t
	{
#if defined(__APPLE__)
		return malloc_size(aMemory);
#elif defined(_WIN32)
		return _msize(aMemory);
#else
		return malloc_usable_size(aMemory);
#endif
	}

	struct heap_meter
	{
		std::atomic<bool>         Armed{ false };
		std::atomic<std::int64_t> Current{ 0 };
		std::atomic<std::int64_t> Peak{ 0 };

		void arm() noexcept
		{
			Current.store(0);
			Peak.store(0);
			Armed.store(true);
		}

		void disarm() noexcept { Armed.store(false); }

		void allocated(void* aMemory) noexcept
		{
			if (aMemory == nullptr || !Armed.load(std::memory_order_relaxed)) {
				return;
			}
			const auto now = Current.fetch_add(static_cast<std::int64_t>(block_size(aMemory))) + static_cast<std::int64_t>(block_size(aMemory));
			auto       old = Peak.load();
			while (now > old && !Peak.compare_exchange_weak(old, now)) {
			}
		}

		void released(void* aMemory) noexcept
		{
			if (aMemory != nullptr && Armed.load(std::memory_order_relaxed)) {
				Current.fetch_sub(static_cast<std::int64_t>(block_size(aMemory)));
			}
		}
	};

	auto heap() noexcept -> heap_meter&
	{
		static auto instance = heap_meter{ };
		return instance;
	}

	// tree-sitter's allocations go through counting wrappers around the C
	// allocator. Only the harness sets this; the candidate never does.
	auto ts_counting_malloc(std::size_t aBytes) -> void*
	{
		auto* memory = std::malloc(aBytes);
		heap().allocated(memory);
		return memory;
	}

	auto ts_counting_calloc(std::size_t aCount, std::size_t aBytes) -> void*
	{
		auto* memory = std::calloc(aCount, aBytes);
		heap().allocated(memory);
		return memory;
	}

	auto ts_counting_realloc(void* aMemory, std::size_t aBytes) -> void*
	{
		heap().released(aMemory);
		auto* memory = std::realloc(aMemory, aBytes);
		heap().allocated(memory);
		return memory;
	}

	void ts_counting_free(void* aMemory)
	{
		heap().released(aMemory);
		std::free(aMemory);
	}

	// ---------------------------------------------------------------------
	// The engine selection and the refinement observation.
	// ---------------------------------------------------------------------

	enum class engine : std::uint8_t {
		baseline,
		candidate,
	};

	struct refinement_observation
	{
		engine                    Selected = engine::baseline;
		bool                      Capture  = false;
		app::token_span_list      Grammar;
		bool                      Ran      = false;
		std::int64_t              Nanos    = 0;
		std::int64_t              PeakHeap = 0;
		std::atomic<std::int64_t> StartedAt{ 0 };
	};

	auto observation() -> refinement_observation&
	{
		static auto instance = refinement_observation{ };
		return instance;
	}

	[[nodiscard]] auto now_nanos() -> std::int64_t
	{
		return std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch()).count();
	}
}

void* operator new(std::size_t aBytes)
{
	auto* memory = std::malloc(aBytes == 0U ? 1U : aBytes);
	if (memory == nullptr) {
		throw std::bad_alloc{ };
	}
	heap().allocated(memory);
	return memory;
}

void operator delete(void* aMemory) noexcept
{
	heap().released(aMemory);
	std::free(aMemory);
}

void operator delete(void* aMemory, std::size_t) noexcept
{
	heap().released(aMemory);
	std::free(aMemory);
}

namespace bpr::core {
	auto refine_cpp_identifiers(const std::string_view aText, application::token_span_list& aSpans,
								const std::size_t aMaximumSpans, const std::stop_token aStop) -> application::syntax_classification_status
	{
		auto& seen = observation();
		if (seen.Capture) {
			seen.Grammar = aSpans;
		}
		seen.Ran = true;
		heap().arm();
		const auto begin = now_nanos();
		seen.StartedAt.store(begin);
		const auto status = seen.Selected == engine::baseline
			? refine_cpp_identifiers_baseline(aText, aSpans, aMaximumSpans, aStop)
			: refine_cpp_identifiers_candidate(aText, aSpans, aMaximumSpans, aStop);
		const auto end    = now_nanos();
		heap().disarm();
		seen.Nanos    = end - begin;
		seen.PeakHeap = heap().Peak.load();
		return status;
	}
}

namespace {
	class corpus_blob final : public syntax::blob_source
	{
	public:
		[[nodiscard]] auto blob_bytes(const core::git_blob_oid&) -> std::optional<std::string> override
		{
			return Bytes;
		}

		std::string Bytes;
	};

	struct options
	{
		std::string           Root;
		std::string           Index;
		std::set<std::string> Classes;
		std::set<std::string> StopClasses;
		std::vector<engine>   Engines{ engine::baseline, engine::candidate };
		int                   Rounds       = 3;
		int                   StopTrials   = 0;
		std::uint64_t         Seed         = 1442U;
		std::size_t           MaximumSpans = 262'144U;
		std::string           Metrics;
		std::string           Spans;
	};

	struct corpus_file
	{
		std::string Id;
		std::string Class;
		std::string Path;
		std::string SizeClass;
	};

	[[nodiscard]] auto split(const std::string& aText) -> std::set<std::string>
	{
		auto parts  = std::set<std::string>{ };
		auto stream = std::istringstream{ aText };
		auto item   = std::string{ };
		while (std::getline(stream, item, ',')) {
			if (!item.empty()) {
				parts.insert(item);
			}
		}
		return parts;
	}

	[[nodiscard]] auto parse_options(const int aCount, char** aArguments) -> options
	{
		auto result = options{ };
		for (auto index = 1; index + 1 < aCount; index += 2) {
			const auto key   = std::string_view{ aArguments[index] };
			const auto value = std::string{ aArguments[index + 1] };
			if (key == "--root") {
				result.Root = value;
			} else if (key == "--index") {
				result.Index = value;
			} else if (key == "--classes") {
				result.Classes = split(value);
			} else if (key == "--stop-classes") {
				result.StopClasses = split(value);
			} else if (key == "--rounds") {
				result.Rounds = std::stoi(value);
			} else if (key == "--stop-trials") {
				result.StopTrials = std::stoi(value);
			} else if (key == "--maximum-spans") {
				result.MaximumSpans = std::stoull(value);
			} else if (key == "--seed") {
				result.Seed = std::stoull(value);
			} else if (key == "--metrics") {
				result.Metrics = value;
			} else if (key == "--spans") {
				result.Spans = value;
			} else if (key == "--engines") {
				result.Engines.clear();
				for (const auto& name : split(value)) {
					result.Engines.push_back(name == "baseline" ? engine::baseline : engine::candidate);
				}
			} else {
				throw std::runtime_error{ "unknown option " + std::string{ key } };
			}
		}
		if (result.Root.empty() || result.Index.empty()) {
			throw std::runtime_error{ "--root and --index are required" };
		}
		return result;
	}

	[[nodiscard]] auto read_index(const options& aOptions) -> std::vector<corpus_file>
	{
		auto file = QFile{ QString::fromStdString(aOptions.Index) };
		if (!file.open(QIODevice::ReadOnly)) {
			throw std::runtime_error{ "cannot read the corpus index" };
		}
		const auto document = QJsonDocument::fromJson(file.readAll());
		const auto entries  = document.isArray() ? document.array() : document.object().value(QStringLiteral("files")).toArray();
		auto       files    = std::vector<corpus_file>{ };
		for (const auto& entry : entries) {
			const auto object = entry.toObject();
			auto       item   = corpus_file{
				.Id        = object.value(QStringLiteral("id")).toString().toStdString(),
				.Class     = object.value(QStringLiteral("class")).toString().toStdString(),
				.Path      = object.value(QStringLiteral("path")).toString().toStdString(),
				.SizeClass = object.value(QStringLiteral("size_class")).toString().toStdString(),
			};
			if (aOptions.Classes.empty() || aOptions.Classes.contains(item.Class) || aOptions.Classes.contains(item.SizeClass)) {
				files.push_back(std::move(item));
			}
		}
		return files;
	}

	[[nodiscard]] auto read_bytes(const std::string& aPath) -> std::optional<std::string>
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		if (!stream) {
			return std::nullopt;
		}
		auto contents = std::ostringstream{ };
		contents << stream.rdbuf();
		return contents.str();
	}

	[[nodiscard]] auto status_name(const app::syntax_classification_status aStatus) -> std::string_view
	{
		switch (aStatus) {
		case app::syntax_classification_status::available:      return "available";
		case app::syntax_classification_status::cancelled:      return "cancelled";
		case app::syntax_classification_status::limit_exceeded: return "limit_exceeded";
		case app::syntax_classification_status::unavailable:    return "unavailable";
		}
		return "unknown";
	}

	[[nodiscard]] auto engine_name(const engine aEngine) -> std::string_view
	{
		return aEngine == engine::baseline ? "baseline" : "candidate";
	}

	/// FNV-1a over every span field, so equal digests mean equal spans.
	[[nodiscard]] auto digest(const app::token_span_list& aSpans) -> std::string
	{
		auto hash = std::uint64_t{ 0xcbf29ce484222325ULL };
		auto mix  = [&hash](const std::uint32_t aValue) {
			for (auto shift = 0U; shift < 32U; shift += 8U) {
				hash ^= (aValue >> shift) & 0xFFU;
				hash *= 0x100000001b3ULL;
			}
		};
		for (const auto& span : aSpans) {
			mix(span.Line);
			mix(span.Column);
			mix(span.Length);
			mix(static_cast<std::uint32_t>(span.Role));
		}
		auto text = std::array<char, 17U>{ };
		std::snprintf(text.data(), text.size(), "%016llx", static_cast<unsigned long long>(hash));
		return text.data();
	}

	[[nodiscard]] auto escaped(const std::string_view aText) -> std::string
	{
		auto result = std::string{ };
		for (const auto character : aText) {
			if (character == '"' || character == '\\') {
				result += '\\';
			}
			result += character;
		}
		return result;
	}

	void write_spans(std::ostream& aStream, const corpus_file& aFile, const std::string_view aEngine,
					 const app::syntax_classification_status aStatus, const app::token_span_list& aSpans)
	{
		aStream << "{\"f\":\"" << escaped(aFile.Path) << "\",\"engine\":\"" << aEngine << "\",\"status\":\""
				<< status_name(aStatus) << "\",\"spans\":[";
		auto first = true;
		for (const auto& span : aSpans) {
			aStream << (first ? "" : ",") << '[' << span.Line << ',' << span.Column << ',' << span.Length << ','
					<< static_cast<unsigned>(span.Role) << ']';
			first = false;
		}
		aStream << "]}\n";
	}

	struct run_result
	{
		app::syntax_classification Classification;
		std::int64_t               TotalNanos  = 0;
		std::int64_t               RefineNanos = 0;
		std::int64_t               PeakHeap    = 0;
		bool                       Refined     = false;
	};

	[[nodiscard]] auto run_once(syntax::packaged_syntax_classifier& aClassifier, const app::syntax_classification_request& aRequest,
								const engine aEngine, const std::stop_token aStop, const bool aCapture) -> run_result
	{
		auto& seen    = observation();
		seen.Selected = aEngine;
		seen.Capture  = aCapture;
		seen.Ran      = false;
		seen.Nanos    = 0;
		seen.PeakHeap = 0;
		seen.StartedAt.store(0);
		const auto begin      = now_nanos();
		auto       result     = run_result{ };
		result.Classification = aClassifier.classify(aRequest, aStop);
		result.TotalNanos     = now_nanos() - begin;
		result.RefineNanos    = seen.Nanos;
		result.PeakHeap       = seen.PeakHeap;
		result.Refined        = seen.Ran;
		seen.Capture          = false;
		return result;
	}

	/// Requests a stop at a uniformly random point of the refinement (mode
	/// "refinement") or of the whole call (mode "whole"), and measures how long
	/// the classifier takes to return after it.
	void stop_trials(std::ostream& aStream, syntax::packaged_syntax_classifier& aClassifier,
					 const app::syntax_classification_request& aRequest, const corpus_file& aFile, const engine aEngine,
					 const std::int64_t aRefineNanos, const std::int64_t aTotalNanos, const int aTrials, std::mt19937_64& aRandom)
	{
		for (const auto* mode : { "refinement", "whole" }) {
			const auto refinement = std::string_view{ mode } == "refinement";
			const auto span       = refinement ? aRefineNanos : aTotalNanos;
			if (span <= 0) {
				continue;
			}
			for (auto trial = 0; trial < aTrials; ++trial) {
				const auto delay     = std::uniform_int_distribution<std::int64_t>{ 0, span }(aRandom);
				auto       source    = std::stop_source{ };
				auto       requested = std::atomic<std::int64_t>{ 0 };
				auto       finished  = std::atomic<bool>{ false };
				const auto callStart = now_nanos();
				auto       stopper   = std::thread{ [&] {
					auto origin = callStart;
					if (refinement) {
						while (!finished.load() && observation().StartedAt.load() == 0) {
							std::this_thread::yield();
						}
						origin = observation().StartedAt.load();
						if (origin == 0) {
							return;
						}
					}
					const auto target = origin + delay;
					while (!finished.load() && now_nanos() < target) {
						if (target - now_nanos() > 200'000) {
							std::this_thread::sleep_for(std::chrono::microseconds{ 100 });
						} else {
							std::this_thread::yield();
						}
					}
					if (!finished.load()) {
						requested.store(now_nanos());
						source.request_stop();
					}
				} };
				const auto result = run_once(aClassifier, aRequest, aEngine, source.get_token(), false);
				const auto ended  = now_nanos();
				finished.store(true);
				stopper.join();
				const auto at     = requested.load();
				const auto missed = at == 0 || at > ended;
				aStream << "{\"f\":\"" << escaped(aFile.Path) << "\",\"class\":\"" << aFile.Class << "\",\"size_class\":\""
						<< aFile.SizeClass << "\",\"engine\":\"" << engine_name(aEngine) << "\",\"mode\":\"" << mode
						<< "\",\"delay_ns\":" << delay << ",\"latency_ns\":" << (missed ? 0 : ended - at)
						<< ",\"missed\":" << (missed ? "true" : "false") << ",\"status\":\""
						<< status_name(result.Classification.Status) << "\",\"spans\":" << result.Classification.Spans.size()
						<< ",\"digest\":\"" << digest(result.Classification.Spans) << "\"}\n";
			}
		}
	}

	[[nodiscard]] auto peak_resident_bytes() -> long long
	{
#if defined(_WIN32)
		return 0;
#else
		auto usage = rusage{ };
		getrusage(RUSAGE_SELF, &usage);
	#if defined(__APPLE__)
		return static_cast<long long>(usage.ru_maxrss);
	#else
		return static_cast<long long>(usage.ru_maxrss) * 1024LL;
	#endif
#endif
	}

	auto run(const options& aOptions) -> int
	{
		QStandardPaths::setTestModeEnabled(true);
		ts_set_allocator(ts_counting_malloc, ts_counting_calloc, ts_counting_realloc, ts_counting_free);
		const auto catalog    = syntax::packaged_definition_catalog{ };
		auto       blobs      = corpus_blob{ };
		auto       classifier = syntax::packaged_syntax_classifier{ catalog, blobs };
		auto       metrics    = std::ofstream{ };
		auto       spans      = std::ofstream{ };
		if (!aOptions.Metrics.empty()) {
			metrics.open(aOptions.Metrics, std::ios::binary);
		}
		if (!aOptions.Spans.empty()) {
			spans.open(aOptions.Spans, std::ios::binary);
		}
		auto       random     = std::mt19937_64{ aOptions.Seed };
		const auto files      = read_index(aOptions);
		auto       blobSeed   = std::uint8_t{ 0U };
		auto       classified = std::size_t{ 0U };
		auto       skipped    = std::size_t{ 0U };
		for (std::size_t fileIndex = 0; fileIndex < files.size(); ++fileIndex) {
			const auto& file       = files[fileIndex];
			auto        bytes      = read_bytes((std::filesystem::path{ aOptions.Root } / file.Path).string());
			const auto  definition = catalog.definition_for_path(file.Path);
			if (!bytes || definition.Name != "C++") {
				++skipped;
				metrics << "{\"f\":\"" << escaped(file.Path) << "\",\"skipped\":\"" << (bytes ? definition.Name : std::string{ "unreadable" }) << "\"}\n";
				continue;
			}
			++classified;
			blobs.Bytes        = std::move(*bytes);
			auto key           = std::array<std::uint8_t, 32U>{ };
			key[0]             = ++blobSeed;
			key[1]             = static_cast<std::uint8_t>(fileIndex >> 8U);
			key[2]             = static_cast<std::uint8_t>(fileIndex);
			const auto request = app::syntax_classification_request{
				.Context      = { },
				.Key          = { .Blob = core::git_blob_oid{ core::git_hash_algorithm::sha1, key }, .Definition = definition },
				.MaximumSpans = aOptions.MaximumSpans,
			};
			auto refineMedian = std::map<engine, std::vector<std::int64_t>>{ };
			auto totalMedian  = std::map<engine, std::vector<std::int64_t>>{ };
			for (auto round = 0; round < aOptions.Rounds; ++round) {
				auto order = aOptions.Engines;
				if ((static_cast<std::size_t>(round) + fileIndex) % 2U == 1U) {
					std::reverse(order.begin(), order.end());
				}
				for (const auto selected : order) {
					const auto capture = round == 0 && !aOptions.Spans.empty();
					const auto result  = run_once(classifier, request, selected, { }, capture);
					if (capture) {
						if (selected == order.front()) {
							write_spans(spans, file, "grammar", result.Refined ? app::syntax_classification_status::available : result.Classification.Status, observation().Grammar);
						}
						write_spans(spans, file, engine_name(selected), result.Classification.Status, result.Classification.Spans);
					}
					refineMedian[selected].push_back(result.RefineNanos);
					totalMedian[selected].push_back(result.TotalNanos);
					metrics << "{\"f\":\"" << escaped(file.Path) << "\",\"class\":\"" << file.Class << "\",\"size_class\":\""
							<< file.SizeClass << "\",\"bytes\":" << blobs.Bytes.size() << ",\"engine\":\"" << engine_name(selected)
							<< "\",\"round\":" << round << ",\"status\":\"" << status_name(result.Classification.Status)
							<< "\",\"refined\":" << (result.Refined ? "true" : "false") << ",\"total_ns\":" << result.TotalNanos
							<< ",\"refine_ns\":" << result.RefineNanos << ",\"grammar_ns\":" << (result.TotalNanos - result.RefineNanos)
							<< ",\"peak_heap\":" << result.PeakHeap << ",\"spans\":" << result.Classification.Spans.size()
							<< ",\"digest\":\"" << digest(result.Classification.Spans) << "\"}\n";
				}
			}
			if (aOptions.StopTrials > 0 && (aOptions.StopClasses.contains(file.Class) || aOptions.StopClasses.contains(file.SizeClass))) {
				for (const auto selected : aOptions.Engines) {
					auto& refines = refineMedian[selected];
					auto& totals  = totalMedian[selected];
					std::sort(refines.begin(), refines.end());
					std::sort(totals.begin(), totals.end());
					stop_trials(metrics, classifier, request, file, selected, refines[refines.size() / 2U], totals[totals.size() / 2U], aOptions.StopTrials, random);
				}
			}
			metrics.flush();
		}
		metrics << "{\"summary\":true,\"classified\":" << classified << ",\"skipped\":" << skipped
				<< ",\"peak_resident_bytes\":" << peak_resident_bytes() << "}\n";
		std::cout << "syntax-evaluation-harness:done classified=" << classified << " skipped=" << skipped
				  << " peak_resident_bytes=" << peak_resident_bytes() << '\n';
		return EXIT_SUCCESS;
	}
}

auto main(int aCount, char** aArguments) -> int
try {
	return run(parse_options(aCount, aArguments));
} catch (const std::exception& aError) {
	std::cerr << "syntax-evaluation-harness: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
