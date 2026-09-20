#include "bpr/application/manual_executor.hpp"
#include "bpr/application/release_update_application_host.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
	using namespace bpr;
	using reason  = core::release_update_reason;
	using phase   = core::release_update_phase;
	using command = core::release_update_command;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	struct fake_clock final : application::release_update_clock_port
	{
		core::release_update_time Time{ 100'000, 100U };
		auto                      now() noexcept -> core::release_update_time override { return Time; }
	};
	struct fake_access final : application::release_update_access_port
	{
		core::release_update_access_id   Binding{ 7U };
		core::release_update_access_kind Kind = core::release_update_access_kind::native_github_com;
		core::release_update_release     Release{ core::release_update_source::better_pull_review_github_com,
												  core::release_update_release_id{ 10U },
												  "v2.3.4",
												  false,
												  false,
												  { { core::release_update_asset_id{ 20U }, "BetterPullReview-2.3.4-macos-universal.zip", 100U, { }, true } } };
		reason                           Failure           = reason::none;
		std::size_t                      Checks            = 0U;
		std::size_t                      Downloads         = 0U;
		bool                             Throw             = false;
		bool                             WrongArchive      = false;
		bool                             RevokeDuringCheck = false;
		std::stop_token                  LastStop;
		auto                             binding() const noexcept -> core::release_update_access_id override { return Binding; }
		auto                             kind() const noexcept -> core::release_update_access_kind override { return Kind; }
		auto                             latest(const std::stop_token aStop) -> application::release_update_check_result override
		{
			++Checks;
			LastStop = aStop;
			if (Throw) {
				throw std::runtime_error{ "fixture failure" };
			}
			if (RevokeDuringCheck) {
				Binding = core::release_update_access_id{ };
			}
			return { Failure, Release, 60U };
		}
		auto download(const core::release_update_candidate& aCandidate, const std::stop_token aStop,
					  const std::function<void(std::uint64_t)>& aProgress) -> application::release_update_download_result override
		{
			++Downloads;
			LastStop = aStop;
			for (std::uint64_t bytes = 0U; bytes <= aCandidate.Size; ++bytes) {
				aProgress(bytes);
			}
			auto candidate = aCandidate;
			if (WrongArchive) {
				++candidate.Size;
			}
			return { Failure, std::make_shared<application::release_update_archive>(candidate), aCandidate.Size, 60U };
		}
	};
	struct fake_verifier final : application::release_update_verification_port
	{
		std::size_t     Calls       = 0U;
		reason          Failure     = reason::none;
		bool            WrongBundle = false;
		std::stop_token LastStop;
		auto            verify(std::shared_ptr<const application::release_update_archive> aArchive,
							   const std::stop_token                                      aStop) -> application::release_update_verification_result override
		{
			++Calls;
			LastStop       = aStop;
			auto candidate = aArchive->candidate();
			if (WrongBundle) {
				candidate.Asset = core::release_update_asset_id{ 900U };
			}
			return { Failure, std::make_shared<application::release_update_verified_bundle>(candidate) };
		}
	};
	struct projection_sink final : application::release_update_projection_port
	{
		std::vector<application::release_update_projection> Seen;
		std::function<void()>                               Feedback;
		void                                                publish(const application::release_update_projection& aProjection, bool) override
		{
			Seen.push_back(aProjection);
			if (Feedback) {
				Feedback();
			}
		}
	};
	struct fixture
	{
		fake_clock                                   Clock;
		application::manual_executor                 Decision;
		application::manual_executor                 Effects;
		std::shared_ptr<fake_access>                 Access   = std::make_shared<fake_access>();
		std::shared_ptr<fake_verifier>               Verifier = std::make_shared<fake_verifier>();
		projection_sink                              Sink;
		application::release_update_application_host Host{ core::application_session_id{ 1U }, "1.2.3", core::release_update_capability::eligible, { }, Clock, Decision, Effects, Verifier, Sink };

		void settle()
		{
			for (std::size_t turns = 0U; turns < 100U && (Effects.pending() || Decision.pending()); ++turns) {
				if (Effects.pending()) {
					Effects.run_next();
				}
				Decision.drain();
			}
			expect(Host.quiescent(), "host owns no unaccounted work after draining");
		}
		void offer()
		{
			Host.select_access(Access);
			expect(Host.dispatch({ command::check, { } }), "manual gesture admitted");
			expect(Access->Checks == 0U && Host.projection().Phase == phase::checking, "port never runs on decision lane");
			settle();
			expect(Host.projection().Phase == phase::available, "public host projects offered update");
		}
	};

	void test_public_course_and_dependency_cut()
	{
		fixture course;
		expect(course.Host.dispatch({ command::check, { } }), "gesture reaches host with missing release port");
		course.settle();
		expect(!course.Host.projection().Candidate && course.Access->Checks == 0U, "negative control: absent release capability cannot offer");
		course.offer();
		expect(course.Host.dispatch({ command::download, course.Host.projection().Candidate }), "download gesture admitted");
		course.Effects.run_next();
		expect(course.Decision.pending() == 2U, "101 progress observations coalesce to one pending update plus terminal");
		course.settle();
		expect(course.Host.projection().Phase == phase::ready && course.Verifier->Calls == 1U
				   && course.Access->Downloads == 1U,
			   "gesture/host/fake ports/core/projection reaches verified ready");
		expect(!course.Host.projection().Commands[5].enabled(), "ready cannot expose absent installation capability");
		course.Host.shutdown();
		course.settle();
	}

	void test_out_of_order_accounts_and_revoked_lease()
	{
		fixture course;
		course.Host.select_access(course.Access);
		expect(course.Host.dispatch({ command::check, { } }), "first check admitted");
		course.Effects.run_next();
		auto replacement                    = std::make_shared<fake_access>();
		replacement->Binding                = core::release_update_access_id{ 8U };
		replacement->Release.Tag            = "v3.0.0";
		replacement->Release.Assets[0].Name = "BetterPullReview-3.0.0-macos-universal.zip";
		course.Host.select_access(replacement);
		expect(course.Access->LastStop.stop_requested(), "replacement cancels old lease job before returning");
		expect(course.Host.dispatch({ command::check, { } }), "replacement check admitted");
		course.Effects.run_next();
		course.Decision.run(1U);
		expect(course.Host.projection().Candidate->Tag == "v3.0.0", "new account terminal accepted first");
		course.Decision.run_next();
		expect(course.Host.projection().Candidate->Tag == "v3.0.0", "late old account result cannot replace offer");
		course.Host.select_access({ });
		course.settle();
		expect(!course.Host.projection().Candidate, "revocation clears accepted candidate");

		fixture revoked;
		revoked.Access->RevokeDuringCheck = true;
		revoked.Host.select_access(revoked.Access);
		expect(revoked.Host.dispatch({ command::check, { } }), "revoked fixture check admitted");
		revoked.settle();
		expect(revoked.Host.projection().Reason == reason::access_required && !revoked.Host.projection().Candidate,
			   "lease revocation during transport cannot certify release access");

		fixture delivered;
		delivered.Host.select_access(delivered.Access);
		expect(delivered.Host.dispatch({ command::check, { } }), "queued revocation check admitted");
		delivered.Effects.run_next();
		delivered.Access->Binding = core::release_update_access_id{ };
		delivered.settle();
		expect(delivered.Host.projection().Reason == reason::access_required && !delivered.Host.projection().Candidate,
			   "revocation between worker completion and serialized delivery refuses acceptance");
	}

	void test_failures_and_staged_identity()
	{
		for (const auto failure : { reason::inaccessible, reason::unauthenticated, reason::forbidden, reason::offline,
									reason::rate_limited, reason::malformed_release, reason::certificate, reason::redirect, reason::timed_out }) {
			fixture course;
			course.Access->Failure = failure;
			course.Host.select_access(course.Access);
			expect(course.Host.dispatch({ command::check, { } }), "failing check admitted");
			course.settle();
			expect(course.Host.projection().Phase == phase::failed && course.Host.projection().Reason == failure
					   && !course.Host.projection().Preferences.LastSuccess,
				   "failure remains typed and never current/successful");
		}
		for (const auto failure : { reason::unsafe_archive, reason::invalid_bundle, reason::wrong_architecture,
									reason::wrong_signer, reason::digest_mismatch, reason::changed_candidate, reason::size_limit }) {
			fixture course;
			course.offer();
			course.Verifier->Failure = failure;
			expect(course.Host.dispatch({ command::download, course.Host.projection().Candidate }), "download for rejected staging admitted");
			course.settle();
			expect(course.Host.projection().Phase == phase::failed && course.Host.projection().Reason == failure,
				   "staging failure cannot become ready");
		}
		for (const bool wrongArchive : { true, false }) {
			fixture course;
			course.offer();
			course.Access->WrongArchive  = wrongArchive;
			course.Verifier->WrongBundle = !wrongArchive;
			expect(course.Host.dispatch({ command::download, course.Host.projection().Candidate }), "identity mismatch download admitted");
			course.settle();
			expect(course.Host.projection().Reason == reason::changed_candidate && !course.Host.projection().Candidate,
				   "opaque archive and verified bundle still require exact candidate identity");
		}
		fixture throwing;
		throwing.Access->Throw = true;
		throwing.Host.select_access(throwing.Access);
		expect(throwing.Host.dispatch({ command::check, { } }), "exception check admitted");
		throwing.settle();
		expect(throwing.Host.projection().Reason == reason::unavailable, "effect exception becomes a typed terminal");
	}

	void test_shutdown_and_cancellation_in_every_phase()
	{
		for (const auto active : { phase::checking, phase::downloading, phase::verifying }) {
			fixture course;
			if (active == phase::checking) {
				course.Host.select_access(course.Access);
				expect(course.Host.dispatch({ command::check, { } }), "pending check admitted");
			} else {
				course.offer();
				expect(course.Host.dispatch({ command::download, course.Host.projection().Candidate }), "pending download admitted");
				if (active == phase::verifying) {
					course.Effects.run_next();
					course.Decision.drain();
				}
			}
			course.Effects.run_next();
			expect(!course.Host.quiescent(), "terminal awaiting decision remains accounted");
			course.Host.shutdown();
			const auto stop = active == phase::verifying ? course.Verifier->LastStop : course.Access->LastStop;
			expect(stop.stop_requested(), "shutdown propagates cancellation to each active phase");
			course.settle();
			expect(course.Host.projection().Reason == reason::shutdown && !course.Host.projection().Candidate,
				   "late success after shutdown cannot offer or become ready");
		}
		for (const bool disable : { true, false }) {
			fixture course;
			course.offer();
			expect(course.Host.dispatch({ command::download, course.Host.projection().Candidate }), "cancel course download admitted");
			course.Effects.run_next();
			if (disable) {
				expect(course.Host.set_automatic_checks(false), "disable admitted");
			} else {
				expect(course.Host.dispatch({ command::cancel, { } }), "cancel admitted");
			}
			course.settle();
			expect(course.Access->LastStop.stop_requested() && course.Verifier->Calls == 0U && !course.Host.projection().Candidate,
				   "disable/cancel prevents stale download from starting verification");
		}
	}

	void test_resource_bounds_and_projection_reentrancy()
	{
		fixture bounded;
		for (std::uint64_t identity = 1U; identity <= 12U; ++identity) {
			auto access     = std::make_shared<fake_access>();
			access->Binding = core::release_update_access_id{ identity };
			bounded.Host.select_access(access);
			expect(bounded.Host.dispatch({ command::check, { } }), "bounded request admitted to core");
		}
		expect(bounded.Effects.pending() == 4U && bounded.Host.projection().Reason == reason::busy,
			   "cancelled but unfinished jobs count toward the four-job bound");
		bounded.settle();

		fixture reentrant;
		reentrant.Host.select_access(reentrant.Access);
		std::size_t admitted    = 0U;
		bool        fedBack     = false;
		reentrant.Sink.Feedback = [&] {
			if (fedBack) {
				return;
			}
			fedBack = true;
			for (std::size_t index = 0U; index < 100U; ++index) {
				if (reentrant.Host.dispatch({ command::check, { } })) {
					++admitted;
				}
			}
			throw std::runtime_error{ "projection failure" };
		};
		expect(reentrant.Host.dispatch({ command::check, { } }), "reentrant check admitted");
		expect(admitted == 64U && reentrant.Effects.pending() == 1U, "bounded FIFO reentrancy never creates overlapping core checks");
		reentrant.settle();
		expect(reentrant.Host.projection().Phase == phase::available, "projection exception cannot veto acceptance");

		fixture closing;
		closing.Host.select_access(closing.Access);
		closing.Sink.Feedback = [&] { closing.Host.shutdown(); };
		expect(closing.Host.dispatch({ command::check, { } }), "shutdown feedback admitted");
		closing.settle();
		expect(closing.Access->Checks == 0U && closing.Host.projection().Reason == reason::shutdown,
			   "reentrant shutdown has priority and cancels a queued effect before access");

		fixture replacing;
		auto    other           = std::make_shared<fake_access>();
		other->Binding          = core::release_update_access_id{ 8U };
		bool alternate          = false;
		replacing.Sink.Feedback = [&] {
			alternate = !alternate;
			replacing.Host.select_access(alternate ? other : replacing.Access);
		};
		replacing.Host.select_access(replacing.Access);
		expect(replacing.Sink.Seen.size() <= 67U && replacing.Host.projection().Reason == reason::access_required,
			   "unbounded replacement feedback ends with authority revoked");
		replacing.Sink.Feedback = { };
		replacing.settle();
	}

	struct rejecting_effects final : application::effect_executor
	{
		void post(std::function<void()>) override { throw std::runtime_error{ "queue unavailable" }; }
	};

	void test_executor_refusal_and_destroyed_projection()
	{
		fake_clock                                   clock;
		application::manual_executor                 decision;
		rejecting_effects                            effects;
		projection_sink                              sink;
		auto                                         access = std::make_shared<fake_access>();
		application::release_update_application_host host{ core::application_session_id{ 1U }, "1.2.3", core::release_update_capability::eligible, { }, clock, decision, effects, { }, sink };
		host.select_access(access);
		expect(host.dispatch({ command::check, { } }), "core accepts before executor refusal");
		expect(host.quiescent() && host.projection().Reason == reason::unavailable && access->Checks == 0U,
			   "effect post refusal retires its job and produces one terminal");

		application::manual_executor pendingEffects;
		{
			auto temporary = std::make_unique<application::release_update_application_host>(core::application_session_id{ 2U },
																							"1.2.3", core::release_update_capability::eligible, core::release_update_preferences{ }, clock,
																							decision, pendingEffects, nullptr, sink);
			temporary->select_access(access);
			expect(temporary->dispatch({ command::check, { } }), "destroy course check admitted");
		}
		const auto publications = sink.Seen.size();
		pendingEffects.drain();
		decision.drain();
		expect(access->Checks == 0U && sink.Seen.size() == publications, "destroyed host leaves safe counted cancellation and no projection callback");
	}
}

auto main() -> int
{
	test_public_course_and_dependency_cut();
	test_out_of_order_accounts_and_revoked_lease();
	test_failures_and_staged_identity();
	test_shutdown_and_cancellation_in_every_phase();
	test_resource_bounds_and_projection_reentrancy();
	test_executor_refusal_and_destroyed_projection();
	std::cout << "release-update-host-tests:pass\n";
}
