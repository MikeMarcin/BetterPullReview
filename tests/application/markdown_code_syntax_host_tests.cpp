// [[BPR-1562]]. The reader's code classification host: one text at a time,
// each text and definition once, the cache answering a second reader, plain
// and oversized text never classified, a failure ending only its own text,
// and shutdown leaving nothing to publish.

#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/core/markdown_code.hpp"

#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
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

	class manual_executor final : public application::decision_executor, public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }

		auto run() -> std::size_t
		{
			auto ran = std::size_t{ 0U };
			while (!Tasks.empty()) {
				auto task = std::move(Tasks.front());
				Tasks.pop_front();
				task();
				++ran;
			}
			return ran;
		}

		std::deque<std::function<void()>> Tasks;
	};

	/// Reads the held text the way the composed classifier does, and answers
	/// one keyword span per line; `Fail` answers unavailable instead.
	class fake_classifier final : public application::syntax_classification_port
	{
	public:
		explicit fake_classifier(application::markdown_code_texts& aTexts)
			: Texts{ aTexts }
		{
		}

		auto classify(const application::syntax_classification_request& aRequest, std::stop_token /*aStop*/) -> application::syntax_classification override
		{
			++Calls;
			auto       answer = application::syntax_classification{ .Context = aRequest.Context, .Key = aRequest.Key };
			const auto text   = Texts.bytes(aRequest.Key.Blob);
			if (!text || Fail) {
				answer.Status = application::syntax_classification_status::unavailable;
				return answer;
			}
			Read.push_back(*text);
			answer.Status = application::syntax_classification_status::available;
			auto line     = std::uint32_t{ 0U };
			for (auto at = std::size_t{ 0U }; at <= text->size(); ++line) {
				const auto end = std::min(text->find('\n', at), text->size());
				if (end > at) {
					answer.Spans.push_back({ .Line = line, .Column = 0U, .Length = static_cast<std::uint32_t>(end - at), .Role = core::appearance_role::syntax_keyword });
				}
				at = end + 1U;
			}
			return answer;
		}

		application::markdown_code_texts& Texts;
		std::vector<std::string>          Read;
		std::size_t                       Calls = 0U;
		bool                              Fail  = false;
	};

	auto definition_for(const std::string_view aSelector) -> std::optional<application::syntax_definition_version>
	{
		if (aSelector == "code.cpp") {
			return application::syntax_definition_version{ .Name = "C++", .Version = 1U };
		}
		if (aSelector == "code.hpp") {
			return application::syntax_definition_version{ .Name = "C++", .Version = 1U };
		}
		if (aSelector == "code.py") {
			return application::syntax_definition_version{ .Name = "Python", .Version = 1U };
		}
		return std::nullopt;
	}

	auto request(std::string aSelector, std::string aText) -> application::markdown_code_syntax_request
	{
		return { .Selector = std::move(aSelector), .Text = std::make_shared<const std::string>(std::move(aText)) };
	}

	void classifies_once()
	{
		auto texts      = application::markdown_code_texts{ };
		auto classifier = fake_classifier{ texts };
		auto effects    = manual_executor{ };
		auto decisions  = manual_executor{ };
		auto published  = std::vector<application::markdown_code_syntax_publication>{ };
		auto host       = application::markdown_code_syntax_host{ classifier, texts, effects, decisions, definition_for,
																  [&](const application::markdown_code_syntax_publication& aPublication) { published.push_back(aPublication); } };
		host.request({ request("code.cpp", "int a;\nint b;"), request("code.py", "x = 1"), request("code.cpp", "int a;\nint b;"), request("code.txt", "plain") });
		expect(host.refused() == 1U, "plain text is never classified");
		expect(published.empty(), "nothing arrives before the effect lane runs");
		expect(effects.Tasks.size() == 1U, "one classification at a time");
		while (!effects.Tasks.empty() || !decisions.Tasks.empty()) {
			effects.run();
			decisions.run();
		}
		expect(classifier.Calls == 2U, "each text and definition classified once: " + std::to_string(classifier.Calls));
		expect(published.size() == 2U, "one answer per text and file name");
		expect(published[0].Selector == "code.py", "the newest request goes first");
		expect(published[1].Selector == "code.cpp" && *published[1].Text == "int a;\nint b;" && published[1].Spans && published[1].Spans->size() == 2U, "spans come with their text");

		// A second reader of the same text, by another file name of the same
		// language, is answered from the cache, at once.
		published.clear();
		host.request({ request("code.hpp", "int a;\nint b;") });
		expect(published.size() == 1U && published[0].Selector == "code.hpp", "a cached text answers within the request");
		expect(classifier.Calls == 2U && host.cache_hits() == 1U, "the cache, not the classifier, answered");
		expect(effects.Tasks.empty() && decisions.Tasks.empty(), "a cached answer posts nothing");
		host.shutdown();
		expect(host.is_quiescent(), "quiescent after shutdown");
	}

	void bounds_and_failures()
	{
		auto texts      = application::markdown_code_texts{ };
		auto classifier = fake_classifier{ texts };
		auto effects    = manual_executor{ };
		auto decisions  = manual_executor{ };
		auto published  = std::vector<application::markdown_code_syntax_publication>{ };
		auto host       = application::markdown_code_syntax_host{ classifier, texts, effects, decisions, definition_for,
																  [&](const application::markdown_code_syntax_publication& aPublication) { published.push_back(aPublication); },
																  application::markdown_code_syntax_limits{ .Waiting = 3U, .Cached = 4U } };
		host.request({ request("code.cpp", std::string(core::markdown_code_classified_bytes + 1U, 'x')) });
		expect(host.refused() == 1U && effects.Tasks.empty(), "a block over the limit stays plain");

		auto many = std::vector<application::markdown_code_syntax_request>{ };
		for (auto index = 0; index < 10; ++index) {
			many.push_back(request("code.cpp", "int v" + std::to_string(index) + ";"));
		}
		host.request(many);
		expect(host.waiting() == 2U, "waiting texts are bounded: three kept, the newest in flight");
		classifier.Fail = true;
		effects.run();
		decisions.run();
		expect(published.empty(), "a failure publishes nothing");
		classifier.Fail = false;
		while (!effects.Tasks.empty() || !decisions.Tasks.empty()) {
			effects.run();
			decisions.run();
		}
		expect(published.size() == 2U, "after a failure the waiting texts still run: " + std::to_string(published.size()));
		expect(*published.back().Text == "int v7;", "the oldest waiting gave way");

		// Shutdown with a classification in flight publishes nothing more.
		published.clear();
		host.request({ request("code.py", "late = 1") });
		host.shutdown();
		effects.run();
		decisions.run();
		expect(published.empty(), "nothing is published after shutdown");
		host.request({ request("code.py", "later = 1") });
		expect(effects.Tasks.empty() && host.refused() == 2U, "requests after shutdown are refused");
	}
} // namespace

int main()
{
	classifies_once();
	bounds_and_failures();
	std::cout << "markdown-code-syntax-host-tests:pass\n";
	return EXIT_SUCCESS;
}
