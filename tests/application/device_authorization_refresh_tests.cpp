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

	// [[BPR-0550]]. The same profile, a different account. That is the whole
	// distance between a second grant that rekeys nothing and one whose renewal
	// chain is filed under an account it never authenticated as.
	[[nodiscard]] auto second_account_authority()
		-> application::github_credential_authority_key
	{
		auto key    = authority();
		key.Account = core::github_account_id{ 11U };
		return key;
	}

	// [[BPR-0546]]. The generation the GitHub host's accepted session runs
	// under. Composition reads it from that host and carries it in with the
	// authority, because the device host has no way to know it.
	[[nodiscard]] auto accepted_generation() -> core::github_identity_generation
	{
		return core::github_identity_generation{ 3U };
	}

	// Exactly what `initial_device_authorization_state()` in the composition
	// seeds: an application session, a first request, and an identity
	// generation of zero, because no identity exists when the device host is
	// constructed and nothing ever advances this copy of it.
	[[nodiscard]] auto composed_initial_state()
		-> core::device_authorization_state
	{
		return core::device_authorization_state{
			.ApplicationSession = core::application_session_id{ 1U },
			.NextRequest        = core::request_id{ 1U },
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

		void publish_refresh(
			const application::device_authorization_refresh_observation&
				aObservation) override
		{
			Refreshes.push_back(aObservation);
		}

		std::vector<application::device_authorization_presentation> Published;
		std::vector<application::device_authorization_refresh_observation>
			Refreshes;
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
			// [[BPR-0546]], on [[BPR-0548]]'s rule. Every platform vault checks
			// `valid()` on the enrollment before it queues anything and
			// answers `invalid_request` otherwise, so a double that skips the
			// check passes for contexts no shipped vault would accept -- which
			// is exactly how a device host minting identity generation zero
			// stayed green here while the composed product stored nothing.
			if (!application::valid(aEnrollment)) {
				const auto refused = aEnrollment.Context;
				aEnrollment.Credential.reset();
				Order->emplace_back("refuse-store-refresh");
				aCompletion(application::github_credential_store_error{
					.Context = refused,
					.Failure = application::github_credential_vault_failure::
						invalid_request,
				});
				return;
			}
			Order->emplace_back("store-refresh");
			const auto context = aEnrollment.Context;
			StoreContexts.push_back(context);
			if (HoldStore) {
				// The slot has taken the material and has not answered yet.
				// Whatever the host does next, it does with a store
				// outstanding, which is the only way to test what a late
				// answer does.
				Stored = text_of(aEnrollment.Credential.bytes());
				aEnrollment.Credential.reset();
				HeldStore        = std::move(aCompletion);
				HeldStoreContext = context;
				return;
			}
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
			if (!application::valid(aContext)) {
				Order->emplace_back("refuse-retrieve-refresh");
				aCompletion(application::github_credential_retrieve_error{
					.Context = aContext,
					.Failure = application::github_credential_vault_failure::
						invalid_request,
				});
				return;
			}
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
			if (!application::valid(aContext)) {
				Order->emplace_back("refuse-delete-refresh");
				aCompletion(application::github_credential_delete_error{
					.Context = aContext,
					.Failure = application::github_credential_vault_failure::
						invalid_request,
				});
				return;
			}
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

		// Answers a store that was held, either way. The host has to survive
		// both, and after a cancel neither may change anything.
		void release_held_store(const bool aStored)
		{
			auto held = std::exchange(
				HeldStore,
				application::github_credential_store_completion_handler{ });
			expect(held != nullptr, "a store was held");
			if (!aStored) {
				Stored.reset();
				held(application::github_credential_store_error{
					.Context = HeldStoreContext,
					.Failure = application::github_credential_vault_failure::
						unavailable,
				});
				return;
			}
			held(application::github_credential_store_success{
				.Context     = HeldStoreContext,
				.Disposition = application::
					github_credential_store_disposition::stored,
			});
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
		application::github_credential_store_completion_handler       HeldStore;
		application::github_credential_operation_context              HeldStoreContext;
		bool                                                          RefuseStore    = false;
		bool                                                          RefuseRetrieve = false;
		bool                                                          HoldRetrieve   = false;
		bool                                                          HoldStore      = false;
	};

	struct harness
	{
		explicit harness(
			core::device_authorization_state aState = initial_state())
			: Transport{ Order }
			, Vault{ Order }
			, Enrollment{ Order }
			, Host{ std::move(aState), Transport, Vault, Enrollment,
					Clock, Timer, Lane, Projections,
					application::device_authorization_host_limits{ },
					application::device_authorization_browser_handoff{ },
					// These courses are about renewal, which only a registered
					// build ever reaches, and they begin for the profile the
					// capability was composed for ([[BPR-0535]]).
					application::device_authorization_capability{
						.Registered = true, .Profile = profile() } }
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
		// authority bound before the grant is answered, which is the shape a
		// reviewer whose session is already established authorizes in: the slot
		// exists while the flow runs, so the material is durable before the
		// access token is enrolled.
		//
		// [[BPR-0550]]. Bound inside the flow rather than ahead of it. A binding
		// made before `begin` belongs to whatever session preceded this flow, and
		// the host releases it there; only one this flow ran under is spent.
		void authorize_with_bound_authority(
			const std::uint32_t aLifetime = 3'600U)
		{
			reach_awaiting();
			poll_once();
			Host.bind_refresh_authority(authority(), accepted_generation());
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

		world.Host.resume_refresh(authority(), accepted_generation());
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

		world.Host.resume_refresh(authority(), accepted_generation());
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

		world.Host.bind_refresh_authority(authority(), accepted_generation());
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

		world.Host.bind_refresh_authority(authority(), accepted_generation());
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
		world.reach_awaiting();
		world.poll_once();
		// [[BPR-0550]]. Bound inside the flow. A binding made before `begin`
		// belongs to whatever session preceded it, and the host releases it
		// there rather than storing this grant's material under it.
		world.Host.bind_refresh_authority(authority(), accepted_generation());
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
	// [[BPR-0285]] residual R1. The store is submitted inside the flow and its
	// answer is marshalled onto the host's lane, so a cancel that arrives
	// first is a cancel that arrives while the slot is still outstanding.
	// This is the shape a reviewer produces by pressing Cancel during a
	// re-authorization.
	void test_a_store_that_answers_after_a_cancel_arms_no_renewal()
	{
		harness world;
		world.Vault.HoldStore = true;
		world.reach_awaiting();
		world.poll_once();
		// [[BPR-0550]]. Bound inside the flow. A binding made before `begin`
		// belongs to whatever session preceded it, and the host releases it
		// there rather than storing this grant's material under it.
		world.Host.bind_refresh_authority(authority(), accepted_generation());
		world.Transport.answer_token(world.issued());
		world.Lane.drain();
		expect(
			!world.Vault.StoreContexts.empty(),
			"the refresh entry is being written");
		expect(
			world.Host.state().Stage
				!= core::device_authorization_stage::authorized,
			"and the flow has not finished, because the entry is not durable");

		world.Host.dispatch(core::cancel_device_authorization_intent{ });
		expect(
			!world.Vault.Cancellations.empty(),
			"the cancel released the outstanding refresh slot");
		world.Vault.release_held_store(true);
		world.Lane.drain();

		expect(
			world.Host.state().Stage
				== core::device_authorization_stage::cancelled,
			"the flow is cancelled");
		expect(
			!world.Timer.armed(),
			"a store that answered after the cancel arms no renewal");
		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::idle,
			"and renewal stays idle");
		expect(
			world.Host.refresh_observation().Rotations == 0U,
			"nothing rotated");
		expect(
			world.Enrollment.Credentials.empty(),
			"and no access token was enrolled from a cancelled flow");
	}

	// The other half of R1. A refused store after a cancel used to tell the
	// reviewer to authorize again, which is an instruction about a flow they
	// had just stopped.
	void test_a_refused_store_after_a_cancel_asks_for_nothing()
	{
		harness world;
		world.Vault.HoldStore = true;
		world.reach_awaiting();
		world.poll_once();
		// [[BPR-0550]]. Bound inside the flow. A binding made before `begin`
		// belongs to whatever session preceded it, and the host releases it
		// there rather than storing this grant's material under it.
		world.Host.bind_refresh_authority(authority(), accepted_generation());
		world.Transport.answer_token(world.issued());
		world.Lane.drain();

		world.Host.dispatch(core::cancel_device_authorization_intent{ });
		world.Vault.release_held_store(false);
		world.Lane.drain();

		expect(
			world.Host.state().Stage
				== core::device_authorization_stage::cancelled,
			"the flow is cancelled rather than failed");
		expect(
			world.Host.refresh_observation().State
				!= application::device_authorization_refresh_state::
					reenrollment_required,
			"a store refused after a cancel asks for no re-enrollment");
		expect(
			world.Host.refresh_observation().Failure
				== application::device_authorization_refresh_failure::none,
			"and reports no renewal failure");
		expect(!world.Timer.armed(), "and arms nothing");
	}

	// The lifecycle has to leave the host to be worth anything: every state a
	// reviewer could act on is one presentation was told about.
	// [[BPR-0546]]. The composed device host is constructed from
	// `initial_device_authorization_state()`, whose identity generation is zero
	// -- no identity exists when it is built -- and nothing ever advances it.
	// Before the fix every vault context this host minted carried that zero,
	// which `valid()` refuses, so `store_refresh` came back `invalid_request`,
	// `on_refresh_stored` mapped it to `fail_storage`, and a real sign-in ended
	// in `reenrollment_required(storage_failed)` moments after it succeeded.
	// The generation the binding carries is what makes the slot reachable.
	void test_a_composed_device_host_stores_under_a_context_the_vault_accepts()
	{
		harness world{ composed_initial_state() };
		world.authorize_with_bound_authority();

		expect(
			world.Host.state().Stage
				== core::device_authorization_stage::authorized,
			"the flow authorizes rather than failing on the store");
		expect(
			world.Host.state().IdentityGeneration.value() == 0U,
			"with the device flow's own generation still the seeded zero");
		expect(
			world.Vault.StoreContexts.size() == 1U,
			"one store reached the vault");
		expect(
			application::valid(world.Vault.StoreContexts.front()),
			"under a context every platform vault accepts");
		expect(
			world.Vault.StoreContexts.front().IdentityGeneration
				== accepted_generation(),
			"carrying the accepted session's identity generation");
		expect(
			world.Vault.Stored.has_value() && *world.Vault.Stored == "refresh-1",
			"and the refresh material is in the slot");
		const auto expected = order_log{ "store-refresh", "enroll" };
		expect(world.Order == expected, "with nothing refused on the way");
		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::scheduled,
			"and renewal is armed rather than asking for re-enrollment");
	}

	void test_the_renewal_lifecycle_reaches_presentation()
	{
		harness world;
		world.authorize_with_bound_authority();
		expect(
			!world.Projections.Refreshes.empty(),
			"renewal was published at least once");
		expect(
			world.Projections.Refreshes.back()
				== world.Host.refresh_observation(),
			"and what presentation holds is what the host holds");
		expect(
			world.Projections.Refreshes.back().State
				== application::device_authorization_refresh_state::scheduled,
			"a renewal armed is a renewal presentation knows about");

		const auto published = world.Projections.Refreshes.size();
		world.Host.bind_refresh_authority(authority(), accepted_generation());
		world.Lane.drain();
		expect(
			world.Projections.Refreshes.size() == published,
			"and an unchanged observation publishes nothing");
	}
	// [[BPR-0550]]. A binding belongs to the session that named it. A second
	// grant in one process authenticates as whoever authorized it, which need
	// not be the account the first one bound. Material stored under the stale
	// key is what the incoming session's mismatch window erases ([[BPR-0549]]),
	// and by then the host has called it durable and armed a renewal against
	// it: the panel reads `scheduled` for a chain nobody can spend, and at the
	// lead time the retrieve answers absent and the reviewer is told to enroll
	// again while the token they hold still works.
	void test_a_second_grant_stores_under_the_authority_its_own_session_named()
	{
		harness world;
		world.authorize_with_bound_authority();
		expect(
			world.Vault.StoreContexts.size() == 1U
				&& world.Vault.StoreContexts.back().Authority == authority(),
			"the first grant stored under the authority its own session named");

		world.reach_awaiting();
		world.poll_once();
		world.Transport.answer_token(
			world.issued(3'600U, "access-2", "refresh-2"));
		world.Lane.drain();

		expect(
			world.Host.holds_pending_refresh_material(),
			"the second grant's material waits for its own authority");
		expect(
			world.Vault.StoreContexts.size() == 1U,
			"and nothing was written under the previous session's");

		world.Host.bind_refresh_authority(
			second_account_authority(),
			accepted_generation());
		world.Lane.drain();

		expect(
			world.Vault.StoreContexts.size() == 2U,
			"the new session's binding is what stores it");
		expect(
			world.Vault.StoreContexts.back().Authority
				== second_account_authority(),
			"under the authority the second session named, not the first");
		expect(
			world.Vault.Stored.has_value()
				&& *world.Vault.Stored == "refresh-2",
			"and what the slot holds is the second grant's material");
	}

	// [[BPR-0553]]. The panel offers *Get a code* in every terminal stage,
	// `authorized` among them, and the core accepts a begin from there. A
	// reviewer with a live session who presses it and then abandons, denies, or
	// fails that flow has changed nothing about the session they still hold:
	// the renewal armed for it is the only thing between their access token and
	// an expiry with nothing to renew it, and a flow that produced no grant has
	// no claim on it. Released at the begin, the press alone dropped it, and
	// the observation read `idle` rather than `reenrollment_required`, so
	// nothing was said until the token expired hours later.
	void test_a_second_flow_that_fails_leaves_the_first_renewal_armed()
	{
		harness world;
		world.authorize_with_bound_authority(3'600U);
		expect(
			world.Timer.armed() && world.Timer.last_delay() == 3'300U,
			"the first session's renewal is armed");

		world.begin();
		world.Transport.answer_code(
			application::device_authorization_transport_error{
				.Context = world.flow_context(),
				.Failure = application::device_authorization_transport_failure::
					network,
			});
		world.Lane.drain();

		expect(
			world.Host.state().Stage
				== core::device_authorization_stage::failed,
			"the second flow failed");
		expect(
			world.Timer.armed() && world.Timer.last_delay() == 3'300U,
			"and the renewal is the same one, on the same timer");
		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::scheduled,
			"the observation says so unchanged");

		// A binding that survived on paper is worth nothing. This is the one
		// that matters: the renewal can still be spent, under the slot the
		// first session named.
		world.Clock.advance(world.Timer.last_delay());
		world.Timer.fire();
		world.Lane.drain();
		expect(
			world.Vault.RetrieveContexts.size() == 1U
				&& world.Vault.RetrieveContexts.back().Authority == authority(),
			"and it renews under the authority the first session named");
	}

	// [[BPR-0553]]. The same abandonment, carried far enough that the flow took
	// the timer. One timer serves both purposes, so the poll displaces the
	// renewal for as long as the flow runs; a flow that then ends without a
	// grant no longer needs it, and an observation still reading `scheduled`
	// with nothing armed behind it would be the same silent drop by another
	// route.
	void test_a_denied_second_flow_gets_the_first_renewal_back_on_the_timer()
	{
		harness world;
		world.authorize_with_bound_authority(3'600U);

		world.reach_awaiting();
		world.poll_once();
		world.Transport.answer_token(application::device_token_refused{
			.Context = world.flow_context(),
			.Reason  = application::device_token_refusal_reason::access_denied,
		});
		world.Lane.drain();

		expect(
			world.Host.state().Stage
				== core::device_authorization_stage::denied,
			"the reviewer denied the second flow");
		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::scheduled,
			"the first session's renewal is still what the host reports");
		expect(
			world.Timer.armed() && world.Timer.last_delay() == 3'295U,
			"and it is armed again, late by exactly the time the flow ran");

		world.Clock.advance(world.Timer.last_delay());
		world.Timer.fire();
		world.Lane.drain();
		expect(
			world.Vault.RetrieveContexts.size() == 1U
				&& world.Vault.RetrieveContexts.back().Authority == authority(),
			"the renewal reads the slot the first session named");

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
		expect(
			world.Enrollment.Credentials
				== std::vector<std::string>{ "access-1", "access-2" },
			"and the session the reviewer still holds is renewed");
	}
	// [[BPR-0554]]. The half [[BPR-0553]] left behind. A device code lives 900
	// seconds and a renewal is armed 300 seconds before the access token
	// expires, so a renewal falling due while a second flow polls is the
	// ordinary case rather than a corner. Restored at the flow's terminal it
	// came back late by however long the flow ran, and the token it was there
	// to protect could expire first. The host holds both deadlines now, and the
	// timer holds whichever is earlier: a renewal due before the next poll is
	// not queued behind it.
	void test_a_renewal_due_during_a_second_flow_fires_on_time()
	{
		harness world;
		world.authorize_with_bound_authority(3'600U);
		expect(
			world.Timer.armed() && world.Timer.last_delay() == 3'300U,
			"the first session's renewal is armed");

		// To within two seconds of it, and then the reviewer presses *Get a
		// code* again, which the panel offers in every terminal stage.
		world.Clock.advance(3'298U);
		world.reach_awaiting();

		expect(
			world.Host.state().Stage
				== core::device_authorization_stage::awaiting_authorization,
			"a second flow is polling");
		expect(
			world.Timer.armed() && world.Timer.last_delay() == 3'300U,
			"the timer holds the renewal, not the poll behind it");

		world.Clock.advance(2U);
		world.Timer.fire();
		world.Lane.drain();

		expect(
			world.Vault.RetrieveContexts.size() == 1U
				&& world.Vault.RetrieveContexts.back().Authority == authority(),
			"the renewal happens at the second it was due");
		expect(
			world.Host.state().Stage
				== core::device_authorization_stage::awaiting_authorization,
			"and the flow it ran alongside is untouched");
		expect(
			world.Timer.armed() && world.Timer.last_delay() == 3U,
			"with the poll back on for its last three seconds");

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
		expect(
			world.Enrollment.Credentials
				== std::vector<std::string>{ "access-1", "access-2" },
			"and the token it was racing is renewed in time");
	}

	// [[BPR-0554]]. The transient dishonesty the same single timer produced.
	// The poll spends the timer the moment it fires, and for as long as the
	// request it sent is outstanding the observation reads `scheduled` with
	// nothing behind it at all -- an answer presentation shows the reviewer,
	// and one no reviewer can act on, because there is nothing to act on. The
	// renewal's deadline is not the timer's to spend, so it is armed the
	// instant the poll's is spent.
	void test_the_observation_never_says_scheduled_with_nothing_armed()
	{
		harness world;
		world.authorize_with_bound_authority(3'600U);
		expect(
			world.Host.refresh_observation().State
					== application::device_authorization_refresh_state::scheduled
				&& world.Timer.armed(),
			"the session renews and the timer says so");

		world.reach_awaiting();
		world.poll_once();

		expect(
			world.Host.refresh_observation().State
				== application::device_authorization_refresh_state::scheduled,
			"the observation still calls the renewal scheduled");
		expect(
			world.Timer.armed(),
			"and one it calls scheduled is one the timer holds");
		expect(
			world.Timer.last_delay() == 3'295U,
			"for exactly what is left of the deadline it named");

		world.Clock.advance(world.Timer.last_delay());
		world.Timer.fire();
		world.Lane.drain();
		expect(
			world.Vault.RetrieveContexts.size() == 1U
				&& world.Vault.RetrieveContexts.back().Authority == authority(),
			"and firing it renews under the first session's slot");
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
	test_a_store_that_answers_after_a_cancel_arms_no_renewal();
	test_a_refused_store_after_a_cancel_asks_for_nothing();
	test_the_renewal_lifecycle_reaches_presentation();
	test_a_composed_device_host_stores_under_a_context_the_vault_accepts();
	test_a_second_grant_stores_under_the_authority_its_own_session_named();
	test_a_second_flow_that_fails_leaves_the_first_renewal_armed();
	test_a_denied_second_flow_gets_the_first_renewal_back_on_the_timer();
	test_a_renewal_due_during_a_second_flow_fires_on_time();
	test_the_observation_never_says_scheduled_with_nothing_armed();

	std::cout << "device-authorization-refresh-tests:pass\n";
	return EXIT_SUCCESS;
}
