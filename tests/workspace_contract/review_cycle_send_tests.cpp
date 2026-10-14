#include "../sqlite_contract/scratch_root.hpp"
#include "product_diff.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_export_destination.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_cycle_runtime.hpp"
#include "bpr/application/review_cycle_send.hpp"
#include "bpr/application/review_cycle_workbench.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/core/review_cycle_send.hpp"
#include "bpr/core/sha256.hpp"
#include "workspace_fixture.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1180]]. Send to Agent through the desktop's cycle host and the
/// real store, with the adapter scripted: the round's channel and the
/// profile's host policy decide first, the adapter receives exactly what Copy
/// gives, what it saw is recorded against the round it was sent, late and
/// uncertain sends keep to their own round and are never sent again, and the
/// cycle carries on by hand once the adapter is gone. The real adapter and
/// processes are `application_review_cycle_send_public_path`'s.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::write_file;

	constexpr auto profile = core::bpr_profile_id{ 6 };

	struct recording_port final : application::review_cycle_workbench_port
	{
		application::review_cycle_workbench_projection Last;
		void                                           publish(const application::review_cycle_workbench_projection& aProjection) override
		{
			Last = aProjection;
		}
		void publish(const std::vector<application::review_cycle_recent>&) override { }
	};

	/// The adapter, scripted: it keeps each assignment and answers as told.
	struct scripted_sender final : application::review_cycle_sender
	{
		std::vector<application::cycle_send_assignment>                                   Received;
		std::function<core::cycle_send_result(const application::cycle_send_assignment&)> Answer;
		std::size_t                                                                       Cancels = 0;
		auto                                                                              send(const application::cycle_send_assignment& aAssignment, const std::stop_token aStop) -> core::cycle_send_result override
		{
			Received.push_back(aAssignment);
			if (aStop.stop_requested()) {
				return { .End = core::cycle_send_end::cancelled };
			}
			return Answer ? Answer(aAssignment) : core::cycle_send_result{ .End = core::cycle_send_end::completed };
		}
		void cancel() noexcept override
		{
			++Cancels;
		}
	};

	struct clipboard_double final : application::review_cycle_clipboard
	{
		std::vector<std::string> Written;
		auto                     write(const std::string_view aText) -> outcome override
		{
			Written.emplace_back(aText);
			return outcome::confirmed;
		}
	};

	/// A host over the profile root on manual lanes; a send runs on its own.
	struct desk
	{
		application::manual_executor                         Lanes;
		application::manual_executor                         SendLane;
		std::unique_ptr<application::workspace_source_port>  Source = adapters::workspace::make_local_workspace_source();
		recording_port                                       Port;
		clipboard_double                                     Clipboard;
		std::shared_ptr<scripted_sender>                     Sender = std::make_shared<scripted_sender>();
		std::optional<std::string>                           AccountHost;
		std::unique_ptr<application::review_cycle_workbench> Host;

		desk(const std::filesystem::path& aProfileRoot, const std::uint8_t aSeed, const bool aAdapter)
		{
			auto seed = std::array<std::uint8_t, 32>{ };
			seed[0]   = aSeed;
			Host      = std::make_unique<application::review_cycle_workbench>(adapters::sqlite::make_review_cycle_store(aProfileRoot, profile), *Source, Lanes, Lanes, Port, seed, nullptr, bpr::tests::product_line_matcher());
			Host->set_clipboard(&Clipboard);
			Host->set_destination_factory([](const std::string& aPath) { return adapters::workspace::make_local_export_destination(std::filesystem::path{ aPath }); });
			if (aAdapter) {
				Host->set_send_environment({ .Profiles = core::built_in_agent_profiles(), .Sender = Sender, .Lane = &SendLane, .AccountHost = [this] { return AccountHost; } });
			}
		}
		desk(const desk&)                    = delete;
		auto operator=(const desk&) -> desk& = delete;
		desk(desk&&)                         = delete;
		auto operator=(desk&&) -> desk&      = delete;
		~desk()
		{
			Host->shutdown();
			SendLane.drain();
			Lanes.drain();
		}
		auto view() -> const application::review_cycle_workbench_projection&
		{
			Lanes.drain();
			return Host->projection();
		}
		auto share() -> const application::review_cycle_share_view&
		{
			return *view().Share;
		}
		/// A new goal, saved as the draft's next revision.
		void goal(std::string aGoal)
		{
			Host->edit_composition(core::set_round_goal{ std::move(aGoal) });
			Lanes.drain();
		}
		/// The adapter runs now; its observations are recorded.
		void deliver()
		{
			while (Lanes.pending() != 0U || SendLane.pending() != 0U) {
				Lanes.drain();
				SendLane.drain();
			}
		}
	};

	template <typename T, typename TStart>
	auto settle(const std::filesystem::path& aProfileRoot, const core::local_cycle_subject& aSubject, TStart aStart) -> T
	{
		application::manual_executor      lanes;
		application::review_cycle_runtime runtime{ adapters::sqlite::make_review_cycle_store(aProfileRoot, profile), lanes, lanes, nullptr, bpr::tests::product_line_matcher() };
		runtime.select(aSubject);
		auto result = std::optional<application::cycle_store_result<T>>{ };
		aStart(runtime, [&](application::cycle_store_result<T> aValue) { result = std::move(aValue); });
		lanes.drain();
		runtime.shutdown();
		lanes.drain();
		expect(result && *result, "the store answered");
		return std::move(**result);
	}

	void allow(const std::filesystem::path& aProfileRoot, const core::local_cycle_subject& aSubject, const core::cycle_channel_permission aPermission, const std::uint8_t aIndex)
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = 0xC1;
		bytes[1]   = aIndex;
		static_cast<void>(settle<application::cycle_write_receipt>(aProfileRoot, aSubject, [&](auto& aRuntime, auto aDone) {
			aRuntime.set_channel_permission(application::cycle_operation_id{ bytes }, core::cycle_data_classification::unclassified, core::cycle_disclosure_channel::agent_adapter, aPermission,
											std::move(aDone));
		}));
	}

	auto deliveries(const std::filesystem::path& aProfileRoot, const core::local_cycle_subject& aSubject) -> std::vector<core::cycle_delivery_record>
	{
		return settle<std::vector<core::cycle_delivery_record>>(aProfileRoot, aSubject, [](auto& aRuntime, auto aDone) { aRuntime.deliveries_selected(std::move(aDone)); });
	}

	auto responses(const std::filesystem::path& aProfileRoot, const core::local_cycle_subject& aSubject) -> application::cycle_response_entries
	{
		return settle<application::cycle_response_entries>(aProfileRoot, aSubject, [](auto& aRuntime, auto aDone) { aRuntime.responses_selected(std::move(aDone)); });
	}

	auto states_of(const std::vector<core::cycle_delivery_record>& aRecords, const std::size_t aFrom) -> std::vector<core::cycle_delivery_state>
	{
		auto states = std::vector<core::cycle_delivery_state>{ };
		for (auto index = aFrom; index < aRecords.size(); ++index) {
			states.push_back(aRecords[index].Observation.State);
		}
		return states;
	}

	auto offer_of(const application::review_cycle_share_view& aShare, const core::agent_profile_id aProfile) -> core::cycle_send_offer
	{
		expect(aShare.Send.has_value(), "Send to Agent is offered");
		const auto found = std::ranges::find(aShare.Send->Offers, aProfile, &core::cycle_send_offer::Profile);
		expect(found != aShare.Send->Offers.end(), "every installed agent is listed");
		return *found;
	}

	auto route_of(const application::review_cycle_share_view& aShare, const core::cycle_share_route aRoute) -> std::optional<core::cycle_share_route_offer>
	{
		const auto found = std::ranges::find(aShare.Routes, aRoute, &core::cycle_share_route_offer::Route);
		return found == aShare.Routes.end() ? std::nullopt : std::optional{ *found };
	}

	auto contains(const std::string& aText, const std::string_view aPart) -> bool
	{
		return aText.find(aPart) != std::string::npos;
	}

	/// A response to the frozen round `aRound` of `aSubject`, as a recipient would write it, in a reply.
	auto reply_for(const std::filesystem::path& aProfileRoot, const core::local_cycle_subject& aSubject, const core::cycle_round_id aRound, const std::string& aResponseId) -> std::string
	{
		const auto frozen   = settle<application::cycle_frozen_round>(aProfileRoot, aSubject, [&](auto& aRuntime, auto aDone) { aRuntime.read_round_selected(aRound, std::move(aDone)); });
		auto       document = core::cycle_response_document{
			.ResponseId    = aResponseId,
			.CycleId       = frozen.Content.Payload.CycleId,
			.RoundId       = frozen.Content.Payload.RoundId,
			.RoundDigest   = frozen.Round.Round.Digest,
			.ClaimedAuthor = { .DisplayName = "Fixture agent", .Kind = core::cycle_claimed_author_kind::agent },
		};
		for (const auto& item : frozen.Content.Payload.Items) {
			document.Entries.push_back({ .Item = item.Id, .ItemRevision = item.Revision, .Disposition = core::cycle_response_disposition::addressed, .Body = "Done." });
		}
		const auto encoded = core::encode_response_document(document);
		expect(encoded.has_value(), "a response document");
		return "I made the change.\n```json\n" + *encoded + "\n```\n";
	}

	struct course
	{
		std::filesystem::path     ProfileRoot;
		std::filesystem::path     Project;
		core::local_cycle_subject Subject;
	};

	/// A cycle with two notes on a folder, on `aDesk`.
	auto start(desk& aDesk, const std::filesystem::path& aProfileRoot, const std::filesystem::path& aProject) -> course
	{
		write_file(aProject / "README.md", "# Sender\n");
		write_file(aProject / "src" / "send.cpp", "int send(int aRound)\n{\n\treturn aRound;\n}\n");
		aDesk.Host->open_project(aProject.string());
		aDesk.Lanes.drain();
		aDesk.Host->comment({ .Primary = core::cycle_whole_target{ }, .Body = "Say what a send claims." });
		aDesk.Lanes.drain();
		aDesk.Host->comment({ .Primary = core::cycle_whole_target{ }, .Body = "Never send twice." });
		expect(aDesk.view().Notes.size() == 2U, "two notes");
		return { .ProfileRoot = aProfileRoot, .Project = aProject, .Subject = aDesk.view().Cycle->Subject };
	}

	const auto claude   = core::cycle_send_request{ .Profile = core::claude_stream_json_profile_id };
	const auto terminal = core::cycle_send_request{ .Profile = core::external_terminal_profile_id };
	const auto codex    = core::cycle_send_request{ .Profile = core::codex_jsonl_profile_id };

	/// RC60-08: the route is there only with an adapter, refused until the
	/// round's channel allows it, and a corporate host's refusal of a profile
	/// stands: nothing is frozen or sent and nothing else is used instead.
	void refuses_before_anything_leaves(const std::filesystem::path& aScratch)
	{
		tests::sqlite_contract::scratch_root profileScratch{ "cycle-send-refusal" };
		auto                                 bare = desk{ profileScratch.path(), 1, false };
		const auto                           run  = start(bare, profileScratch.path(), aScratch / "refusal-project");
		bare.Host->compose();
		bare.Lanes.drain();
		expect(!route_of(bare.share(), core::cycle_share_route::send_to_agent) && !bare.share().Send, "no adapter: no route, not even a refused one");
		bare.Host->close_compose();
		bare.Host->shutdown();

		auto work = desk{ profileScratch.path(), 2, true };
		work.Host->resume(run.Subject);
		work.Lanes.drain();
		work.Host->compose();
		work.Lanes.drain();
		const auto refused = route_of(work.share(), core::cycle_share_route::send_to_agent);
		expect(refused && refused->Decision == core::cycle_authorization::channel_unconfigured, "source of unknown origin: listed, refused until configured");
		work.Host->send(claude);
		expect(contains(work.share().Status, "Nothing configured allows sending to an agent") && work.view().Facts.Rounds.empty() && work.Sender->Received.empty(),
			   "refused: nothing frozen, nothing started");

		allow(run.ProfileRoot, run.Subject, core::cycle_channel_permission::allowed, 1);
		work.AccountHost = "ghe.example.com";
		work.Host->compose();
		work.Lanes.drain();
		expect(route_of(work.share(), core::cycle_share_route::send_to_agent)->offered(), "allowed by the stored policy");
		const auto denied = offer_of(work.share(), core::codex_jsonl_profile_id);
		expect(!denied.offered() && denied.Verdict.Refusal == core::agent_invocation_refusal::profile_forbidden_on_host, "a corporate account: Codex listed, refused");
		expect(offer_of(work.share(), core::claude_stream_json_profile_id).offered() && offer_of(work.share(), core::external_terminal_profile_id).offered(), "the approved agents stay");
		work.Host->send(codex);
		work.deliver();
		expect(contains(work.share().Status, "does not allow Codex (profile_forbidden_on_host)") && contains(work.share().Status, "no other agent or way was used instead"),
			   "the denied GHES profile is refused by name");
		expect(work.view().Facts.Rounds.empty() && work.Sender->Received.empty() && work.Clipboard.Written.empty() && work.share().Deliveries.empty(),
			   "nothing frozen, sent, copied or recorded in its place");

		allow(run.ProfileRoot, run.Subject, core::cycle_channel_permission::denied, 2);
		work.AccountHost.reset();
		work.Host->compose();
		work.Lanes.drain();
		work.Host->send(claude);
		expect(contains(work.share().Status, "does not allow sending to an agent") && work.Sender->Received.empty(), "a denied channel stays denied for every agent");
		std::cout << "NATIVE: send refused by channel and by the corporate host, nothing frozen or sent\n";
	}

	/// RC60-07, RC60-08, RC60-09: the exact round, one assignment, the
	/// response model every channel uses, sessions bound exactly.
	void sends_one_exact_assignment(const std::filesystem::path& aScratch)
	{
		tests::sqlite_contract::scratch_root profileScratch{ "cycle-send-exact" };
		auto                                 work = desk{ profileScratch.path(), 3, true };
		const auto                           run  = start(work, profileScratch.path(), aScratch / "exact-project");
		allow(run.ProfileRoot, run.Subject, core::cycle_channel_permission::allowed, 1);
		const auto revisions = work.view().Revisions.size();

		work.Host->compose();
		work.Lanes.drain();
		const auto preview = work.share().Document;
		expect(work.share().Send && work.share().Send->Folder == run.Project.string() && !work.share().Send->Sending, "the folder it would run in, said before sending");
		work.Sender->Answer = [&](const application::cycle_send_assignment&) {
			return core::cycle_send_result{ .End = core::cycle_send_end::completed, .Session = "s-1", .Reply = reply_for(run.ProfileRoot, run.Subject, core::cycle_round_id{ 1 }, "RESP-1") };
		};
		work.Host->send(claude);
		expect(work.share().Frozen && work.share().Send->Sending && contains(work.share().Status, "Sending F1 to Claude in a new session"),
			   "frozen, and said to be sending in a new session");
		work.Host->send(terminal);
		expect(contains(work.share().Status, "A send is under way") && work.Sender->Received.empty(), "one send at a time");
		work.deliver();
		expect(work.Sender->Received.size() == 1U, "one assignment");
		const auto& sent = work.Sender->Received.front();
		expect(sent.Document == preview && sent.Profile.Id == core::claude_stream_json_profile_id && !sent.Session.Continue && sent.WorkingDirectory == run.Project.string(),
			   "the previewed document, the chosen agent, a new session, in the cycle's folder");
		work.Host->share(core::cycle_share_route::copy_document);
		work.Lanes.drain();
		expect(work.Clipboard.Written.size() == 1U && work.Clipboard.Written.front() == sent.Document, "payload parity: Copy of the same round gives the same bytes");
		// What `bpr cycle export RC-1 --round F1` prints: the runtime call the command makes.
		const auto printed = settle<application::cycle_share_output>(run.ProfileRoot, run.Subject, [](auto& aRuntime, auto aDone) {
			aRuntime.export_round_selected({ .Round = core::cycle_round_id{ 1 }, .Channel = core::cycle_disclosure_channel::terminal_output, .Format = core::cycle_round_file_kind::markdown },
										   std::move(aDone));
		});
		expect(printed.Decision == core::cycle_authorization::allowed && printed.Bytes == sent.Document, "and the terminal's export prints the same bytes");

		const auto records = deliveries(run.ProfileRoot, run.Subject);
		using enum core::cycle_delivery_state;
		expect(states_of(records, 0) == std::vector{ accepted_by_adapter, recipient_acknowledged, copied }, "accepted and acknowledged; finishing claims nothing more");
		const auto digest = core::cycle_material_digest{ core::sha256({ reinterpret_cast<const std::uint8_t*>(sent.Document.data()), sent.Document.size() }) };
		expect(records[0].Observation.Output && records[0].Observation.Output->Digest == digest && records[0].Observation.Output->Bytes == sent.Document.size(),
			   "the bytes the adapter took");
		expect(records[1].Observation.Destination == "claude_stream_json/s-1" && records[1].Observation.Channel == core::cycle_disclosure_channel::agent_adapter, "the session it reported");
		const auto answered = responses(run.ProfileRoot, run.Subject);
		expect(answered.size() == 1U && answered[0].first.Channel == core::cycle_response_channel::agent_adapter && answered[0].first.Round == core::cycle_round_id{ 1 }
				   && answered[0].first.ForeignId == "RESP-1",
			   "the reply's response, checked and appended like any other, from the adapter");
		expect(work.view().Revisions.size() == revisions, "nothing captured");
		work.Host->compose();
		work.Lanes.drain();
		expect(offer_of(work.share(), core::claude_stream_json_profile_id).Sessions == std::vector<std::string>{ "s-1" }, "that session can be continued now");
		expect(offer_of(work.share(), core::external_terminal_profile_id).Sessions.empty(), "only by the harness that reported it");

		// Continue it, exactly; the prose reply is the reviewer's to keep.
		work.goal("Second round.");
		work.Sender->Answer = [](const application::cycle_send_assignment&) {
			return core::cycle_send_result{ .End = core::cycle_send_end::completed, .Session = "s-1", .Reply = "I renamed it; tests pass." };
		};
		work.Host->send({ .Profile = core::claude_stream_json_profile_id, .Session = { .Continue = "s-1" } });
		expect(contains(work.share().Status, "continuing session s-1"), "the session it continues, said");
		work.deliver();
		expect(work.Sender->Received.size() == 2U && work.Sender->Received.back().Session.Continue == std::optional<std::string>{ "s-1" }, "bound to exactly that session");
		expect(contains(work.share().Status, "Its reply is below") && work.share().Send->Reply == "I renamed it; tests pass." && work.share().Send->ReplyRound == core::cycle_round_id{ 2 },
			   "prose is shown, not appended");
		expect(responses(run.ProfileRoot, run.Subject).size() == 1U, "nothing appended by itself");
		work.Host->add_send_reply();
		work.Lanes.drain();
		const auto kept = responses(run.ProfileRoot, run.Subject);
		expect(kept.size() == 2U && kept[1].first.Channel == core::cycle_response_channel::manual_entry && kept[1].first.Round == core::cycle_round_id{ 2 }
				   && std::get<core::cycle_manual_reply>(kept[1].second).From == "Claude (Send to Agent)",
			   "added as the reviewer's own entry, to its round");
		expect(work.share().Send && work.share().Send->Reply.empty(), "and no longer waiting");

		work.goal("Third round.");
		work.Host->send({ .Profile = core::claude_stream_json_profile_id, .Session = { .Continue = "s-9" } });
		expect(contains(work.share().Status, "was not reported to a send of this cycle"), "a session never reported is not continued");
		work.Host->send({ .Profile = core::external_terminal_profile_id, .Session = { .Continue = "s-1" } });
		expect(contains(work.share().Status, "cannot continue a session"), "a harness that cannot continue is not told to");
		expect(work.Sender->Received.size() == 2U && work.view().Facts.Rounds.size() == 2U, "neither froze or sent anything");
		std::cout << "NATIVE: send one exact assignment, acknowledged, response appended, session continued, prose kept by hand\n";
	}

	/// RC60-09: a late reply keeps to its own round, an uncertain send is
	/// never sent again or replaced by another way, and a send outlives the
	/// cycle being closed.
	void keeps_late_and_uncertain_sends_to_their_round(const std::filesystem::path& aScratch)
	{
		tests::sqlite_contract::scratch_root profileScratch{ "cycle-send-late" };
		auto                                 work = desk{ profileScratch.path(), 4, true };
		const auto                           run  = start(work, profileScratch.path(), aScratch / "late-project");
		allow(run.ProfileRoot, run.Subject, core::cycle_channel_permission::allowed, 1);

		// F1 goes out; F2 is copied before F1's reply comes back. The reply
		// names F1, so it is F1's, whichever round is open now.
		work.Host->compose();
		work.Lanes.drain();
		work.Host->send(claude);
		work.Lanes.drain();
		expect(work.SendLane.pending() == 1U, "F1 is with the adapter");
		work.goal("Moved on.");
		work.Host->share(core::cycle_share_route::copy_document);
		expect(work.view().Facts.Rounds.size() == 2U && work.share().Round == core::cycle_round_id{ 2 }, "F2 frozen and shown meanwhile");
		work.Sender->Answer = [&](const application::cycle_send_assignment&) {
			return core::cycle_send_result{ .End = core::cycle_send_end::completed, .Session = "s-1", .Reply = reply_for(run.ProfileRoot, run.Subject, core::cycle_round_id{ 1 }, "RESP-F1") };
		};
		work.deliver();
		auto answered = responses(run.ProfileRoot, run.Subject);
		expect(answered.size() == 1U && answered[0].first.Round == core::cycle_round_id{ 1 } && contains(work.share().Status, "Its response is added to F1"), "the late reply is F1's, not F2's");
		auto records = deliveries(run.ProfileRoot, run.Subject);
		expect(records.size() == 3U && records[1].Observation.Round.Id == core::cycle_round_id{ 1 } && records[2].Observation.Round.Id == core::cycle_round_id{ 1 },
			   "and the send is recorded against F1, the round it was sent");

		// F2 goes out; its reply names F1: wrong-round output, refused.
		work.Host->send(claude);
		work.Sender->Answer = [&](const application::cycle_send_assignment&) {
			return core::cycle_send_result{ .End = core::cycle_send_end::completed, .Reply = reply_for(run.ProfileRoot, run.Subject, core::cycle_round_id{ 1 }, "RESP-WRONG") };
		};
		work.deliver();
		expect(contains(work.share().Status, "Its reply held a response: The response is for another round; nothing was added."), "a reply about another round is refused");
		answered = responses(run.ProfileRoot, run.Subject);
		expect(answered.size() == 1U, "nothing appended to either round");

		// Uncertain: the harness stopped after it had the round. Never again by itself.
		const auto sends    = work.Sender->Received.size();
		const auto from     = deliveries(run.ProfileRoot, run.Subject).size();
		work.Sender->Answer = [](const application::cycle_send_assignment&) { return core::cycle_send_result{ .End = core::cycle_send_end::stopped, .Session = "s-2" }; };
		work.goal("Uncertain.");
		work.Host->send(claude);
		work.deliver();
		using enum core::cycle_delivery_state;
		records = deliveries(run.ProfileRoot, run.Subject);
		expect(states_of(records, from) == std::vector{ accepted_by_adapter, recipient_acknowledged, uncertain }, "accepted, acknowledged, then uncertain");
		expect(contains(work.share().Status, "is not known. It was not sent again."), "said as uncertain");
		for (auto turn = 0; turn < 3; ++turn) {
			work.deliver();
		}
		expect(work.Sender->Received.size() == sends + 1U && work.SendLane.pending() == 0U, "no resend");
		expect(deliveries(run.ProfileRoot, run.Subject).size() == records.size(), "no copy, save or other agent in its place");

		// Not started: failed, and nothing else was tried.
		work.Sender->Answer = [](const application::cycle_send_assignment&) { return core::cycle_send_result{ .End = core::cycle_send_end::not_started }; };
		work.goal("Missing harness.");
		work.Host->send(terminal);
		work.deliver();
		records = deliveries(run.ProfileRoot, run.Subject);
		expect(records.back().Observation.State == failed && !records.back().Observation.Output && contains(work.share().Status, "could not be started"), "failed: nothing left");

		// Stopped by the reviewer before it ran.
		work.goal("Stopped.");
		work.Host->send(claude);
		work.Lanes.drain();
		work.Host->cancel_send();
		expect(work.Sender->Cancels == 1U && contains(work.share().Status, "Stopping the send"), "Stop reaches the adapter");
		work.deliver();
		records = deliveries(run.ProfileRoot, run.Subject);
		expect(states_of(records, records.size() - 2U) == std::vector{ accepted_by_adapter, uncertain } && contains(work.share().Status, "You stopped the send"), "stopped: uncertain");

		// The cycle closes while a send runs: it is still recorded against its round.
		const auto before   = deliveries(run.ProfileRoot, run.Subject).size();
		work.Sender->Answer = [](const application::cycle_send_assignment&) { return core::cycle_send_result{ .End = core::cycle_send_end::completed, .Session = "s-3" }; };
		work.goal("Closed while sending.");
		work.Host->send(claude);
		work.Lanes.drain();
		work.Host->close_compose();
		work.Host->close();
		work.Lanes.drain();
		work.deliver();
		records = deliveries(run.ProfileRoot, run.Subject);
		expect(records.size() == before + 2U && records.back().Observation.Destination == "claude_stream_json/s-3", "recorded although the cycle was closed");
		expect(work.Host->is_quiescent(), "nothing left running");
		std::cout << "NATIVE: late wrong-round reply refused, uncertain kept uncertain, no resend or fallback, recorded after close\n";
	}

	/// RC60-09: with the adapter gone the cycle reads every send and response
	/// and carries on by hand.
	void carries_on_without_the_adapter(const std::filesystem::path& aScratch)
	{
		tests::sqlite_contract::scratch_root profileScratch{ "cycle-send-removed" };
		auto                                 run = course{ };
		{
			auto work = desk{ profileScratch.path(), 5, true };
			run       = start(work, profileScratch.path(), aScratch / "removed-project");
			allow(run.ProfileRoot, run.Subject, core::cycle_channel_permission::allowed, 1);
			work.Host->compose();
			work.Lanes.drain();
			work.Sender->Answer = [&](const application::cycle_send_assignment&) {
				return core::cycle_send_result{ .End = core::cycle_send_end::completed, .Session = "s-1", .Reply = reply_for(run.ProfileRoot, run.Subject, core::cycle_round_id{ 1 }, "RESP-1") };
			};
			work.Host->send(claude);
			work.deliver();
			work.goal("Sent, then removed.");
			work.Sender->Answer = [](const application::cycle_send_assignment&) { return core::cycle_send_result{ .End = core::cycle_send_end::timed_out }; };
			work.Host->send(claude);
			work.deliver();
		}
		auto after = desk{ profileScratch.path(), 6, false };
		after.Host->resume(run.Subject);
		after.Lanes.drain();
		after.Host->compose();
		after.Lanes.drain();
		const auto& shown = after.share();
		expect(!route_of(shown, core::cycle_share_route::send_to_agent) && !shown.Send, "no adapter, no route");
		using enum core::cycle_delivery_state;
		expect(shown.Deliveries.size() == 4U && states_of(shown.Deliveries, 0) == std::vector{ accepted_by_adapter, recipient_acknowledged, accepted_by_adapter, uncertain },
			   "every observation reads without it");
		expect(responses(run.ProfileRoot, run.Subject).size() == 1U, "and the response it brought");
		after.Host->attach_reply(core::cycle_round_id{ 2 }, core::cycle_manual_reply{ .From = "Pasted from the agent's terminal", .Body = "Done by hand." });
		after.Lanes.drain();
		expect(responses(run.ProfileRoot, run.Subject).size() == 2U, "a reply entered by hand continues the round");
		after.Host->share(core::cycle_share_route::copy_document);
		after.Lanes.drain();
		expect(after.Clipboard.Written.size() == 1U && contains(after.Clipboard.Written.front(), "Sent, then removed."), "and Copy shares the round");
		std::cout << "NATIVE: adapter removed mid-cycle, history intact, manual continuation\n";
	}
}

auto main() -> int
try {
	tests::sqlite_contract::scratch_root scratch{ "cycle-send" };
	refuses_before_anything_leaves(scratch.path());
	sends_one_exact_assignment(scratch.path());
	keeps_late_and_uncertain_sends_to_their_round(scratch.path());
	carries_on_without_the_adapter(scratch.path());
	std::cout << "review-cycle-send-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "review cycle send failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
