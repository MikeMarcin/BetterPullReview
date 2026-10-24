/// \file
/// \brief [[BPR-1753]], owner decision (b) 2026-10-23 ("Continuous
/// session per PR with an option to clear session"). The conversation host on
/// manual lanes with a memory store: a pull request's kept turns come back
/// when it is opened, a turn that ends is kept, Clear session keeps none, a
/// read that ends after another pull request opened is dropped, writes go one
/// at a time with only the newest waiting, and the thread is bounded.

#include "bpr/application/agent_conversation_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using turns = std::vector<application::agent_conversation_turn>;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class memory_store final : public application::agent_conversation_store_port
	{
	public:
		[[nodiscard]] auto load(const application::agent_conversation_key& aKey) -> std::optional<turns> override
		{
			const auto found = Kept.find(aKey);
			return found == Kept.end() ? turns{ } : found->second;
		}
		[[nodiscard]] auto store(const application::agent_conversation_key& aKey, const turns& aTurns) -> bool override
		{
			++Writes;
			Kept[aKey] = aTurns;
			return true;
		}

		[[nodiscard]] auto load_agent(const application::agent_conversation_key& aKey) -> std::optional<std::uint32_t> override
		{
			auto pull        = aKey;
			pull.Agent       = 0U;
			const auto found = Chosen.find(pull);
			return found == Chosen.end() ? std::nullopt : std::optional{ found->second };
		}
		[[nodiscard]] auto store_agent(const application::agent_conversation_key& aKey) -> bool override
		{
			auto pull    = aKey;
			pull.Agent   = 0U;
			Chosen[pull] = aKey.Agent;
			return true;
		}
		[[nodiscard]] auto forget_agents() -> bool override
		{
			Chosen.clear();
			return true;
		}

		std::map<application::agent_conversation_key, turns>         Kept;
		std::map<application::agent_conversation_key, std::uint32_t> Chosen;
		int                                                          Writes = 0;
	};

	struct fixture
	{
		application::manual_executor                            Effects;
		application::manual_executor                            Decision;
		memory_store                                            Store;
		std::vector<application::agent_conversation_projection> Published;
		std::unique_ptr<application::agent_conversation_host>   Host;

		fixture()
		{
			auto dependencies     = application::agent_conversation_dependencies{ };
			dependencies.Store    = &Store;
			dependencies.Effects  = &Effects;
			dependencies.Decision = &Decision;
			dependencies.Publish  = [this](const application::agent_conversation_projection& aView) { Published.push_back(aView); };
			Host                  = std::make_unique<application::agent_conversation_host>(std::move(dependencies));
		}

		void settle()
		{
			for (auto turn = 0; turn < 6; ++turn) {
				Effects.drain();
				Decision.drain();
			}
		}
	};

	[[nodiscard]] auto seven() -> application::agent_conversation_key { return application::agent_conversation_key{ .Host = "github.com", .Owner = "MikeMarcin", .Name = "BPR-Test", .Number = 7U }; }
	[[nodiscard]] auto eight() -> application::agent_conversation_key { return application::agent_conversation_key{ .Host = "github.com", .Owner = "MikeMarcin", .Name = "BPR-Test", .Number = 8U }; }

	void test_a_kept_conversation_comes_back()
	{
		auto test                = fixture{ };
		test.Store.Kept[seven()] = turns{ { .Asked = "Does this work?", .Answered = "Yes.", .Ending = "Claude Code answered in 0:12." } };
		test.Host->open(seven());
		expect(test.Published.size() == 1U && test.Published.back().Key == std::optional{ seven() } && !test.Published.back().Loaded && test.Published.back().Turns.empty(),
			   "opening a pull request shows an empty thread while its turns are read");
		test.settle();
		expect(test.Published.back().Loaded && test.Published.back().Turns == test.Store.Kept[seven()], "then the turns it kept, in order");
		const auto count = test.Published.size();
		test.Host->open(seven());
		expect(test.Published.size() == count, "opening the open pull request again changes nothing");
	}

	void test_turns_are_kept_and_cleared()
	{
		auto test = fixture{ };
		test.Host->open(seven());
		test.settle();
		const auto thread = turns{ { .Asked = "Review this pull request", .Answered = "Nothing to report: the loop is bounded.", .Ending = "answered" } };
		test.Host->record(thread);
		test.settle();
		expect(test.Store.Kept[seven()] == thread && test.Host->projection().Turns == thread, "a turn that ends is kept for its pull request");
		test.Host->clear();
		test.settle();
		expect(test.Store.Kept[seven()].empty() && test.Host->projection().Turns.empty(), "Clear session keeps none");
		test.Host->close();
		test.Host->record(thread);
		test.settle();
		expect(test.Store.Kept[seven()].empty() && !test.Host->projection().Key, "with no pull request open nothing is kept");
	}

	void test_a_late_read_is_dropped()
	{
		auto test                = fixture{ };
		test.Store.Kept[seven()] = turns{ { .Asked = "Seven?", .Answered = "Seven." } };
		test.Host->open(seven());
		test.Host->open(eight());
		test.settle();
		expect(test.Host->projection().Key == std::optional{ eight() } && test.Host->projection().Turns.empty() && test.Host->projection().Loaded,
			   "the first pull request's read, ending after the second opened, is not shown for it");
	}

	void test_one_write_at_a_time()
	{
		auto test = fixture{ };
		test.Host->open(seven());
		test.settle();
		auto first  = turns{ { .Asked = "One" } };
		auto second = turns{ { .Asked = "One" }, { .Asked = "Two" } };
		auto third  = turns{ { .Asked = "One" }, { .Asked = "Two" }, { .Asked = "Three" } };
		test.Host->record(first);
		test.Host->record(second);
		test.Host->record(third);
		test.settle();
		expect(test.Store.Writes == 2 && test.Store.Kept[seven()] == third, "a write waits for the one in flight, and only the newest thread waiting is written");
	}

	/// Owner decision 2026-10-23 ("separate session for each agent makes the
	/// most sense to me"). Each agent has its own conversation about a pull
	/// request; switching shows the other's and keeps it as the one asked
	/// with; clearing one leaves the other.
	void test_one_conversation_per_agent()
	{
		auto test               = fixture{ };
		auto claude             = seven();
		claude.Agent            = 1U;
		auto codex              = seven();
		codex.Agent             = 2U;
		test.Store.Kept[claude] = turns{ { .Asked = "Claude?", .Answered = "Claude." } };
		test.Store.Kept[codex]  = turns{ { .Asked = "Codex?", .Answered = "Codex." } };
		test.Host->open(claude);
		test.settle();
		expect(test.Host->projection().Key == std::optional{ claude } && test.Host->projection().Turns == test.Store.Kept[claude], "the pull request opens on its agent's own thread");
		test.Host->switch_agent(2U);
		test.settle();
		expect(test.Host->projection().Key == std::optional{ codex } && test.Host->projection().Turns == test.Store.Kept[codex], "switching shows the other agent's own thread");
		test.Host->clear();
		test.settle();
		expect(test.Store.Kept[codex].empty() && test.Store.Kept[claude].size() == 1U, "Clear session clears only the current agent's");
		test.Host->switch_agent(1U);
		test.settle();
		expect(test.Host->projection().Turns == test.Store.Kept[claude], "switching back resumes where it left off");
		test.Host->close();
		test.Host->open(codex);
		test.settle();
		expect(test.Host->projection().Key == std::optional{ claude }, "reopened, the pull request is on the agent it was last asked with");
		// [[BPR-1755]]: Reset agents forgets that choice, so the next
		// opening is on the agent asked for; every turn stays.
		test.Host->forget_agent_choices();
		test.settle();
		test.Host->close();
		test.Host->open(codex);
		test.settle();
		expect(test.Store.Chosen.empty() && test.Host->projection().Key == std::optional{ codex } && test.Store.Kept[claude].size() == 1U,
			   "after Reset agents a pull request opens on the agent asked for, and its turns are kept");
	}

	void test_the_thread_is_bounded()
	{
		auto many = turns(application::maximum_conversation_turns + 3U);
		for (auto index = std::size_t{ 0U }; index < many.size(); ++index) {
			many[index].Asked = std::to_string(index);
		}
		many.back().Answered = std::string(application::maximum_conversation_text_bytes - 1U, 'a') + "\xC3\xA9";
		const auto bounded   = application::bound_conversation(many);
		expect(bounded.size() == application::maximum_conversation_turns && bounded.front().Asked == "3" && bounded.back().Answered.size() == application::maximum_conversation_text_bytes - 1U
				   && application::valid(bounded),
			   "the newest turns are kept, and a long answer is cut at a character boundary");
	}
} // namespace

auto main() -> int
{
	test_a_kept_conversation_comes_back();
	test_turns_are_kept_and_cleared();
	test_a_late_read_is_dropped();
	test_one_write_at_a_time();
	test_one_conversation_per_agent();
	test_the_thread_is_bounded();
	std::cout << "agent-conversation-host-tests:pass\n";
	return EXIT_SUCCESS;
}
