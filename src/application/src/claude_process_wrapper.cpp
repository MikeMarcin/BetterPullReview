#include "bpr/application/claude_process_wrapper.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace bpr::application {
	auto describe_wrapper_refusal(const std::string_view aPath, const core::agent_wrapper_refusal aRefusal) -> std::string
	{
		return "Claude process wrapper " + std::string{ aPath } + ": " + std::string{ core::describe(aRefusal) };
	}

	void claude_process_wrapper_source::set(std::string aPath)
	{
		const std::scoped_lock lock{ Mutex };
		Path = std::move(aPath);
	}

	auto claude_process_wrapper_source::current() const -> std::string
	{
		const std::scoped_lock lock{ Mutex };
		return Path;
	}

	wrapped_claude_process::wrapped_claude_process(agent_process_port& aInner, std::shared_ptr<const claude_process_wrapper_source> aSource,
												   agent_wrapper_inspector aInspect)
		: Inner{ aInner }
		, Source{ std::move(aSource) }
		, Inspect{ std::move(aInspect) }
	{ }

	auto wrapped_claude_process::run(const agent_launch_request& aRequest, const int aTimeoutMilliseconds, const agent_output_sink& aSink,
									 std::stop_token aStop) -> agent_run_result
	{
		if (aRequest.Protocol != core::agent_protocol::claude_stream_json || !Source) {
			return Inner.run(aRequest, aTimeoutMilliseconds, aSink, std::move(aStop));
		}
		auto wrapper = Source->current();
		if (wrapper.empty()) {
			return Inner.run(aRequest, aTimeoutMilliseconds, aSink, std::move(aStop));
		}
		auto refusal = Inspect ? Inspect(wrapper) : std::optional{ core::agent_wrapper_refusal::missing };
		if (refusal) {
			return { .Session = aRequest.Session, .Outcome = agent_run_outcome::failed_to_start, .StandardError = describe_wrapper_refusal(wrapper, *refusal) };
		}
		auto wrapped       = aRequest;
		wrapped.Executable = std::move(wrapper);
		return Inner.run(wrapped, aTimeoutMilliseconds, aSink, std::move(aStop));
	}

	void wrapped_claude_process::cancel() noexcept { Inner.cancel(); }

	struct claude_wrapper_editor::implementation final : std::enable_shared_from_this<implementation>
	{
		implementation(workbench_preferences_host& aPreferences, effect_executor& aEffects, decision_executor& aDecision, agent_wrapper_inspector aInspect)
			: Preferences{ aPreferences }
			, Effects{ aEffects }
			, Decision{ aDecision }
			, Inspect{ std::move(aInspect) }
		{ }

		void settle(const std::uint64_t aGeneration, claude_wrapper_answer aAnswer, const answered& aAnswered)
		{
			if (Closing || aGeneration != Generation) {
				return;
			}
			if (!aAnswer.Refusal) {
				aAnswer.Submitted = Preferences.apply_claude_wrapper(aAnswer.Path);
			}
			if (aAnswered) {
				aAnswered(aAnswer);
			}
		}

		workbench_preferences_host& Preferences;
		effect_executor&            Effects;
		decision_executor&          Decision;
		agent_wrapper_inspector     Inspect;
		std::uint64_t               Generation = 0;
		std::atomic_bool            Closing{ false };
		/// Inspections posted and not yet answered on the decision lane.
		std::atomic_size_t Outstanding{ 0U };
	};

	claude_wrapper_editor::claude_wrapper_editor(workbench_preferences_host& aPreferences, effect_executor& aEffects, decision_executor& aDecision,
												 agent_wrapper_inspector aInspect)
		: Implementation{ std::make_shared<implementation>(aPreferences, aEffects, aDecision, std::move(aInspect)) }
	{ }

	claude_wrapper_editor::~claude_wrapper_editor() { shutdown(); }

	void claude_wrapper_editor::choose(std::string aPath, answered aAnswered)
	{
		auto& editor = *Implementation;
		if (editor.Closing) {
			return;
		}
		const auto generation = ++editor.Generation;
		if (aPath.empty()) {
			editor.settle(generation, { .Path = { } }, aAnswered);
			return;
		}
		if (!core::storable_agent_wrapper(aPath)) {
			// Unstorable text is a control character or too long, which read
			// the same under either platform's rules.
			editor.settle(generation, { .Path = aPath, .Refusal = core::inspect_agent_wrapper_text(aPath, core::agent_wrapper_platform::posix) }, aAnswered);
			return;
		}
		++editor.Outstanding;
		try {
			editor.Effects.post([self = Implementation, generation, path = aPath, done = aAnswered]() mutable {
				if (self->Closing) {
					--self->Outstanding;
					return;
				}
				auto refusal = std::optional{ core::agent_wrapper_refusal::missing };
				try {
					if (self->Inspect) {
						refusal = self->Inspect(path);
					}
				} catch (...) { }
				try {
					self->Decision.post([self, generation, answer = claude_wrapper_answer{ .Path = std::move(path), .Refusal = refusal }, reply = std::move(done)]() mutable {
						--self->Outstanding;
						self->settle(generation, std::move(answer), reply);
					});
				} catch (...) {
					--self->Outstanding;
				}
			});
		} catch (...) {
			--editor.Outstanding;
			editor.settle(generation, { .Path = std::move(aPath), .Refusal = core::agent_wrapper_refusal::missing }, aAnswered);
		}
	}

	auto claude_wrapper_editor::is_quiescent() const noexcept -> bool { return Implementation->Outstanding == 0U; }

	void claude_wrapper_editor::shutdown() noexcept { Implementation->Closing = true; }
} // namespace bpr::application
