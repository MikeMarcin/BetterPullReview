#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	using application::github_credential_material;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto bytes_of(const std::string_view aText)
		-> std::vector<std::byte>
	{
		auto bytes = std::vector<std::byte>{ };
		bytes.reserve(aText.size());
		for (const auto character : aText) {
			bytes.push_back(static_cast<std::byte>(character));
		}
		return bytes;
	}

	[[nodiscard]] auto text_of(const std::span<const std::byte> aBytes)
		-> std::string
	{
		auto text = std::string{ };
		text.reserve(aBytes.size());
		for (const auto byte : aBytes) {
			text.push_back(static_cast<char>(byte));
		}
		return text;
	}

	// Synthetic stand-ins throughout. No value here is, or resembles, material
	// any server ever issued.
	[[nodiscard]] auto material(const std::string_view aText)
		-> github_credential_material
	{
		auto created = github_credential_material::create(bytes_of(aText));
		expect(created.has_value(), "synthetic material is created");
		return std::move(*created);
	}

	[[nodiscard]] auto profile() -> core::host_profile_id
	{
		return core::host_profile_id{ 7U };
	}

	[[nodiscard]] auto authority()
		-> application::github_credential_authority_key
	{
		return application::github_credential_authority_key{
			.Profile         = profile(),
			.ProfileRevision = application::github_profile_revision{ 2U },
			.Host            = core::git_host_id{ 3U },
			.Hostname        = application::github_canonical_hostname{ "github.com" },
			.Account         = core::github_account_id{ 4U },
			.ServerContract  = application::github_server_contract::
				github_com_2022_11_28,
		};
	}

	[[nodiscard]] auto initial_state() -> core::device_authorization_state
	{
		return core::device_authorization_state{
			.ApplicationSession = core::application_session_id{ 5U },
			.NextRequest        = core::request_id{ 1U },
			.IdentityGeneration = core::github_identity_generation{ 3U },
			.Generation         = core::device_authorization_generation{ },
			.Stage              = core::device_authorization_stage::idle,
			.Pending            = std::nullopt,
			.Profile            = std::nullopt,
			.TerminalReason     = core::device_authorization_reason_code::none,
		};
	}

	class fake_clock final : public application::device_authorization_clock_port
	{
	public:
		[[nodiscard]] auto observe_monotonic() noexcept
			-> std::chrono::milliseconds override
		{
			return Now;
		}

		void advance(const std::uint32_t aSeconds)
		{
			Now += std::chrono::seconds{ aSeconds };
		}

		std::chrono::milliseconds Now{ 0 };
	};

	class fake_timer final : public application::device_authorization_timer_port
	{
	public:
		[[nodiscard]] auto schedule_after(
			const std::uint32_t   aSeconds,
			std::function<void()> aExpiry) -> bool override
		{
			Delays.push_back(aSeconds);
			Expiry = std::move(aExpiry);
			return true;
		}

		void cancel() noexcept override
		{
			Expiry = { };
		}

		[[nodiscard]] auto armed() const noexcept -> bool
		{
			return static_cast<bool>(Expiry);
		}

		[[nodiscard]] auto last_delay() const -> std::uint32_t
		{
			expect(!Delays.empty(), "a delay was scheduled");
			return Delays.back();
		}

		void fire()
		{
			auto expiry = std::exchange(Expiry, std::function<void()>{ });
			expect(expiry != nullptr, "the timer was armed before it fired");
			expiry();
		}

		std::function<void()>      Expiry;
		std::vector<std::uint32_t> Delays;
	};

	// The order log is the point of this file. Rotation is only safe if the
	// renewable material becomes durable before the access token is enrolled,
	// and the only way to see that is to watch both boundaries in one sequence.
	using order_log = std::vector<std::string>;

	class scripted_transport final
		: public application::device_authorization_transport_port
	{
	public:
		explicit scripted_transport(order_log& aOrder)
			: Order{ &aOrder }
		{
		}

		void acquire_device_code(
			core::device_authorization_request_context  aContext,
			application::device_code_completion_handler aCompletion) override
		{
			CodeRequests.push_back(aContext);
			CodeCompletion = std::move(aCompletion);
		}

		void exchange_device_token(
			application::device_token_exchange           aExchange,
			application::device_token_completion_handler aCompletion) override
		{
			TokenContext    = aExchange.Context;
			TokenCompletion = std::move(aCompletion);
		}

		void refresh_access_token(
			application::device_token_exchange           aExchange,
			application::device_token_completion_handler aCompletion) override
		{
			Order->emplace_back("refresh-exchange");
			RefreshMaterial.push_back(text_of(aExchange.Code.bytes()));
			RefreshContexts.push_back(aExchange.Context);
			RefreshCompletion = std::move(aCompletion);
		}

		void cancel(
			core::device_authorization_request_context aContext) override
		{
			Cancellations.push_back(aContext);
		}

		void answer_code(application::device_code_completion aCompletion)
		{
			auto handler = std::exchange(
				CodeCompletion,
				application::device_code_completion_handler{ });
			expect(handler != nullptr, "a device-code request was outstanding");
			handler(std::move(aCompletion));
		}

		void answer_token(application::device_token_completion aCompletion)
		{
			auto handler = std::exchange(
				TokenCompletion,
				application::device_token_completion_handler{ });
			expect(handler != nullptr, "a token request was outstanding");
			handler(std::move(aCompletion));
		}

		// Keeping the handler lets a test answer the same exchange twice, which
		// is how a duplicate terminal is put in front of the host.
		void answer_refresh(application::device_token_completion aCompletion)
		{
			if (KeepRefreshHandler) {
				expect(
					RefreshCompletion != nullptr,
					"a refresh exchange was outstanding");
				RefreshCompletion(std::move(aCompletion));
				return;
			}
			auto handler = std::exchange(
				RefreshCompletion,
				application::device_token_completion_handler{ });
			expect(handler != nullptr, "a refresh exchange was outstanding");
			handler(std::move(aCompletion));
		}

		[[nodiscard]] auto last_code_context() const
			-> core::device_authorization_request_context
		{
			expect(!CodeRequests.empty(), "a device code was requested");
			return CodeRequests.back();
		}

		[[nodiscard]] auto last_refresh_context() const
			-> core::device_authorization_request_context
		{
			expect(!RefreshContexts.empty(), "a refresh was requested");
			return RefreshContexts.back();
		}

		order_log*                                              Order;
		std::vector<core::device_authorization_request_context> CodeRequests;
		std::vector<core::device_authorization_request_context> RefreshContexts;
		std::vector<std::string>                                RefreshMaterial;
		std::vector<core::device_authorization_request_context> Cancellations;
		core::device_authorization_request_context              TokenContext;
		application::device_code_completion_handler             CodeCompletion;
		application::device_token_completion_handler            TokenCompletion;
		application::device_token_completion_handler            RefreshCompletion;
		bool                                                    KeepRefreshHandler = false;
	};

	class recording_enrollment final
		: public application::device_authorization_enrollment_port
	{
	public:
		explicit recording_enrollment(order_log& aOrder)
			: Order{ &aOrder }
		{
		}

		void enroll(
			const core::host_profile_id aProfile,
			github_credential_material  aCredential) override
		{
			Order->emplace_back("enroll");
			Profiles.push_back(aProfile);
			Credentials.push_back(text_of(aCredential.bytes()));
		}

		order_log*                         Order;
		std::vector<core::host_profile_id> Profiles;
		std::vector<std::string>           Credentials;
	};

	class recording_projections final
		: public application::device_authorization_projection_port
	{
	public:
		void publish(
			const application::device_authorization_presentation& aPresentation)
			override
		{
			Published.push_back(aPresentation);
		}

		std::vector<application::device_authorization_presentation> Published;
	};

	// One slot, in memory, under manual control. Completions run immediately
	// because the host marshals them onto its own lane anyway; what the test
	// controls is what the slot answers.
	//
	// One slot rather than two because the refresh-only port is all the device
	// host is given. The base-token slot used to be stubbed here to satisfy the
	// full vault, and every one of those stubs was unreachable — a renewal
	// course that accidentally stored an access token would have been caught by
	// nothing. Now it would not compile.
	class slotted_vault final
		: public application::github_credential_refresh_vault_port
	{
	public:
		explicit slotted_vault(order_log& aOrder)
			: Order{ &aOrder }
		{
		}

		void store_refresh(
			application::github_credential_enrollment               aEnrollment,
			application::github_credential_store_completion_handler aCompletion)
			override
		{
			Order->emplace_back("store-refresh");
			const auto context = aEnrollment.Context;
			StoreContexts.push_back(context);
			if (RefuseStore) {
				aEnrollment.Credential.reset();
				aCompletion(application::github_credential_store_error{
					.Context = context,
					.Failure = application::github_credential_vault_failure::
						unavailable,
				});
				return;
			}

			Stored = text_of(aEnrollment.Credential.bytes());
			aEnrollment.Credential.reset();
			aCompletion(application::github_credential_store_success{
				.Context     = context,
				.Disposition = application::github_credential_store_disposition::
					stored,
			});
		}

		void retrieve_refresh(
			application::github_credential_operation_context           aContext,
			application::github_credential_retrieve_completion_handler aCompletion)
			override
		{
			Order->emplace_back("retrieve-refresh");
			RetrieveContexts.push_back(aContext);
			if (RefuseRetrieve) {
				aCompletion(application::github_credential_retrieve_error{
					.Context = aContext,
					.Failure = application::github_credential_vault_failure::
						unavailable,
				});
				return;
			}
			if (!Stored.has_value()) {
				aCompletion(application::github_credential_retrieve_absent{
					.Context = aContext,
				});
				return;
			}
			if (HoldRetrieve) {
				HeldRetrieve = std::move(aCompletion);
				HeldContext  = aContext;
				return;
			}
			aCompletion(application::github_credential_retrieve_success{
				.Context    = aContext,
				.Credential = material(*Stored),
			});
		}

		void delete_refresh(
			application::github_credential_operation_context         aContext,
			application::github_credential_delete_completion_handler aCompletion)
			override
		{
			const auto erased = Stored.has_value();
			Stored.reset();
			aCompletion(application::github_credential_delete_success{
				.Context     = aContext,
				.Disposition = erased
					? application::github_credential_delete_disposition::deleted
					: application::github_credential_delete_disposition::absent,
			});
		}

		void cancel(
			application::github_credential_operation_context aContext) override
		{
			Cancellations.push_back(aContext);
			if (HeldRetrieve) {
				auto held = std::exchange(
					HeldRetrieve,
					application::github_credential_retrieve_completion_handler{ });
				held(application::github_credential_retrieve_cancelled{
					.Context = aContext,
				});
			}
		}

		void release_held()
		{
			auto held = std::exchange(
				HeldRetrieve,
				application::github_credential_retrieve_completion_handler{ });
			expect(held != nullptr, "a retrieve was held");
			held(application::github_credential_retrieve_success{
				.Context    = HeldContext,
				.Credential = material(*Stored),
			});
		}

		order_log*                                                    Order;
		std::optional<std::string>                                    Stored;
		std::vector<application::github_credential_operation_context> StoreContexts;
		std::vector<application::github_credential_operation_context> RetrieveContexts;
		std::vector<application::github_credential_operation_context> Cancellations;
		application::github_credential_retrieve_completion_handler    HeldRetrieve;
		application::github_credential_operation_context              HeldContext;
		bool                                                          RefuseStore    = false;
		bool                                                          RefuseRetrieve = false;
		bool                                                          HoldRetrieve   = false;
	};

	struct harness
	{
		harness()
			: Transport{ Order }
			, Vault{ Order }
			, Enrollment{ Order }
			, Host{ initial_state(), Transport, Vault, Enrollment,
					Clock, Timer, Lane, Projections,
					application::device_authorization_host_limits{ },
					application::device_authorization_browser_handoff{ },
					// These courses are about renewal, which only a registered
					// build ever reaches.
					application::device_authorization_capability{
						.Registered = true } }
		{
		}

		void begin()
		{
			Host.dispatch(core::begin_device_authorization_intent{
				.Profile = profile(),
			});
		}

		[[nodiscard]] auto flow_context() const
			-> core::device_authorization_request_context
		{
			return Transport.last_code_context();
		}

		void reach_awaiting()
		{
			begin();
			Transport.answer_code(application::device_code_acquired{
				.Context = Transport.last_code_context(),
				.Grant   = application::device_code_acquisition{
					.DeviceCode       = material("device-code-1"),
					.UserCode         = "WDJB-MJHT",
					.VerificationUri  = "https://github.com/login/device",
					.ExpiresInSeconds = 900U,
					.IntervalSeconds  = 5U,
				},
			});
			Lane.drain();
		}

		void poll_once()
		{
			expect(Timer.armed(), "a poll is armed");
			Clock.advance(Timer.last_delay());
			Timer.fire();
			Lane.drain();
		}

		[[nodiscard]] auto issued(
			const std::uint32_t                   aLifetime = 3'600U,
			const std::string_view                aAccess   = "access-1",
			const std::optional<std::string_view> aRefresh  = "refresh-1")
			-> application::device_token_completion
		{
			auto refresh = std::optional<github_credential_material>{ };
			if (aRefresh.has_value()) {
				refresh.emplace(material(*aRefresh));
			}
			return application::device_token_issued{
				.Context = flow_context(),
				.Grant   = application::device_token_material{
					.AccessToken                  = material(aAccess),
					.RefreshToken                 = std::move(refresh),
					.ExpiresInSeconds             = aLifetime,
					.RefreshTokenExpiresInSeconds = 15'552'000U,
				},
			};
		}

		// Runs a whole device flow to the authorized terminal with the refresh
		// authority already bound, which is the re-authorization shape.
		void authorize_with_bound_authority(
			const std::uint32_t aLifetime = 3'600U)
		{
			Host.bind_refresh_authority(authority());
			reach_awaiting();
			poll_once();
			Transport.answer_token(issued(aLifetime));
			Lane.drain();
		}

		order_log                    Order;
		application::manual_executor Lane;
		fake_clock                   Clock;
		fake_timer                   Timer;
		scripted_transport           Transport;
		slotted_vault                Vault;
		recording_enrollment         Enrollment;
		recording_projections        Projections;

		application::device_authorization_application_host Host;
	};

	// The order is the invariant. If the access token were enrolled first, a
	// crash before the store would leave a vault holding renewable material the
	// server has already invalidated, and nothing able to renew.
	void test_the_refresh_entry_is_written_before_the_access_token_is_enrolled()
	{
		harness world;
		world.authorize_with_bound_authority();

		expect(
			world.Host.state().Stage
				== core::device_authorization_stage::authorized,
			"the flow is authorized");
		const auto expected = order_log{ "store-refresh", "enroll" };
		expect(world.Order == expected, "the refresh entry is durable first");
		expect(
			world.Vault.Stored.has_value() && *world.Vault.Stored == "refresh-1",
			"the vault holds the exact refresh material");
		expect(
			world.Enrollment.Credentials == std::vector<std::string>{ "access-1" },
			"and only the access token crossed the enrollment boundary");
		expect(
			!world.Host.holds_transient_material()
				&& !world.Host.holds_pending_refresh_material(),
			"the host keeps no copy of either");
	}

	void test_renewal_is_armed_before_the_access_token_expires()
	{
		harness world;
		world.authorize_with_bound_authority(3'600U);

		expect(world.Timer.armed(), "a renewal is armed");
		expect(
			world.Timer.last_delay() == 3'300U,
			"one lead time before the access token expires");
		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::scheduled,
			"and the host says so");
	}

	// A lifetime shorter than twice the lead renews at its own midpoint. A lead
	// longer than the lifetime would ask for a renewal in the past.
	void test_a_short_lifetime_renews_at_its_midpoint()
	{
		harness world;
		world.authorize_with_bound_authority(100U);

		expect(
			world.Timer.last_delay() == 50U,
			"a short-lived token renews halfway through");
	}

	void test_rotation_stores_the_new_material_before_enrolling_it()
	{
		harness world;
		world.authorize_with_bound_authority();
		world.Order.clear();

		world.Clock.advance(world.Timer.last_delay());
		world.Timer.fire();
		world.Lane.drain();

		expect(
			world.Transport.RefreshMaterial
				== std::vector<std::string>{ "refresh-1" },
			"renewal presents the stored refresh material");
		world.Transport.answer_refresh(application::device_token_issued{
			.Context = world.Transport.last_refresh_context(),
			.Grant   = application::device_token_material{
				.AccessToken                  = material("access-2"),
				.RefreshToken                 = material("refresh-2"),
				.ExpiresInSeconds             = 3'600U,
				.RefreshTokenExpiresInSeconds = 15'552'000U,
			},
		});
		world.Lane.drain();

		const auto expected = order_log{
			"retrieve-refresh",
			"refresh-exchange",
			"store-refresh",
			"enroll",
		};
		expect(world.Order == expected, "rotation keeps the same durable order");
		expect(
			*world.Vault.Stored == "refresh-2",
			"the new refresh material replaced the old one");
		expect(
			world.Enrollment.Credentials
				== std::vector<std::string>{ "access-1", "access-2" },
			"and the new access token was enrolled through the one path");
		expect(
			world.Host.refresh_observation().Rotations == 1U,
			"the rotation is counted");
		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::scheduled,
			"and the next renewal is armed");
		expect(
			!world.Host.holds_transient_material(),
			"nothing is retained afterwards");
	}

	// GitHub keeps the existing refresh entry alive when it issues no new one.
	// There is nothing to write down, so the ordering question does not arise.
	void test_a_renewal_without_new_refresh_material_keeps_the_old_entry()
	{
		harness world;
		world.authorize_with_bound_authority();
		world.Order.clear();

		world.Clock.advance(world.Timer.last_delay());
		world.Timer.fire();
		world.Lane.drain();
		world.Transport.answer_refresh(application::device_token_issued{
			.Context = world.Transport.last_refresh_context(),
			.Grant   = application::device_token_material{
				.AccessToken                  = material("access-2"),
				.RefreshToken                 = std::nullopt,
				.ExpiresInSeconds             = 3'600U,
				.RefreshTokenExpiresInSeconds = 0U,
			},
		});
		world.Lane.drain();

		expect(
			*world.Vault.Stored == "refresh-1",
			"the existing refresh entry survives");
		expect(
			world.Enrollment.Credentials.back() == "access-2",
			"and the new access token is still enrolled");
		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::scheduled,
			"renewal stays armed");
	}

	void test_a_refusal_to_store_the_first_refresh_entry_fails_the_flow()
	{
		harness world;
		world.Vault.RefuseStore = true;
		world.authorize_with_bound_authority();

		expect(
			world.Host.state().Stage
				== core::device_authorization_stage::failed,
			"the authorization fails rather than half succeeding");
		expect(
			world.Host.state().TerminalReason
				== core::device_authorization_reason_code::vault_unavailable,
			"and names the vault");
		expect(
			world.Enrollment.Profiles.empty(),
			"nothing was enrolled, so no profile is half authorized");
		expect(
			!world.Host.holds_transient_material()
				&& !world.Host.holds_pending_refresh_material(),
			"and both tokens were erased");
	}

	void test_a_refusal_to_store_rotated_material_requests_re_enrollment()
	{
		harness world;
		world.authorize_with_bound_authority();
		world.Vault.RefuseStore = true;

		world.Clock.advance(world.Timer.last_delay());
		world.Timer.fire();
		world.Lane.drain();
		world.Transport.answer_refresh(application::device_token_issued{
			.Context = world.Transport.last_refresh_context(),
			.Grant   = application::device_token_material{
				.AccessToken                  = material("access-2"),
				.RefreshToken                 = material("refresh-2"),
				.ExpiresInSeconds             = 3'600U,
				.RefreshTokenExpiresInSeconds = 0U,
			},
		});
		world.Lane.drain();

		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::
					reenrollment_required,
			"renewal asks for re-enrollment instead of retrying quietly");
		expect(
			world.Host.refresh_observation().Failure
				== application::device_authorization_refresh_failure::storage_failed,
			"with the exact reason");
		expect(
			world.Enrollment.Credentials.size() == 1U,
			"the unstorable rotation enrolled nothing");
		expect(!world.Timer.armed(), "and armed no retry");
	}

	void test_every_renewal_failure_is_typed_and_asks_for_re_enrollment()
	{
		{
			harness world;
			world.authorize_with_bound_authority();
			world.Clock.advance(world.Timer.last_delay());
			world.Timer.fire();
			world.Lane.drain();
			world.Transport.answer_refresh(
				application::device_token_refused{
					.Context = world.Transport.last_refresh_context(),
					.Reason  = application::device_token_refusal_reason::
						access_denied,
				});
			world.Lane.drain();
			expect(
				world.Host.refresh_observation().Failure
					== application::device_authorization_refresh_failure::
						refresh_rejected,
				"a refused renewal is reported as rejected material");
		}
		{
			harness world;
			world.authorize_with_bound_authority();
			world.Clock.advance(world.Timer.last_delay());
			world.Timer.fire();
			world.Lane.drain();
			world.Transport.answer_refresh(
				application::device_authorization_transport_error{
					.Context = world.Transport.last_refresh_context(),
					.Failure = application::device_authorization_transport_failure::
						network,
				});
			world.Lane.drain();
			expect(
				world.Host.refresh_observation().Failure
					== application::device_authorization_refresh_failure::
						transport_failed,
				"a failed exchange is reported as a failed exchange");
			expect(
				!world.Timer.armed(),
				"and does not become a retry loop");
		}
		{
			harness world;
			world.authorize_with_bound_authority();
			world.Vault.RefuseRetrieve = true;
			world.Clock.advance(world.Timer.last_delay());
			world.Timer.fire();
			world.Lane.drain();
			expect(
				world.Host.refresh_observation().Failure
					== application::device_authorization_refresh_failure::
						vault_unavailable,
				"an unreachable vault is reported as unavailable");
		}
		{
			harness world;
			world.authorize_with_bound_authority();
			world.Vault.Stored.reset();
			world.Clock.advance(world.Timer.last_delay());
			world.Timer.fire();
			world.Lane.drain();
			expect(
				world.Host.refresh_observation().Failure
					== application::device_authorization_refresh_failure::
						refresh_absent,
				"an empty slot is reported as absent");
			expect(
				world.Host.refresh_observation().State
					== application::device_authorization_refresh_state::
						reenrollment_required,
				"and the reviewer is asked to authorize again");
		}
	}

	// The restart path: nothing is authorized in this process yet, and the only
	// thing the host has is what the vault kept.
	void test_a_restart_renews_from_the_stored_refresh_entry()
	{
		harness world;
		world.Vault.Stored = "refresh-kept";

		world.Host.resume_refresh(authority());
		world.Lane.drain();
		expect(
			world.Transport.RefreshMaterial
				== std::vector<std::string>{ "refresh-kept" },
			"the stored material is presented for renewal");
		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::refreshing,
			"and the host says a renewal is in flight");

		world.Transport.answer_refresh(application::device_token_issued{
			.Context = world.Transport.last_refresh_context(),
			.Grant   = application::device_token_material{
				.AccessToken                  = material("access-restarted"),
				.RefreshToken                 = material("refresh-rotated"),
				.ExpiresInSeconds             = 3'600U,
				.RefreshTokenExpiresInSeconds = 15'552'000U,
			},
		});
		world.Lane.drain();

		expect(
			world.Enrollment.Credentials
				== std::vector<std::string>{ "access-restarted" },
			"a fresh access token is enrolled without a device flow");
		expect(
			*world.Vault.Stored == "refresh-rotated",
			"and the rotated refresh entry replaced the old one");
		expect(
			world.Transport.CodeRequests.empty(),
			"the reviewer was never asked for a code");
		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::scheduled,
			"with the next renewal armed");
	}

	void test_a_restart_with_an_empty_slot_asks_for_re_enrollment()
	{
		harness world;

		world.Host.resume_refresh(authority());
		world.Lane.drain();

		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::
					reenrollment_required,
			"there is nothing to renew from");
		expect(
			world.Host.refresh_observation().Failure
				== application::device_authorization_refresh_failure::refresh_absent,
			"and the reason is exact");
		expect(
			world.Transport.RefreshMaterial.empty(),
			"nothing was presented to the server");
	}

	// The authority a refresh entry is keyed by only exists once an
	// authenticated observation has bound the profile to an account, which is
	// after enrollment. Until then the material waits in memory.
	void test_refresh_material_waits_for_its_authority_and_is_stored_once_bound()
	{
		harness world;
		world.reach_awaiting();
		world.poll_once();
		world.Transport.answer_token(world.issued());
		world.Lane.drain();

		expect(
			world.Host.state().Stage
				== core::device_authorization_stage::authorized,
			"the access token is enrolled without waiting for an authority");
		expect(
			world.Host.holds_pending_refresh_material(),
			"and the refresh material is held until it has a slot");
		expect(!world.Vault.Stored.has_value(), "nothing was stored yet");
		expect(!world.Timer.armed(), "and no renewal is armed yet");

		world.Host.bind_refresh_authority(authority());
		world.Lane.drain();

		expect(
			world.Vault.Stored.has_value() && *world.Vault.Stored == "refresh-1",
			"binding the authority stores the material");
		expect(
			!world.Host.holds_pending_refresh_material(),
			"and the host stops holding it");
		expect(world.Timer.armed(), "renewal is armed once the entry is durable");
	}

	void test_pending_refresh_material_is_erased_by_shutdown()
	{
		harness world;
		world.reach_awaiting();
		world.poll_once();
		world.Transport.answer_token(world.issued());
		world.Lane.drain();
		expect(
			world.Host.holds_pending_refresh_material(),
			"the material is held");

		world.Host.shutdown();
		expect(
			!world.Host.holds_pending_refresh_material(),
			"shutdown erases material that never found a slot");

		world.Host.bind_refresh_authority(authority());
		world.Lane.drain();
		expect(
			!world.Vault.Stored.has_value(),
			"and a binding after shutdown stores nothing");
	}

	// A renewal resolves exactly once. A duplicate terminal for the same
	// exchange carries a whole second pair of tokens, and rotating from it would
	// enroll material the first rotation already replaced.
	void test_a_second_answer_for_a_resolved_renewal_rotates_nothing()
	{
		harness world;
		world.authorize_with_bound_authority();
		world.Transport.KeepRefreshHandler = true;

		world.Clock.advance(world.Timer.last_delay());
		world.Timer.fire();
		world.Lane.drain();
		const auto context = world.Transport.last_refresh_context();

		world.Transport.answer_refresh(application::device_token_issued{
			.Context = context,
			.Grant   = application::device_token_material{
				.AccessToken                  = material("access-2"),
				.RefreshToken                 = material("refresh-2"),
				.ExpiresInSeconds             = 3'600U,
				.RefreshTokenExpiresInSeconds = 0U,
			},
		});
		world.Lane.drain();
		const auto enrolled  = world.Enrollment.Credentials.size();
		const auto rotations = world.Host.refresh_observation().Rotations;

		world.Transport.answer_refresh(application::device_token_issued{
			.Context = context,
			.Grant   = application::device_token_material{
				.AccessToken                  = material("access-3"),
				.RefreshToken                 = material("refresh-3"),
				.ExpiresInSeconds             = 3'600U,
				.RefreshTokenExpiresInSeconds = 0U,
			},
		});
		world.Lane.drain();

		expect(
			world.Enrollment.Credentials.size() == enrolled,
			"a second answer for a resolved renewal enrolls nothing");
		expect(
			world.Host.refresh_observation().Rotations == rotations,
			"and counts no second rotation");
		expect(
			*world.Vault.Stored == "refresh-2",
			"the vault still holds the material the one rotation wrote");
		expect(
			!world.Host.holds_transient_material(),
			"and the duplicate's tokens were released rather than kept");
	}

	void test_shutdown_during_a_renewal_cancels_it_and_rotates_nothing()
	{
		harness world;
		world.authorize_with_bound_authority();
		world.Vault.HoldRetrieve = true;

		world.Clock.advance(world.Timer.last_delay());
		world.Timer.fire();
		world.Lane.drain();
		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::refreshing,
			"a renewal is in flight");

		world.Host.shutdown();
		world.Lane.drain();
		expect(
			world.Vault.Cancellations.size() == 1U,
			"shutdown cancelled the outstanding vault read");
		expect(
			world.Transport.RefreshMaterial.empty(),
			"nothing reached the server");
		expect(
			world.Enrollment.Credentials.size() == 1U,
			"and nothing rotated");
	}

	void test_an_answer_with_no_refresh_material_schedules_no_renewal()
	{
		harness world;
		world.Host.bind_refresh_authority(authority());
		world.reach_awaiting();
		world.poll_once();
		world.Transport.answer_token(
			world.issued(3'600U, "access-only", std::nullopt));
		world.Lane.drain();

		expect(
			world.Host.state().Stage
				== core::device_authorization_stage::authorized,
			"the flow is authorized");
		expect(
			world.Enrollment.Credentials
				== std::vector<std::string>{ "access-only" },
			"the access token is enrolled");
		expect(!world.Vault.Stored.has_value(), "nothing reached the refresh slot");
		expect(!world.Timer.armed(), "and no renewal is armed");
		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::idle,
			"there is nothing to renew");
	}
} // namespace

auto main() -> int
{
	test_the_refresh_entry_is_written_before_the_access_token_is_enrolled();
	test_renewal_is_armed_before_the_access_token_expires();
	test_a_short_lifetime_renews_at_its_midpoint();
	test_rotation_stores_the_new_material_before_enrolling_it();
	test_a_renewal_without_new_refresh_material_keeps_the_old_entry();
	test_a_refusal_to_store_the_first_refresh_entry_fails_the_flow();
	test_a_refusal_to_store_rotated_material_requests_re_enrollment();
	test_every_renewal_failure_is_typed_and_asks_for_re_enrollment();
	test_a_restart_renews_from_the_stored_refresh_entry();
	test_a_restart_with_an_empty_slot_asks_for_re_enrollment();
	test_refresh_material_waits_for_its_authority_and_is_stored_once_bound();
	test_pending_refresh_material_is_erased_by_shutdown();
	test_a_second_answer_for_a_resolved_renewal_rotates_nothing();
	test_shutdown_during_a_renewal_cancels_it_and_rotates_nothing();
	test_an_answer_with_no_refresh_material_schedules_no_renewal();

	std::cout << "device-authorization-refresh-tests:pass\n";
	return EXIT_SUCCESS;
}
