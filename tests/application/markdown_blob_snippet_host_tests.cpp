// [[BPR-1802]]. The permalink snippet host: only the accepted repository on
// its host is read, each file once, a held file answering later readers
// through a posted task, failures remembered, a late answer for a replaced
// scope or a reset dropped, and the bounds kept.

#include "bpr/application/markdown_blob_snippet_host.hpp"

#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class manual_executor final : public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }

		void run()
		{
			while (!Tasks.empty()) {
				auto task = std::move(Tasks.front());
				Tasks.pop_front();
				task();
			}
		}

		std::deque<std::function<void()>> Tasks;
	};

	/// Holds every read until the test answers it.
	class fake_source final : public application::markdown_blob_source_port
	{
	public:
		struct pending
		{
			application::markdown_blob_source_read                         Read;
			std::stop_token                                                Stop;
			std::function<void(application::markdown_blob_source_result)> Completion;
		};

		auto read(const application::markdown_blob_source_read& aRead, std::stop_token aStop, std::function<void(application::markdown_blob_source_result)> aCompletion) -> bool override
		{
			if (Refuse) {
				return false;
			}
			Pending.push_back({ .Read = aRead, .Stop = std::move(aStop), .Completion = std::move(aCompletion) });
			return true;
		}

		void answer(const std::size_t aIndex, application::markdown_blob_source_status aStatus, std::string aBytes = { })
		{
			auto entry = std::move(Pending.at(aIndex));
			Pending.erase(Pending.begin() + static_cast<std::ptrdiff_t>(aIndex));
			auto result   = application::markdown_blob_source_result{ };
			result.Status = aStatus;
			result.Bytes  = std::move(aBytes);
			entry.Completion(std::move(result));
		}

		std::vector<pending> Pending;
		bool                 Refuse = false;
	};

	const std::string commit = "0123456789abcdef0123456789abcdef01234567";

	auto permalink(const std::string& aRepository, const std::string& aPath, const std::string& aAnchor, const std::string& aHost = "github.com") -> core::markdown_blob_permalink
	{
		const auto found = core::recognize_markdown_blob_permalink("https://" + aHost + "/" + aRepository + "/blob/" + commit + "/" + aPath + "#" + aAnchor);
		expect(found.has_value(), "fixture permalink");
		return *found;
	}

	auto request(const core::markdown_blob_permalink& aPermalink) -> application::markdown_blob_snippet_request
	{
		auto made      = application::markdown_blob_snippet_request{ };
		made.Permalink = aPermalink;
		return made;
	}
} // namespace

int main()
{
	auto decisions = manual_executor{ };
	auto source    = fake_source{ };
	auto scope     = std::optional<application::markdown_blob_snippet_scope>{ };
	scope.emplace();
	scope->Host       = "github.com";
	scope->Owner      = "octo-org";
	scope->Repository = "widgets";
	scope->Identity   = "pr-1@head-a";
	auto published    = std::vector<application::markdown_blob_snippet_publication>{ };
	auto host         = application::markdown_blob_snippet_host{
        source, decisions, [&scope] { return scope; },
        [&published](const application::markdown_blob_snippet_publication& aPublication) { published.push_back(aPublication); },
        application::markdown_blob_snippet_limits{ .Concurrent = 1U, .Waiting = 2U, .CachedBytes = 64U }
	};
	const auto file    = std::string{ "one\ntwo\nthree\nfour\n" };
	const auto twoTo3  = permalink("octo-org/widgets", "src/frame.cpp", "L2-L3");
	const auto line4   = permalink("Octo-Org/Widgets", "src/frame.cpp", "L4");
	const auto another = permalink("octo-org/gadgets", "src/frame.cpp", "L2");
	const auto ghes    = permalink("octo-org/widgets", "src/frame.cpp", "L2", "ghe.example.invalid");

	// Out of scope: another repository, another host, an over-bound range.
	auto huge     = twoTo3;
	huge.LastLine = huge.FirstLine + core::markdown_blob_snippet_maximum_lines;
	host.request({ request(another), request(ghes), request(huge) });
	expect(source.Pending.empty() && host.refused() == 3U, "nothing is read out of scope");

	// Two ranges of one file share one read; the answer publishes both.
	host.request({ request(twoTo3), request(line4), request(twoTo3) });
	expect(source.Pending.size() == 1U && source.Pending[0].Read.Identity == "pr-1@head-a" && source.Pending[0].Read.Permalink.Path == "src/frame.cpp", "one read for one file");
	source.answer(0U, application::markdown_blob_source_status::read, file);
	expect(published.size() == 2U && *published[0].Lines == std::vector<std::string>{ "two", "three" } && *published[1].Lines == std::vector<std::string>{ "four" }, "each range gets its lines");
	expect(core::markdown_blob_permalink_same_lines(published[1].Permalink, line4), "the answer names the permalink asked");

	// A held file answers through a posted task, never inline.
	published.clear();
	host.request({ request(line4) });
	expect(published.empty() && source.Pending.empty() && host.cache_hits() == 1U, "a held file answers later, without a read");
	decisions.run();
	expect(published.size() == 1U, "the posted answer arrives");

	// A range past the end gives no box, and nothing more is read.
	published.clear();
	host.request({ request(permalink("octo-org/widgets", "src/frame.cpp", "L4-L6")) });
	decisions.run();
	expect(published.empty() && source.Pending.empty(), "a range past the end publishes nothing");

	// An absent file is remembered; an unavailable one is asked again.
	const auto missing = permalink("octo-org/widgets", "gone.cpp", "L1");
	host.request({ request(missing) });
	source.answer(0U, application::markdown_blob_source_status::absent);
	host.request({ request(missing) });
	expect(source.Pending.empty(), "an absent file is not read twice");
	const auto later = permalink("octo-org/widgets", "later.cpp", "L1");
	host.request({ request(later) });
	source.answer(0U, application::markdown_blob_source_status::unavailable);
	host.request({ request(later) });
	expect(source.Pending.size() == 1U, "an unavailable file is asked again");
	source.answer(0U, application::markdown_blob_source_status::read, "x\n");
	expect(!published.empty() && *published.back().Lines == std::vector<std::string>{ "x" }, "the second read answers");

	// A refused start remembers nothing.
	source.Refuse = true;
	host.request({ request(permalink("octo-org/widgets", "refused.cpp", "L1")) });
	expect(source.Pending.empty() && host.waiting() == 0U, "a read that cannot start is dropped");
	source.Refuse = false;

	// Concurrency one, two waiting: the oldest waiting gives way.
	host.request({ request(permalink("octo-org/widgets", "a.cpp", "L1")), request(permalink("octo-org/widgets", "b.cpp", "L1")), request(permalink("octo-org/widgets", "c.cpp", "L1")),
				   request(permalink("octo-org/widgets", "d.cpp", "L1")) });
	expect(source.Pending.size() == 1U && host.waiting() == 2U, "one read at once, the waiting bounded");

	// A new scope cancels: the late answer is dropped, nothing waits.
	published.clear();
	scope->Identity = "pr-1@head-b";
	host.request({ });
	expect(source.Pending[0].Stop.stop_requested() && host.waiting() == 0U, "a replaced scope stops its reads");
	source.answer(0U, application::markdown_blob_source_status::read, file);
	expect(published.empty(), "a late answer for a replaced scope is dropped");
	host.request({ request(line4) });
	expect(source.Pending.size() == 1U, "the new scope holds nothing from the old one");
	source.answer(0U, application::markdown_blob_source_status::read, file);
	expect(published.size() == 1U, "and reads again");

	// The cache keeps the newest files within its bytes.
	host.request({ request(permalink("octo-org/widgets", "big.cpp", "L1")) });
	source.answer(0U, application::markdown_blob_source_status::read, std::string(60U, 'b'));
	expect(host.cached_bytes() <= 64U, "the cache stays within its bound");

	// No scope: refused. Reset forgets; shutdown publishes nothing more.
	scope.reset();
	const auto before = host.refused();
	host.request({ request(line4) });
	expect(host.refused() == before + 1U && source.Pending.empty(), "no accepted pull request reads nothing");
	scope.emplace();
	scope->Host       = "github.com";
	scope->Owner      = "octo-org";
	scope->Repository = "widgets";
	scope->Identity   = "pr-1@head-b";
	host.request({ request(permalink("octo-org/widgets", "z.cpp", "L1")) });
	host.reset();
	expect(source.Pending[0].Stop.stop_requested(), "reset stops its reads");
	host.request({ request(permalink("octo-org/widgets", "z.cpp", "L1")) });
	host.shutdown();
	published.clear();
	while (!source.Pending.empty()) {
		source.answer(0U, application::markdown_blob_source_status::read, "z\n");
	}
	decisions.run();
	expect(published.empty() && host.is_quiescent(), "after shutdown nothing publishes and the host is quiescent");
	return EXIT_SUCCESS;
}
