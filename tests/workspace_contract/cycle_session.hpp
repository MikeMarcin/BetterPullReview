#ifndef BPR_TESTS_WORKSPACE_CONTRACT_CYCLE_SESSION_HPP
#define BPR_TESTS_WORKSPACE_CONTRACT_CYCLE_SESSION_HPP

/// \file
/// \brief The production Review Cycles runtime over a course's store, on
/// manual lanes, for the feedback and suggestion contracts.

#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "cycle_course.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::tests::workspace_contract {
	/// Operation identities for the session, apart from the course's own.
	inline auto session_operation(const std::uint16_t aSeed) -> application::cycle_operation_id
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = 0x5E;
		bytes[30]  = static_cast<std::uint8_t>(aSeed >> 8U);
		bytes[31]  = static_cast<std::uint8_t>(aSeed & 0xFFU);
		return application::cycle_operation_id{ bytes };
	}

	/// The production runtime over the course's store, on manual lanes.
	struct session
	{
		application::manual_executor      Lanes;
		application::review_cycle_runtime Runtime;
		std::uint16_t                     NextOperation = 1000;

		session(const std::filesystem::path& aStoreRoot, const core::local_cycle_subject& aSubject)
			: Runtime{ adapters::sqlite::make_review_cycle_store(aStoreRoot, profile), Lanes, Lanes }
		{
			Runtime.select(aSubject);
		}
		session(const session&)                    = delete;
		auto operator=(const session&) -> session& = delete;
		session(session&&)                         = delete;
		auto operator=(session&&) -> session&      = delete;
		~session()
		{
			Runtime.shutdown();
			Lanes.drain();
		}

		template <typename T, typename TStart>
		auto run(TStart aStart) -> application::cycle_store_result<T>
		{
			auto result = std::optional<application::cycle_store_result<T>>{ };
			expect(aStart([&](application::cycle_store_result<T> aValue) { result = std::move(aValue); }), "runtime admits the call");
			Lanes.drain();
			expect(result.has_value(), "completion delivered once");
			return std::move(*result);
		}

		auto state() -> core::review_cycle_state
		{
			const auto loaded = run<std::optional<core::review_cycle_state>>([&](auto aDone) { return Runtime.load_selected(std::move(aDone)); });
			expect(loaded && *loaded, "load selected cycle");
			return **loaded;
		}

		/// Exact retained bytes of one file of one revision: never the workspace.
		auto retained(const std::uint64_t aRevision, const std::string& aPath) -> std::vector<std::uint8_t>
		{
			const auto cycle = state();
			const auto pair  = core::resolve_comparison(cycle, core::full_source_view{ core::cycle_source_revision_id{ aRevision } });
			expect(pair.has_value(), "full source view");
			const auto file = run<application::cycle_file_observation>([&](auto aDone) { return Runtime.compare_file_selected(*pair, aPath, { }, std::move(aDone)); });
			expect(file && file->Status == application::review_material_status::available, "retained file");
			return file->Source;
		}

		auto anchor(const std::uint64_t aRevision, const std::string& aPath, const std::string_view aText) -> core::cycle_source_anchor
		{
			const auto bytes = retained(aRevision, aPath);
			const auto text  = std::string_view{ reinterpret_cast<const char*>(bytes.data()), bytes.size() };
			const auto start = text.find(aText);
			expect(start != std::string_view::npos, "anchor text in retained bytes");
			auto made = core::make_anchor(core::cycle_source_revision_id{ aRevision }, aPath, bytes, start, start + aText.size());
			expect(made.has_value(), "anchor made");
			return std::move(*made);
		}

		auto begin(core::begin_cycle_feedback aCommand) -> application::cycle_store_result<application::cycle_write_receipt>
		{
			const auto expected = state().Revision;
			return begin_as(session_operation(NextOperation++), expected, std::move(aCommand));
		}

		auto begin_as(const application::cycle_operation_id aOperation, const std::uint64_t aExpected, core::begin_cycle_feedback aCommand)
			-> application::cycle_store_result<application::cycle_write_receipt>
		{
			return run<application::cycle_write_receipt>([&](auto aDone) { return Runtime.begin_feedback_selected(aOperation, aExpected, std::move(aCommand), std::move(aDone)); });
		}

		auto change(const core::cycle_feedback_item_ref& aItem, const std::uint64_t aVersion, core::cycle_feedback_change aChange)
			-> application::cycle_store_result<application::cycle_write_receipt>
		{
			return run<application::cycle_write_receipt>([&](auto aDone) { return Runtime.change_feedback_selected(session_operation(NextOperation++), aItem, aVersion, std::move(aChange), std::move(aDone)); });
		}

		auto threads(const std::optional<core::cycle_feedback_item_id> aAfter = std::nullopt) -> application::cycle_feedback_page
		{
			const auto page = run<application::cycle_feedback_page>([&](auto aDone) { return Runtime.feedback_selected(aAfter, std::move(aDone)); });
			expect(page.has_value(), "feedback page");
			return *page;
		}

		auto revisions(const core::cycle_feedback_item_ref& aItem, const std::optional<core::private_review_turn_revision_id> aAfter = std::nullopt)
			-> application::cycle_feedback_revision_page
		{
			const auto page = run<application::cycle_feedback_revision_page>([&](auto aDone) { return Runtime.feedback_revisions_selected(aItem, aAfter, std::move(aDone)); });
			expect(page.has_value(), "revision page");
			return *page;
		}

		auto observe(const core::cycle_feedback_target& aTarget, const std::uint64_t aAgainst) -> application::cycle_correspondence_observation
		{
			const auto observed = run<application::cycle_correspondence_observation>([&](auto aDone) {
				return Runtime.observe_feedback_selected(aTarget, core::cycle_source_revision_id{ aAgainst }, std::move(aDone));
			});
			expect(observed.has_value(), "correspondence observed");
			return *observed;
		}
	};

	/// A round draft of the session's cycle, driven through the runtime.
	struct round_drafts
	{
		session&                    Notes;
		core::cycle_round_draft_ref Draft;
		std::uint64_t               Revision = 1;

		auto save(const core::cycle_round_selection& aSelection) -> application::cycle_write_receipt
		{
			const auto saved = Notes.run<application::cycle_write_receipt>([&](auto aDone) {
				return Notes.Runtime.save_round_selected(session_operation(Notes.NextOperation++), Draft, Revision, aSelection, std::move(aDone));
			});
			expect(saved.has_value(), "draft saved");
			Revision = saved->Revision;
			return *saved;
		}

		auto freeze(const core::cycle_draft_editor_state& aEditor, const std::optional<application::cycle_operation_id> aOperation = std::nullopt)
			-> application::cycle_round_freeze_result
		{
			const auto request = application::cycle_round_freeze_request{
				.Operation             = aOperation.value_or(session_operation(Notes.NextOperation++)),
				.Draft                 = Draft,
				.ExpectedDraftRevision = Revision,
				.Editor                = aEditor,
			};
			const auto result = Notes.run<application::cycle_round_freeze_result>([&](auto aDone) { return Notes.Runtime.freeze_round_selected(request, std::move(aDone)); });
			expect(result.has_value(), "freeze decided");
			return *result;
		}

		auto flushed() const -> core::cycle_draft_editor_state
		{
			return { .ShownRevision = Revision };
		}

		auto read(const std::uint64_t aRound) -> application::cycle_frozen_round
		{
			const auto read = Notes.run<application::cycle_frozen_round>([&](auto aDone) { return Notes.Runtime.read_round_selected(core::cycle_round_id{ aRound }, std::move(aDone)); });
			expect(read.has_value(), "round read");
			return *read;
		}

		auto count() -> std::size_t
		{
			const auto page = Notes.run<application::cycle_round_page>([&](auto aDone) { return Notes.Runtime.rounds_selected(std::nullopt, std::move(aDone)); });
			expect(page.has_value(), "round page");
			return page->Rounds.size();
		}
	};
	/// Begins a round draft on the session's cycle.
	inline auto begin_round(session& aNotes) -> round_drafts
	{
		const auto begun = aNotes.run<application::cycle_write_receipt>([&](auto aDone) { return aNotes.Runtime.begin_round_selected(session_operation(aNotes.NextOperation++), aNotes.state().Revision, std::move(aDone)); });
		expect(begun.has_value(), "round draft begun");
		return { .Notes = aNotes, .Draft = { .Cycle = aNotes.state().Subject.Cycle, .Id = core::cycle_round_draft_id{ begun->Allocated } } };
	}
}

#endif
