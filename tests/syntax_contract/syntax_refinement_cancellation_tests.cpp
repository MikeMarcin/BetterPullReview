// [[BPR-1441]]. A stop that arrives while the C++ identifier refinement is
// running, after the grammar pass has finished, publishes nothing.
//
// The contract test requests its stop before classifying, so the grammar pass
// returns at line 0 and the refinement never runs. Here the stop is requested
// from inside the refinement. This executable replaces the global allocation
// functions to watch the classifying thread: the refinement's first act is to
// allocate one role byte per source byte, and the blob's length is chosen so
// that no earlier allocation has that size (see `role_buffer`). The refinement
// is only entered after the grammar pass has returned `available`, so a stop
// requested at or after that allocation cannot be seen by the grammar pass.
//
// An uncancelled pass counts the allocations the refinement makes. The
// cancelled pass then requests the stop at the middle one, so it lands partway
// through the refinement's work whatever its internal phases are.

#include "../support/test_expectation.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"

#include <QStandardPaths>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <new>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

namespace {
	namespace app    = bpr::application;
	namespace core   = bpr::core;
	namespace syntax = bpr::adapters::syntax;

	using bpr::tests::expect;

	/// The blob length, a multiple of 16 that no geometric growth of a 16-byte
	/// span vector reaches exactly.
	constexpr auto BlobBytes = std::size_t{ 299'952U };

	/// Whether one global `operator new` request is the refinement's role
	/// buffer of `BlobBytes` bytes. libc++ and libstdc++ ask for exactly that.
	/// MSVC's standard library pads any request of 4,096 bytes or more for its
	/// own 32-byte alignment: one pointer plus 31 bytes in Release, two pointers
	/// plus 31 in Debug ([[BPR-1124]] met the same allocator path). The
	/// classifier's own copy of the blob is a string of one more byte, whose
	/// request rounds to `BlobBytes + 16` (libc++, MSVC, then padded) or is
	/// `BlobBytes + 1` (libstdc++), so it never matches.
	[[nodiscard]] constexpr auto role_buffer(const std::size_t aBytes) noexcept -> bool
	{
		return aBytes == BlobBytes || aBytes == BlobBytes + sizeof(void*) + 31U || aBytes == BlobBytes + 2U * sizeof(void*) + 31U;
	}

	/// Watches the allocations of the thread that armed it, which is the
	/// classifying thread. Plain atomics: the allocation functions may run
	/// before `main` and on any thread.
	struct allocation_probe
	{
		std::atomic<bool>              Armed{ false };
		std::atomic<bool>              Refining{ false };
		std::atomic<std::size_t>       Allocations{ 0U };
		std::atomic<std::size_t>       Bytes{ 0U };
		std::atomic<std::size_t>       StopAt{ 0U };
		std::atomic<std::stop_source*> Stopper{ nullptr };
		std::atomic<bool>              Stopped{ false };
		std::atomic<std::thread::id>   Thread{ };

		void arm(const std::size_t aStopAt, std::stop_source* aStopper) noexcept
		{
			Thread.store(std::this_thread::get_id());
			Refining.store(false);
			Allocations.store(0U);
			Bytes.store(0U);
			Stopped.store(false);
			StopAt.store(aStopAt);
			Stopper.store(aStopper);
			Armed.store(true);
		}

		void disarm() noexcept
		{
			Armed.store(false);
			Stopper.store(nullptr);
		}

		void observe(const std::size_t aBytes) noexcept
		{
			if (!Armed.load(std::memory_order_relaxed) || std::this_thread::get_id() != Thread.load()) {
				return;
			}
			if (!Refining.load() && role_buffer(aBytes)) {
				Refining.store(true);
			}
			if (!Refining.load()) {
				return;
			}
			Bytes.fetch_add(aBytes);
			const auto count = Allocations.fetch_add(1U) + 1U;
			auto*      stop  = Stopper.load();
			if (stop != nullptr && count == StopAt.load()) {
				Stopped.store(true);
				stop->request_stop();
			}
		}
	};

	auto probe() noexcept -> allocation_probe&
	{
		static auto instance = allocation_probe{ };
		return instance;
	}

	[[nodiscard]] auto allocate(const std::size_t aBytes) -> void*
	{
		probe().observe(aBytes);
		if (auto* memory = std::malloc(aBytes == 0U ? 1U : aBytes)) {
			return memory;
		}
		throw std::bad_alloc{ };
	}

	/// Serves one blob for the identity the test invents.
	class single_blob final : public syntax::blob_source
	{
	public:
		explicit single_blob(std::string aBytes)
			: Bytes{ std::move(aBytes) }
		{
		}

		[[nodiscard]] auto blob_bytes(const core::git_blob_oid&) -> std::optional<std::string> override
		{
			return Bytes;
		}

	private:
		std::string Bytes;
	};

	/// Many namespaces, enums and qualified uses, so the refinement has work in
	/// every phase. Padded with a trailing comment to exactly `BlobBytes`.
	[[nodiscard]] auto busy_source() -> std::string
	{
		auto text  = std::string{ };
		auto index = 0U;
		while (text.size() + 512U < BlobBytes) {
			text += std::format(
				"namespace Outer{0}::Inner {{\n"
				"enum class Reason{0} {{ Node = 9, ID = 1, Player = 64, DNA }};\n"
				"enum Plain{0} {{ FIRST = ENUM_VALUE, SECOND = call(ENUM_VALUE, (1 + 2)), LAST }};\n"
				"auto chosen{0} = Reason{0}::ID;\n"
				"void use{0}(int Reason) {{ auto hidden = Reason::ID; GCT_ASSERT(hidden); }}\n"
				"}}\n"
				"auto qualified{0} = Outer{0}::Inner::Reason{0}::DNA;\n",
				index++);
		}
		text += "//";
		text.append(BlobBytes - text.size() - 1U, ' ');
		text += '\n';
		return text;
	}

	[[nodiscard]] auto blob() -> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes[0]   = 0x41U;
		bytes[19]  = 0x1BU;
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	void refinement_stop_contract()
	{
		const auto text = busy_source();
		expect(text.size() == BlobBytes, "the blob has the probe's signature length");
		const auto catalog    = syntax::packaged_definition_catalog{ };
		auto       blobs      = single_blob{ text };
		auto       classifier = syntax::packaged_syntax_classifier{ catalog, blobs };
		const auto request    = app::syntax_classification_request{
			.Context      = { },
			.Key          = { .Blob = blob(), .Definition = catalog.definition_for_path("busy.cpp") },
			.MaximumSpans = 1'000'000U,
		};
		expect(request.Key.Definition.Name == "C++", "the blob is classified by the C++ definition");

		// Control: the same blob without a stop is available with spans, and the
		// probe saw the refinement begin.
		probe().arm(0U, nullptr);
		const auto whole = classifier.classify(request, { });
		probe().disarm();
		const auto total = probe().Allocations.load();
		expect(whole.Status == app::syntax_classification_status::available && !whole.Spans.empty(),
			   "the uncancelled blob is available with spans");
		expect(probe().Refining.load(), "the refinement's role buffer identifies the refinement");
		expect(total >= 8U, "the refinement allocates often enough to be stopped partway");

		// The stop arrives at the middle allocation of the refinement.
		auto stop = std::stop_source{ };
		probe().arm(total / 2U, &stop);
		const auto cancelled = classifier.classify(request, stop.get_token());
		probe().disarm();
		expect(probe().Stopped.load(), "the stop was requested while the refinement was running");
		expect(cancelled.Status == app::syntax_classification_status::cancelled,
			   "a stop during the refinement is reported as cancelled");
		expect(cancelled.Spans.empty(), "a stop during the refinement publishes no spans");

		// The classifier is reusable after a stop: the next request is answered
		// exactly as before.
		const auto again = classifier.classify(request, { });
		expect(again.Status == whole.Status && again.Spans == whole.Spans, "the next request is answered as before");
		std::cout << "refinement-stop-contract:pass allocations=" << total << " stop_at=" << total / 2U << '\n';
	}

	/// [[BPR-1474]], RFC-BPR-0081's H8: a run of `{` held a block scope per byte,
	/// so 8 MiB of them took about 2 GB. Deep blocks now share their parent's
	/// scope, and every byte the refinement requests, freed or not, stays within
	/// a small multiple of the source: the tokens and the brace stacks.
	void brace_flood_contract()
	{
		auto text = std::string{ };
		while (text.size() + 64U < BlobBytes) {
			text.append(63U, '{');
			text += '\n';
		}
		text.append(BlobBytes - text.size() - 1U, '{');
		text += '\n';
		const auto catalog    = syntax::packaged_definition_catalog{ };
		auto       blobs      = single_blob{ text };
		auto       classifier = syntax::packaged_syntax_classifier{ catalog, blobs };
		const auto request    = app::syntax_classification_request{
			.Context      = { },
			.Key          = { .Blob = blob(), .Definition = catalog.definition_for_path("flood.cpp") },
			.MaximumSpans = 1'000'000U,
		};
		probe().arm(0U, nullptr);
		const auto result = classifier.classify(request, { });
		probe().disarm();
		const auto bytes = probe().Bytes.load();
		expect(result.Status == app::syntax_classification_status::available, "the brace flood classifies");
		expect(probe().Refining.load(), "the refinement ran on the brace flood");
		expect(bytes < 128U * BlobBytes, "the refinement requests fewer than 128 bytes per source byte on a brace flood");
		std::cout << "brace-flood-contract:pass bytes_per_source_byte=" << bytes / BlobBytes << '\n';
	}
}

// The replaceable global allocation functions. The array and no-throw forms
// call these; the aligned forms keep the library's own pairing.
// SDK declarations use vendor parameter names; the replacements keep the project names.
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
auto operator new(const std::size_t aBytes) -> void*
{
	return allocate(aBytes);
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete(void* aMemory) noexcept
{
	std::free(aMemory);
}

// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
void operator delete(void* aMemory, std::size_t) noexcept
{
	std::free(aMemory);
}

auto main() -> int
try {
	QStandardPaths::setTestModeEnabled(true);
	refinement_stop_contract();
	brace_flood_contract();
	std::cout << "syntax-refinement-cancellation-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
