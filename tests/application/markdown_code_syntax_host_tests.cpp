// [[BPR-1562]]. The reader's code classification host: one text at a time,
// each text and definition once, the cache answering a second reader, plain
// and oversized text never classified, a failure ending only its own text,
// and shutdown leaving nothing to publish.
// [[BPR-2351]]. A block anchored to a reviewed file borrows that file's
// names when the composition holds them, keyed by the file, with no fetch.

#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/core/markdown_code.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
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
			if (Plain) {
				return answer;
			}
			auto line = std::uint32_t{ 0U };
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
		/// Answers no span: every name is left plain, as a grammar leaves a
		/// snake_case type it cannot see declared.
		bool Plain = false;
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
	auto anchored(std::string aSelector, std::string aText, std::string aAnchor) -> application::markdown_code_syntax_request
	{
		auto asked   = request(std::move(aSelector), std::move(aText));
		asked.Anchor = std::move(aAnchor);
		return asked;
	}

	/// \brief A file's context, as the composition learns it from the panes.
	auto context_of(const std::uint8_t aBlob, std::string aDefinition, const core::appearance_role aRole) -> application::markdown_code_context
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aBlob);
		const auto file = application::token_span_cache_key{ .Blob = core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes }, .Definition = { .Name = std::move(aDefinition), .Version = 1U } };
		return { .File = file, .Roles = std::make_shared<const core::code_context_roles>(core::code_context_roles{ .Names = { { .Name = "frame_size", .Role = aRole } } }) };
	}

	auto only_role(const application::markdown_code_syntax_publication& aPublication) -> std::optional<core::appearance_role>
	{
		if (!aPublication.Spans || aPublication.Spans->size() != 1U) {
			return std::nullopt;
		}
		return aPublication.Spans->front().Role;
	}

	void borrows_file_context()
	{
		auto texts       = application::markdown_code_texts{ };
		auto classifier  = fake_classifier{ texts };
		classifier.Plain = true;
		auto effects     = manual_executor{ };
		auto decisions   = manual_executor{ };
		auto published   = std::vector<application::markdown_code_syntax_publication>{ };
		auto host        = application::markdown_code_syntax_host{ classifier, texts, effects, decisions, definition_for,
																   [&](const application::markdown_code_syntax_publication& aPublication) { published.push_back(aPublication); } };
		auto contexts    = std::map<std::string, application::markdown_code_context, std::less<>>{
			{ "src/a.cpp", context_of(1U, "C++", core::appearance_role::syntax_type) },
			{ "src/b.cpp", context_of(2U, "C++", core::appearance_role::syntax_namespace) },
			{ "tools/gen.py", context_of(3U, "Python", core::appearance_role::syntax_type) },
		};
		auto asked = std::vector<std::string>{ };
		host.set_context_source([&](const std::string_view aAnchor) -> std::optional<application::markdown_code_context> {
			asked.emplace_back(aAnchor);
			const auto found = contexts.find(aAnchor);
			return found == contexts.end() ? std::nullopt : std::optional{ found->second };
		});
		const auto drain = [&] {
			while (!effects.Tasks.empty() || !decisions.Tasks.empty()) {
				effects.run();
				decisions.run();
			}
		};
		const auto answer = [&](const std::string& aAnchor) -> const application::markdown_code_syntax_publication* {
			const auto found = std::ranges::find(published, aAnchor, &application::markdown_code_syntax_publication::Anchor);
			return found == published.end() ? nullptr : &*found;
		};

		// One text, four readers: two files, a file in another language, and none.
		constexpr auto block = "frame_size size;";
		host.request({ anchored("code.cpp", block, "src/a.cpp"), anchored("code.cpp", block, "src/b.cpp"), anchored("code.cpp", block, "tools/gen.py"), request("code.cpp", block) });
		drain();
		expect(classifier.Calls == 1U, "one classification of the text, whatever it is anchored to");
		expect(published.size() == 4U, "each anchor is answered: " + std::to_string(published.size()));
		expect(answer("src/a.cpp") && only_role(*answer("src/a.cpp")) == core::appearance_role::syntax_type, "anchored to a file that declares it, the name is a type");
		expect(answer("src/b.cpp") && only_role(*answer("src/b.cpp")) == core::appearance_role::syntax_namespace, "the same block in another file reads with that file's names");
		expect(answer("tools/gen.py") && answer("tools/gen.py")->Spans->empty(), "a file in another language lends nothing");
		expect(answer("") && answer("")->Spans->empty(), "an unanchored block is coloured from its own text alone");
		expect(std::ranges::count(asked, std::string{ }) == 0, "an unanchored block asks for no context");
		expect(host.borrowed() == 2U && host.borrowed_hits() == 0U, "two borrowings computed");

		// The borrowed cache is keyed by the block and the file.
		published.clear();
		host.request({ anchored("code.cpp", block, "src/a.cpp"), anchored("code.cpp", block, "src/b.cpp") });
		expect(published.size() == 2U && effects.Tasks.empty(), "both answered within the request");
		expect(host.borrowed() == 2U && host.borrowed_hits() == 2U, "each from its own file's cached entry");
		expect(only_role(*answer("src/a.cpp")) == core::appearance_role::syntax_type && only_role(*answer("src/b.cpp")) == core::appearance_role::syntax_namespace, "the cache never serves one file's spans to another");

		// A file classified again (a new head) is a new entry.
		contexts["src/a.cpp"] = context_of(4U, "C++", core::appearance_role::syntax_function);
		published.clear();
		host.request({ anchored("code.cpp", block, "src/a.cpp") });
		expect(published.size() == 1U && only_role(published.front()) == core::appearance_role::syntax_function && host.borrowed() == 3U, "a file's new classification is borrowed afresh");

		// No context yet: the block's own spans, nothing fetched; when the
		// file's context arrives the block is answered again from the cache.
		published.clear();
		asked.clear();
		host.request({ anchored("code.cpp", block, "src/c.cpp") });
		expect(published.size() == 1U && published.front().Anchor == "src/c.cpp" && published.front().Spans->empty(), "a file not yet shown lends nothing");
		expect(asked == std::vector<std::string>{ "src/c.cpp" } && effects.Tasks.empty() && classifier.Calls == 1U, "nothing is read or classified for the file");
		contexts["src/c.cpp"] = context_of(5U, "C++", core::appearance_role::syntax_type);
		published.clear();
		host.context_changed("src/c.cpp");
		expect(published.size() == 1U && published.front().Anchor == "src/c.cpp" && only_role(published.front()) == core::appearance_role::syntax_type, "the block is recoloured when its file's names arrive");
		expect(effects.Tasks.empty() && classifier.Calls == 1U, "from the cache, classifying nothing");
		published.clear();
		host.context_changed("src/elsewhere.cpp");
		expect(published.empty(), "another file's arrival answers nothing");

		host.shutdown();
		host.context_changed("src/c.cpp");
		expect(published.empty(), "nothing after shutdown");
	}
} // namespace

int main()
{
	classifies_once();
	bounds_and_failures();
	borrows_file_context();
	std::cout << "markdown-code-syntax-host-tests:pass\n";
	return EXIT_SUCCESS;
}
