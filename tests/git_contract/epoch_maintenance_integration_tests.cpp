#include "../sqlite_contract/epoch_catalog_legacy_fixture.hpp"
#include "../sqlite_contract/platform_test_support.hpp"
#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/agent/agent_process_launcher.hpp"
#include "bpr/adapters/git/installed_git_cache.hpp"
#include "bpr/adapters/mcp/mcp_context_helper.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/insights_storage.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/testing.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "windows_system_root.hpp"

#include <sqlite3.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	namespace application    = bpr::application;
	namespace core           = bpr::core;
	namespace git_adapter    = bpr::adapters::git;
	namespace sqlite_adapter = bpr::adapters::sqlite;

	inline constexpr std::string_view               commit_hex = "826bb464b84352d60b3d7c56481a599a838d998d";
	inline constexpr std::array<std::uint8_t, 230U> pack_bytes{
		0x50,
		0x41,
		0x43,
		0x4b,
		0x00,
		0x00,
		0x00,
		0x02,
		0x00,
		0x00,
		0x00,
		0x03,
		0x97,
		0x0b,
		0x78,
		0x9c,
		0x95,
		0xcb,
		0xc1,
		0x0a,
		0x02,
		0x21,
		0x10,
		0x80,
		0xe1,
		0xbb,
		0x4f,
		0x31,
		0xf7,
		0x20,
		0x74,
		0x35,
		0xdb,
		0x85,
		0x58,
		0xa2,
		0x4b,
		0xd7,
		0xe8,
		0x0d,
		0x46,
		0x1d,
		0x69,
		0xc0,
		0x5d,
		0xc5,
		0xa6,
		0xa8,
		0xb7,
		0xaf,
		0x1e,
		0xa1,
		0xff,
		0xfc,
		0xfd,
		0xd2,
		0x89,
		0xc0,
		0x45,
		0x9a,
		0x72,
		0x0e,
		0x79,
		0x1f,
		0x63,
		0xb2,
		0x26,
		0x51,
		0x08,
		0x1a,
		0xad,
		0xb3,
		0x7a,
		0x18,
		0x8c,
		0x35,
		0x59,
		0xe7,
		0x44,
		0xde,
		0x91,
		0xdd,
		0x25,
		0x85,
		0x0f,
		0xb9,
		0xd5,
		0x0e,
		0xa7,
		0xcb,
		0x15,
		0x0e,
		0xa1,
		0xf5,
		0x23,
		0xbd,
		0x70,
		0x69,
		0x85,
		0xb6,
		0xbc,
		0x3e,
		0xb1,
		0x70,
		0x9a,
		0x61,
		0x72,
		0xde,
		0x8f,
		0x6e,
		0xd4,
		0x1a,
		0x36,
		0xfa,
		0x9b,
		0x8a,
		0x75,
		0x59,
		0x58,
		0x84,
		0xfe,
		0x79,
		0xd4,
		0x8f,
		0xf2,
		0x7a,
		0x17,
		0x2c,
		0x85,
		0x12,
		0x9c,
		0x59,
		0x20,
		0x62,
		0xc3,
		0xc0,
		0x85,
		0xe5,
		0x0d,
		0xad,
		0xd7,
		0x40,
		0xea,
		0x03,
		0x67,
		0xd5,
		0x39,
		0x11,
		0xa5,
		0x02,
		0x78,
		0x9c,
		0x33,
		0x34,
		0x30,
		0x30,
		0x33,
		0x31,
		0x51,
		0x28,
		0x28,
		0xca,
		0x4f,
		0x4a,
		0xd5,
		0x2b,
		0xa9,
		0x28,
		0x61,
		0xb8,
		0xc5,
		0xe3,
		0xb7,
		0xe3,
		0xe6,
		0x92,
		0x5e,
		0x71,
		0x29,
		0xe3,
		0x70,
		0x6f,
		0x0b,
		0xf6,
		0xa0,
		0x87,
		0xcd,
		0x1a,
		0xe9,
		0x81,
		0x00,
		0xe4,
		0xb2,
		0x0c,
		0xc7,
		0x36,
		0x78,
		0x9c,
		0x2b,
		0x28,
		0xca,
		0x4f,
		0x4a,
		0xe5,
		0x02,
		0x00,
		0x08,
		0x96,
		0x02,
		0x23,
		0xe2,
		0x48,
		0xb0,
		0x8f,
		0x09,
		0xa7,
		0x51,
		0x16,
		0xfd,
		0xb2,
		0x39,
		0x7c,
		0x92,
		0x8a,
		0x84,
		0xf6,
		0xa9,
		0x3f,
		0x6c,
		0x34,
	};

	auto require(const bool aCondition, const std::string_view aMessage) -> void
	{
		bpr::tests::expect(aCondition, aMessage);
	}

	template <typename Identity>
	[[nodiscard]] auto identity(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xa5U);
		return Identity{ bytes };
	}

	[[nodiscard]] auto bytes(const std::string_view aText) -> std::vector<std::byte>
	{
		std::vector<std::byte> output;
		output.reserve(aText.size());
		for (const auto character : aText) {
			output.push_back(
				static_cast<std::byte>(static_cast<unsigned char>(character)));
		}
		return output;
	}

	[[nodiscard]] auto lower_hex(const std::span<const std::uint8_t> aValue)
		-> std::string
	{
		static constexpr std::string_view digits = "0123456789abcdef";
		std::string                       output;
		output.reserve(aValue.size() * 2U);
		for (const auto byte : aValue) {
			output.push_back(digits[(static_cast<unsigned>(byte) >> 4U) & 0x0fU]);
			output.push_back(digits[byte & 0x0fU]);
		}
		return output;
	}

	[[nodiscard]] auto packet(const std::span<const std::byte> aPayload)
		-> std::vector<std::byte>
	{
		static constexpr std::string_view digits = "0123456789abcdef";
		const auto                        length = aPayload.size() + 4U;
		require(length <= 0xffffU, "fixture packet is bounded");
		std::vector<std::byte> output{
			static_cast<std::byte>(digits[(length >> 12U) & 0xfU]),
			static_cast<std::byte>(digits[(length >> 8U) & 0xfU]),
			static_cast<std::byte>(digits[(length >> 4U) & 0xfU]),
			static_cast<std::byte>(digits[length & 0xfU]),
		};
		output.insert(output.end(), aPayload.begin(), aPayload.end());
		return output;
	}

	[[nodiscard]] auto packet(const std::string_view aPayload)
		-> std::vector<std::byte>
	{
		const auto value = bytes(aPayload);
		return packet(value);
	}

	auto append(std::vector<std::byte>& aDestination, std::vector<std::byte> aSource)
		-> void
	{
		aDestination.insert(aDestination.end(), aSource.begin(), aSource.end());
	}

	class fixture_transport final : public application::git_smart_transport_port
	{
	public:
		explicit fixture_transport(application::git_smart_request_context aContext, std::function<void()> aDuringFetch = { })
			: Context{ std::move(aContext) }
			, DuringFetch{ std::move(aDuringFetch) }
		{
		}

		void revoke() noexcept override { Revoked = true; }

		[[nodiscard]] auto revoked() const noexcept -> bool override
		{
			return Revoked;
		}

		[[nodiscard]] auto discover(
			const application::git_smart_discovery_request& aRequest,
			const std::stop_token                           aStop)
			-> application::git_smart_transport_result<
				application::git_smart_discovery_response> override
		{
			if (aStop.stop_requested() || aRequest.Context != Context) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::cancelled,
				} };
			}
			auto body = packet("# service=git-upload-pack\n");
			append(body, bytes("0000"));
			append(body, packet("version 2\n"));
			append(body, packet("ls-refs=unborn\n"));
			append(body, packet("fetch=shallow wait-for-done\n"));
			append(body, packet("object-format=sha1\n"));
			append(body, packet("server-option\n"));
			append(body, bytes("0000"));
			return application::git_smart_discovery_response{
				.Body = std::move(body),
			};
		}

		[[nodiscard]] auto ls_refs(
			const application::git_smart_ls_refs_request& aRequest,
			const std::stop_token                         aStop)
			-> application::git_smart_transport_result<
				application::git_smart_ls_refs_response> override
		{
			if (aStop.stop_requested() || aRequest.Context != Context) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::cancelled,
				} };
			}
			auto body = packet(std::string{ commit_hex } + " " + std::string{ Context.BaseRef.view() } + "\n");
			append(body, packet(std::string{ commit_hex } + " refs/pull/81/head\n"));
			append(body, bytes("0000"));
			return application::git_smart_ls_refs_response{
				.Body = std::move(body),
			};
		}

		[[nodiscard]] auto fetch(
			const application::git_smart_fetch_request& aRequest,
			application::git_smart_wire_consumer&       aConsumer,
			const std::stop_token                       aStop)
			-> application::git_smart_transport_result<void> override
		{
			if (aStop.stop_requested() || aRequest.Context != Context) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::cancelled,
				} };
			}
			if (DuringFetch) {
				auto                   partial = packet("packfile\n");
				std::vector<std::byte> chunk{ static_cast<std::byte>(1U) };
				for (std::size_t index = 0; index < pack_bytes.size() / 2U; ++index) {
					chunk.push_back(static_cast<std::byte>(pack_bytes[index]));
				}
				append(partial, packet(chunk));
				require(aConsumer.consume(partial), "partial pack reaches production spool consumer");
				DuringFetch();
				require(false, "partial-fetch checkpoint must kill the process");
			}
			std::vector<std::byte> payload;
			payload.reserve(pack_bytes.size() + 1U);
			payload.push_back(static_cast<std::byte>(1U));
			for (const auto byte : pack_bytes) {
				payload.push_back(static_cast<std::byte>(byte));
			}
			auto body = packet("packfile\n");
			append(body, packet(payload));
			append(body, bytes("0000"));
			if (!aConsumer.consume(body)) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::consumer_rejected,
				} };
			}
			return { };
		}

	private:
		application::git_smart_request_context Context;
		std::function<void()>                  DuringFetch;
		bool                                   Revoked = false;
	};

	class fixture_authority final : public application::git_smart_authority_lease
	{
	public:
		explicit fixture_authority(application::git_smart_authority_binding aBinding, std::function<void()> aDuringFetch = { })
			: Binding{ std::move(aBinding) }
			, DuringFetch{ std::move(aDuringFetch) }
		{
		}

		[[nodiscard]] auto binding() const noexcept
			-> const application::git_smart_authority_binding& override
		{
			return Binding;
		}

		[[nodiscard]] auto acquire(
			const application::git_smart_request_context& aContext)
			-> application::git_smart_transport_result<
				application::git_smart_transport> override
		{
			if (Revoked) {
				return std::unexpected{ application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::cancelled,
				} };
			}
			return std::make_shared<fixture_transport>(aContext, DuringFetch);
		}

		void revoke() noexcept override { Revoked = true; }

		[[nodiscard]] auto revoked() const noexcept -> bool override
		{
			return Revoked;
		}

	private:
		application::git_smart_authority_binding Binding;
		std::function<void()>                    DuringFetch;
		bool                                     Revoked = false;
	};

	class queued_executor final : public application::effect_executor,
								  public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			Tasks.push_back(std::move(aTask));
		}

		[[nodiscard]] auto empty() const noexcept -> bool { return Tasks.empty(); }

		void run_next()
		{
			require(!Tasks.empty(), "executor has a queued task");
			auto task = std::move(Tasks.front());
			Tasks.pop_front();
			task();
		}

	private:
		std::deque<std::function<void()>> Tasks;
	};

	class deterministic_entropy final : public application::epoch_entropy_port
	{
	public:
		[[nodiscard]] auto generate() noexcept
			-> std::optional<application::epoch_entropy_bytes> override
		{
			std::array<std::uint8_t, 32> result{ };
			result.front() = Next++;
			result.back()  = 0xa5U;
			return result;
		}

	private:
		std::uint8_t Next = 1U;
	};

	class deterministic_clock final : public application::epoch_clock_port
	{
	public:
		[[nodiscard]] auto observe_utc() noexcept
			-> std::optional<core::observed_utc_milliseconds> override
		{
			return core::observed_utc_milliseconds{ Next++ };
		}

	private:
		std::int64_t Next = 1'000;
	};

	class capturing_projection final : public application::epoch_projection_port
	{
	public:
		void publish(const application::epoch_presentation_update& aUpdate) override
		{
			Updates.push_back(aUpdate);
			if (AfterPublish) {
				AfterPublish(aUpdate);
			}
		}

		std::vector<application::epoch_presentation_update>                Updates;
		std::function<void(const application::epoch_presentation_update&)> AfterPublish;
	};

	class temporary_root final
	{
	public:
		explicit temporary_root(const std::string_view aLabel, std::optional<std::filesystem::path> aExisting = { })
			: Owned{ !aExisting }
		{
			// [[BPR-1128]]. A root made here is made at a name nothing had, mode
			// `0700` from creation, and owned from then, so a failure below
			// removes it. An existing one is left, as its owner is.
			Path      = aExisting ? *aExisting : bpr::tests::make_scratch_directory(std::string{ "bpr-maintenance-" } + std::string{ aLabel });
			auto made = bpr::tests::scratch_guard{ Owned ? Path : std::filesystem::path{ } };
			std::filesystem::create_directories(Path / "git");
			std::filesystem::create_directories(Path / "catalog");
			std::filesystem::permissions(Path,
										 std::filesystem::perms::owner_all,
										 std::filesystem::perm_options::replace);
			std::filesystem::permissions(Path / "git",
										 std::filesystem::perms::owner_all,
										 std::filesystem::perm_options::replace);
			std::filesystem::permissions(Path / "catalog",
										 std::filesystem::perms::owner_all,
										 std::filesystem::perm_options::replace);
			// Mode 0700 is what POSIX understands; the catalog's own
			// definition of a private root is what Windows needs (BPR-0436).
			require(sqlite_adapter::make_storage_root_owner_private(Path / "git"),
					"narrow the Git root to owner-private");
			require(sqlite_adapter::make_storage_root_owner_private(Path / "catalog"),
					"narrow the catalog root to owner-private");
			Path = std::filesystem::canonical(Path);
			made.release();
		}

		~temporary_root()
		{
			std::error_code error;
			if (Owned) {
				std::filesystem::remove_all(Path, error);
			}
		}

		temporary_root(const temporary_root&)                    = delete;
		auto operator=(const temporary_root&) -> temporary_root& = delete;

		std::filesystem::path Path;
		bool                  Owned;
	};

	[[nodiscard]] auto make_handoff(std::function<void()> aDuringFetch = { }, bool aRetarget = false) -> application::accepted_pull_request_handoff
	{
		const auto commit  = core::parse_git_commit_oid(commit_hex);
		const auto baseRef = application::parse_git_base_branch_ref(aRetarget ? "refs/heads/release" : "refs/heads/main");
		require(commit.has_value() && baseRef.has_value(),
				"fixture identities parse");
		auto handoff = application::accepted_pull_request_handoff{
			.ApplicationSession    = core::application_session_id{ 91U },
			.Profile               = core::host_profile_id{ 11U },
			.ProfileRevision       = core::github_profile_revision{ 21U },
			.Host                  = core::git_host_id{ 31U },
			.HostAuthority         = identity<core::host_authority_fingerprint>(2U),
			.Hostname              = application::github_canonical_hostname{ "github.com" },
			.Account               = core::github_account_id{ 41U },
			.Viewer                = core::github_viewer_actor_id{ 51U },
			.Repository            = core::repository_id{ 61U },
			.Locator               = { .Owner = "octo-org", .Name = "maintenance-fixture" },
			.PullRequest           = core::pull_request_id{ 71U },
			.Number                = core::pull_request_number{ 81U },
			.Snapshot              = core::provider_snapshot_id{ aRetarget ? 2U : 1U },
			.IdentityGeneration    = core::github_identity_generation{ 3U },
			.PullRequestGeneration = core::request_generation{ 4U },
			.Origin                = core::pull_request_revision_origin::live,
			.ObjectFormat          = core::git_hash_algorithm::sha1,
			.BaseBranch            = identity<core::base_branch_id>(aRetarget ? 31U : 30U),
			.BaseRef               = *baseRef,
			.Base                  = *commit,
			.Head                  = *commit,
		};
		handoff.SmartGit = std::make_unique<fixture_authority>(
			application::authority_binding(handoff), std::move(aDuringFetch));
		require(application::valid(handoff), "fixture handoff is valid");
		return handoff;
	}

	[[nodiscard]] auto catalog_key() -> application::epoch_catalog_authority_key
	{
		return {
			.Profile         = core::host_profile_id{ 11U },
			.ProfileRevision = core::github_profile_revision{ 21U },
			.Host            = core::git_host_id{ 31U },
			.HostAuthority   = identity<core::host_authority_fingerprint>(2U),
			.Account         = core::github_account_id{ 41U },
			.Viewer          = core::github_viewer_actor_id{ 51U },
			.Repository      = core::repository_id{ 61U },
			.PullRequest     = core::pull_request_id{ 71U },
			.Number          = core::pull_request_number{ 81U },
			.ObjectFormat    = core::git_hash_algorithm::sha1,
		};
	}

	struct real_fixture final
	{
		explicit real_fixture(const std::string_view aLabel, std::optional<std::filesystem::path> aExisting = { }, std::shared_ptr<application::diagnostic_log_port> aDiagnostics = { })
			: Root{ aLabel, std::move(aExisting) }
		{
			auto madeCache   = git_adapter::make_installed_git_cache({
				.GitExecutable     = std::filesystem::canonical(BPR_TEST_GIT_EXECUTABLE),
				.StorageRoot       = Root.Path / "git",
				.WindowsSystemRoot = bpr::tests::git_contract::windows_system_root(),
			});
			auto madeCatalog = sqlite_adapter::make_epoch_catalog({
				.StorageRoot = Root.Path / "catalog",
			});
			require(madeCache.has_value(), "construct real Git cache");
			require(madeCatalog.has_value(), "construct real SQLite catalog");
			Caches   = std::move(*madeCache);
			Catalogs = std::move(*madeCatalog);
			Host     = std::make_unique<application::epoch_application_host>(
				core::epoch_state{
					.ApplicationSession = core::application_session_id{ 91U },
				},
				*Caches,
				*Catalogs,
				Entropy,
				Clock,
				Effects,
				Decisions,
				Projection, application::epoch_host_limits{ }, std::move(aDiagnostics));
		}

		void drain()
		{
			std::size_t turns = 0U;
			while (!Effects.empty() || !Decisions.empty()) {
				require(++turns <= 100U, "integration host reaches quiescence");
				if (!Effects.empty()) {
					Effects.run_next();
				}
				if (!Decisions.empty()) {
					Decisions.run_next();
				}
			}
		}

		void start(std::function<void()> aDuringFetch = { })
		{
			Host->accept(make_handoff(std::move(aDuringFetch)));
			drain();
			require(Host->state().Catalog.has_value()
						&& Host->state().AcceptedComparison.has_value()
						&& Host->state().Status == core::epoch_status::ready,
					"real composition retains and resolves its initial epoch");
		}

		[[nodiscard]] auto inserted_epoch() const -> core::review_epoch_id
		{
			for (const auto& update : Projection.Updates) {
				if (!update.Catalog) {
					continue;
				}
				for (const auto& change : update.Catalog->Changes) {
					if (change.Kind == application::epoch_row_change_kind::insert) {
						return change.Id;
					}
				}
			}
			require(false, "initial epoch reaches the public projection");
			return { };
		}

		void close_host()
		{
			Host->shutdown();
			drain();
			require(Host->is_quiescent(),
					"integration host shuts down quiescently");
			Host.reset();
		}

		temporary_root                                       Root;
		std::unique_ptr<application::git_cache_port>         Caches;
		std::unique_ptr<application::epoch_catalog_port>     Catalogs;
		deterministic_entropy                                Entropy;
		deterministic_clock                                  Clock;
		queued_executor                                      Effects;
		queued_executor                                      Decisions;
		capturing_projection                                 Projection;
		std::unique_ptr<application::epoch_application_host> Host;
	};

	[[nodiscard]] auto exact_epoch_anchors(
		application::epoch_catalog_session& aCatalog,
		const core::review_epoch_id         aEpoch)
		-> application::exact_anchor_lookup_result
	{
		auto result = aCatalog.exact_anchors(
			application::exact_anchor_lookup_request{
				.Catalog = aCatalog.binding().Catalog,
				.Anchors = {
					core::comparison_anchor_identity{
						.Authority   = aEpoch.Authority,
						.Incarnation = aEpoch.Incarnation,
						.Kind        = core::comparison_anchor_kind::epoch_base,
						.Epoch       = aEpoch.Ordinal,
					},
					core::comparison_anchor_identity{
						.Authority   = aEpoch.Authority,
						.Incarnation = aEpoch.Incarnation,
						.Kind        = core::comparison_anchor_kind::epoch_head,
						.Epoch       = aEpoch.Ordinal,
					},
				},
			},
			{ });
		require(result.has_value(), "load exact durable anchors");
		return *result;
	}

	[[nodiscard]] auto epoch_identity(const core::review_epoch_id& aEpoch) -> std::string
	{
		return lower_hex(aEpoch.Authority.bytes()) + "/" + lower_hex(aEpoch.Incarnation.bytes()) + "/" + std::to_string(aEpoch.Ordinal.value());
	}
	void kill_checkpoint(const std::filesystem::path& aRoot, std::string_view aPhase)
	{
		std::ofstream marker{ aRoot / "kill-ready", std::ios::binary };
		marker << aPhase;
		marker.close();
		require(marker.good(), "kill readiness published");
		static_cast<void>(std::cin.get());
		require(false, "kill checkpoint never resumes or unwinds");
	}
	class checkpoint_diagnostics final : public application::diagnostic_log_port
	{
	public:
		std::function<void(const application::diagnostic_event&)> Observe;
		auto                                                      record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			if (Observe) {
				Observe(aEvent);
			}
			return application::diagnostic_log_status::available;
		}
		auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }
	};
	std::filesystem::path JournalCheckpointRoot;
	std::string_view      JournalCheckpointPhase;
	bool                  JournalCheckpointWithHistory = false;
	void                  observe_journal_checkpoint(sqlite_adapter::detail::testing::epoch_journal_stage aStage)
	{
		using enum sqlite_adapter::detail::testing::epoch_journal_stage;
		if ((aStage == receiving && JournalCheckpointPhase == "--kill-transaction-receiving")
			|| (aStage == spool && JournalCheckpointPhase == "--kill-transaction-spool")
			|| (aStage == installation && JournalCheckpointPhase == "--kill-transaction-install")) {
			kill_checkpoint(JournalCheckpointRoot, std::string{ JournalCheckpointPhase } + (JournalCheckpointWithHistory ? "-history" : ""));
		}
	}

	void kill_journal_transaction(std::string_view aPhase, const std::filesystem::path& aRoot)
	{
		JournalCheckpointWithHistory = aPhase.ends_with("-history");
		if (JournalCheckpointWithHistory) {
			aPhase.remove_suffix(std::string_view{ "-history" }.size());
		}
		require(aPhase == "--kill-transaction-receiving" || aPhase == "--kill-transaction-spool" || aPhase == "--kill-transaction-install",
				"known journal transaction kill course");
		JournalCheckpointRoot  = aRoot;
		JournalCheckpointPhase = aPhase;
		real_fixture value{ "transaction-kill", aRoot };
		if (JournalCheckpointWithHistory) {
			value.start();
			std::ofstream expected{ aRoot / "expected-visible", std::ios::binary };
			expected << epoch_identity(value.inserted_epoch());
			expected.close();
			require(expected.good(), "prior committed epoch identity recorded");
		}
		sqlite_adapter::detail::testing::set_epoch_journal_checkpoint(observe_journal_checkpoint);
		value.Host->accept(make_handoff({ }, JournalCheckpointWithHistory));
		value.drain();
		require(false, "journal must reach its precommit kill checkpoint");
	}

	void kill_epoch(std::string_view aPhase, const std::filesystem::path& aRoot)
	{
		require(aPhase == "--kill-fetch" || aPhase == "--kill-spool" || aPhase == "--kill-prepared" || aPhase == "--kill-visible", "known epoch kill course");
		auto diagnostics     = std::make_shared<checkpoint_diagnostics>();
		diagnostics->Observe = [&](const application::diagnostic_event& aEvent) {
			if (aEvent.Outcome == application::diagnostic_outcome::succeeded && ((aPhase == "--kill-spool" && aEvent.EpochStage == application::diagnostic_epoch_stage::protocol_acquire) || (aPhase == "--kill-prepared" && aEvent.EpochStage == application::diagnostic_epoch_stage::revision_prepare))) {
				kill_checkpoint(aRoot, aPhase);
			}
		};
		real_fixture value{ "kill", aRoot, diagnostics };
		if (aPhase == "--kill-visible") {
			value.Projection.AfterPublish = [&](const application::epoch_presentation_update& aUpdate) {
				if (!aUpdate.Catalog) {
					return;
				}
				for (const auto& change : aUpdate.Catalog->Changes) {
					if (change.Kind == application::epoch_row_change_kind::insert) {
						std::ofstream expected{ aRoot / "expected-visible", std::ios::binary };
						expected << epoch_identity(change.Id);
						expected.close();
						require(expected.good(), "visible epoch identity recorded before kill");
						kill_checkpoint(aRoot, aPhase);
					}
				}
			};
		}
		std::function<void()> duringFetch;
		if (aPhase == "--kill-fetch") {
			duringFetch = [&] { kill_checkpoint(aRoot, aPhase); };
		}
		value.start(std::move(duringFetch));
		require(false, "epoch must reach selected kill checkpoint");
	}
	void recover_epoch(const std::filesystem::path& aRoot)
	{
		real_fixture value{ "recover", aRoot };
		value.start();
		const auto authority = *value.Host->state().Authority;
		value.close_host();
		auto catalog = value.Catalogs->open({ .ApplicationSession = core::application_session_id{ 91U }, .Authority = catalog_key() }, { });
		require(catalog && *catalog, "production catalog reopens after killed-host recovery");
		const auto snapshot       = (*catalog)->snapshot({ });
		const auto expectedCommit = core::parse_git_commit_oid(commit_hex);
		require(expectedCommit && snapshot && snapshot->LatestLive && snapshot->LatestLive->Base == *expectedCommit && snapshot->LatestLive->Head == *expectedCommit, "recovery retains exact fixture commits");
		if (std::filesystem::exists(aRoot / "expected-visible")) {
			std::ifstream input{ aRoot / "expected-visible", std::ios::binary };
			std::string   expected;
			std::getline(input, expected);
			require(expected == epoch_identity(snapshot->LatestLive->Id), "visible epoch identity survives abrupt death without replacement");
		}

		const auto history = (*catalog)->history({ .Catalog = (*catalog)->binding().Catalog }, { });
		require(history && history->Epochs.size() == 1U && !history->Continuation && history->Epochs.front() == *snapshot->LatestLive, "restart retains exactly one visible epoch");
		const auto operations = (*catalog)->recovery_page({ }, application::maximum_epoch_recovery_page_size, { });
		require(operations && operations->Operations.empty() && !operations->Continuation, "normal host recovery leaves no unfinished operation");
		auto cache = value.Caches->open({ .ApplicationSession = core::application_session_id{ 91U }, .Authority = authority, .ObjectFormat = core::git_hash_algorithm::sha1 }, { });
		require(cache && *cache, "production cache reopens without stale lock");
		const auto retained = (*cache)->inspect_retained(snapshot->LatestLive->Id.Ordinal, { });
		require(retained && retained->Base == snapshot->LatestLive->Base && retained->Head == snapshot->LatestLive->Head, "retained Git ref pair matches the visible epoch");
		std::cout << "--recover-epoch:pass\n";
	}
	class legacy_catalog_copy final
	{
	public:
		explicit legacy_catalog_copy(const std::filesystem::path& aPath)
		{
			require(sqlite3_open_v2(aPath.string().c_str(), &Database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
					"open closed copied catalogue for exact legacy conversion");
		}
		~legacy_catalog_copy() { sqlite3_close(Database); }
		legacy_catalog_copy(const legacy_catalog_copy&)                    = delete;
		auto operator=(const legacy_catalog_copy&) -> legacy_catalog_copy& = delete;
		void execute(const char* aSql)
		{
			require(sqlite3_exec(Database, aSql, nullptr, nullptr, nullptr) == SQLITE_OK, "legacy fixture SQL succeeds");
		}
		[[nodiscard]] auto integer(const char* aSql) -> std::int64_t
		{
			sqlite3_stmt* statement = nullptr;
			require(sqlite3_prepare_v2(Database, aSql, -1, &statement, nullptr) == SQLITE_OK
						&& sqlite3_step(statement) == SQLITE_ROW,
					"legacy fixture scalar query succeeds");
			const auto result = sqlite3_column_int64(statement, 0);
			require(sqlite3_finalize(statement) == SQLITE_OK, "legacy fixture scalar query closes");
			return result;
		}

	private:
		sqlite3* Database = nullptr;
	};

	void verify_migrated_binding_consumers(const std::filesystem::path&        aRoot,
										   const core::agent_revision_binding& aBinding,
										   application::epoch_catalog_session& aCatalog, const core::review_epoch_id& aEpoch)
	{
		namespace mcp = bpr::adapters::mcp;
		mcp::mcp_context_helper helper{ aRoot, aBinding };
		const auto              response = helper.handle_line(R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"snapshot_binding","arguments":{}}})");
		require(response && response->find(mcp::encode_revision_binding(aBinding)) != std::string::npos
					&& response->find("error") == std::string::npos,
				"real MCP binding lookup reads the pre-migration snapshot with unchanged epoch references");
		const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(*response));
		const auto content  = document.object().value("result").toObject().value("content").toArray();
		require(content.size() == 1, "MCP lookup returns one binding text result");
		const auto returned = mcp::decode_revision_binding(content.at(0).toObject().value("text").toString().toStdString());
		require(returned && *returned == aBinding, "actual MCP response decodes the original frozen binding");
		auto leftEpoch     = aEpoch;
		leftEpoch.Ordinal  = returned->LeftEpoch;
		auto rightEpoch    = aEpoch;
		rightEpoch.Ordinal = returned->RightEpoch;
		const auto left    = exact_epoch_anchors(aCatalog, leftEpoch);
		const auto right   = exact_epoch_anchors(aCatalog, rightEpoch);
		require(left.Anchors.size() == 2U && right.Anchors.size() == 2U
					&& left.Anchors[0].Commit == returned->BaseOid && right.Anchors[1].Commit == returned->HeadOid,
				"epochs returned by MCP resolve their exact commits through the migrated catalogue");
		auto stale       = aBinding;
		stale.RightEpoch = core::epoch_ordinal{ aBinding.RightEpoch.value() + 1 };
		mcp::mcp_context_helper staleHelper{ aRoot, stale };
		const auto              rejected = staleHelper.handle_line(R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"snapshot_binding","arguments":{}}})");
		require(rejected && rejected->find("revision_binding_stale") != std::string::npos,
				"a changed ordinal cannot alias the original MCP binding");
		const auto supplement = application::build_agent_context_supplement(
			{ .ApplicationSession = core::application_session_id{ 91U }, .Binding = aBinding, .PullRequestUrl = "https://ghe.example.test/octo-org/maintenance-fixture/pull/81" },
			{ .Id = core::agent_session_id{ 1U }, .Profile = core::agent_profile_id{ 1U }, .Binding = aBinding });
		require(supplement.has_value(), "production agent context accepts the resolved pre-migration binding");
		const auto encoded = application::encode_agent_supplement(*supplement);
		require(encoded.find("left_epoch\t" + std::to_string(aBinding.LeftEpoch.value()) + "\n") != std::string::npos
					&& encoded.find("right_epoch\t" + std::to_string(aBinding.RightEpoch.value()) + "\n") != std::string::npos,
				"agent launch context encoding preserves original epoch references");
		const core::agent_profile profile{ .Id = core::agent_profile_id{ 1U }, .DisplayName = "Migration fixture", .Executable = BPR_MIGRATED_BINDING_AGENT_EXECUTABLE, .Protocol = core::agent_protocol::external_terminal, .Workspace = core::agent_workspace_policy::disposable_review_workspace };
		auto                      launch = application::build_agent_launch(
			profile,
			*supplement, [](const std::string&) -> std::optional<std::string> { return std::nullopt; });
		require(launch && launch->Input.find(encoded) != std::string::npos,
				"production launch request frames unchanged original epoch context");
		launch->WorkingDirectory = std::filesystem::canonical(aRoot).string();
		require(launch->Environment.empty(), "migration agent receives no inherited environment or credentials");
		bpr::adapters::agent::agent_process_launcher process;
		const auto                                   result  = process.run(*launch, 10000);
		const auto                                   receipt = "consumed " + std::to_string(aBinding.Host.value())
			+ " " + std::to_string(aBinding.Repository.value()) + " " + std::to_string(aBinding.PullRequest.value())
			+ " " + std::to_string(aBinding.LeftEpoch.value()) + " " + std::to_string(aBinding.RightEpoch.value())
			+ " " + std::string{ core::canonical_hex(aBinding.BaseOid).view() }
			+ " " + std::string{ core::canonical_hex(aBinding.HeadOid).view() };
		require(result.Outcome == application::agent_run_outcome::success && result.ExitCode == 0
					&& result.Session == launch->Session && result.StandardError.empty()
					&& result.Events.size() == 1U && result.Events.front().Session == launch->Session
					&& result.Events.front().Text == receipt,
				"production agent process consumes exact migrated old ordinals, authority and OIDs from stdin");
		auto staleSupplement            = *supplement;
		staleSupplement.Session.Binding = stale;
		auto staleLaunch                = application::build_agent_launch(profile, staleSupplement,
																		  [](const std::string&) -> std::optional<std::string> { return std::nullopt; });
		require(staleLaunch.has_value(), "negative process course frames changed epoch reference");
		staleLaunch->WorkingDirectory = launch->WorkingDirectory;
		const auto staleResult        = process.run(*staleLaunch, 10000);
		require(staleResult.Outcome == application::agent_run_outcome::nonzero_exit
					&& staleResult.ExitCode == EXIT_FAILURE && staleResult.Events.empty()
					&& staleResult.StandardError == "migration binding rejected\n",
				"actual agent child rejects a changed ordinal against the frozen pre-migration binding");
	}

	void verify_real_migrated_retained_refs()
	{
		temporary_root               original{ "migration-original" };
		temporary_root               copied{ "migration-copy" };
		core::review_epoch_id        epoch;
		core::pull_request_authority authority;
		{
			real_fixture value{ "migration", original.Path };
			auto         handoff   = make_handoff();
			handoff.Hostname       = application::github_canonical_hostname{ "ghe.example.test" };
			handoff.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
			handoff.SmartGit       = std::make_unique<fixture_authority>(application::authority_binding(handoff));
			require(application::valid(handoff), "synthetic GHES handoff is valid");
			const auto key = catalog_key();
			require(key.Host == handoff.Host && key.HostAuthority == handoff.HostAuthority
						&& key.Repository == handoff.Repository && key.PullRequest == handoff.PullRequest
						&& key.Profile == handoff.Profile && key.ProfileRevision == handoff.ProfileRevision
						&& key.Account == handoff.Account && key.Viewer == handoff.Viewer && key.Number == handoff.Number,
					"catalogue key names the exact synthetic GHES handoff authority");
			value.Host->accept(std::move(handoff));
			value.drain();
			epoch     = value.inserted_epoch();
			authority = *value.Host->state().Authority;
			value.close_host();
		}
		const core::agent_revision_binding oldBinding{
			.Host = catalog_key().Host, .Repository = catalog_key().Repository, .PullRequest = catalog_key().PullRequest, .BaseOid = *core::parse_git_commit_oid(commit_hex), .HeadOid = *core::parse_git_commit_oid(commit_hex), .LeftEpoch = epoch.Ordinal, .RightEpoch = epoch.Ordinal
		};
		{
			// [[BPR-0853]]. Binary, as the product writes it: a text stream on
			// Windows ends the line with a carriage return the helper will not decode.
			std::ofstream bindingFile{ original.Path / "binding", std::ios::binary };
			bindingFile << bpr::adapters::mcp::encode_revision_binding(oldBinding) << '\n';
			bindingFile.close();
			require(bindingFile.good(), "original MCP binding freezes before migration");
		}
		std::filesystem::copy(original.Path / "catalog", copied.Path / "catalog", std::filesystem::copy_options::recursive);
		// [[BPR-0853]]. A copy is not a catalog the store created. On Windows every
		// copied directory and file inherits its parent's list, and the store
		// refuses an existing catalogue object whose owner-private descriptor is
		// not its own explicit, protected one. Give each copied object, parents
		// first, that descriptor, as the store created the original. On POSIX
		// this restates the owner-only modes the copy carried.
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ copied.Path / "catalog" }) {
			require(bpr::tests::sqlite_contract::make_owner_private(entry.path(), entry.is_directory()),
					"copied catalogue object carries the store's private descriptor");
		}
		std::filesystem::copy_file(original.Path / "binding", copied.Path / "binding");
		{
			legacy_catalog_copy database{ copied.Path / "catalog/state/v1/epoch-catalog.sqlite3" };
			require(database.integer("SELECT count(*) FROM append_occurrence") == 0,
					"live fixture has no historical append occurrence ownership to discard");
			require(bpr::tests::sqlite_contract::make_version_four_catalog(database)
						&& database.integer("PRAGMA user_version") == 4,
					"copy restores exact previous v4 schema");
		}
		for (const auto pass : { 0, 1 }) {
			(void)pass;
			real_fixture migrated{ "migrated", copied.Path };
			migrated.close_host();
			auto catalog = migrated.Catalogs->open({ .ApplicationSession = core::application_session_id{ 91U }, .Authority = catalog_key() }, { });
			require(catalog && *catalog, "migrated catalogue reopens");
			const auto snapshot = (*catalog)->snapshot({ });
			require(snapshot && snapshot->LatestLive && snapshot->LatestLive->Id == epoch,
					"migration and restart preserve original local key, incarnation and ordinal");
			verify_migrated_binding_consumers(copied.Path, oldBinding, **catalog, epoch);
			const auto anchors = exact_epoch_anchors(**catalog, epoch);
			require(anchors.Anchors.size() == 2U && std::ranges::all_of(anchors.Anchors, [&](const auto& aAnchor) {
						return aAnchor.Availability == core::anchor_availability::available
							&& aAnchor.Commit == core::parse_git_commit_oid(commit_hex);
					}),
					"old exact catalogue anchors retain original available commits");
			auto originalCache = git_adapter::make_installed_git_cache({ .GitExecutable     = std::filesystem::canonical(BPR_TEST_GIT_EXECUTABLE),
																		 .StorageRoot       = original.Path / "git",
																		 .WindowsSystemRoot = bpr::tests::git_contract::windows_system_root() });
			require(originalCache.has_value(), "original physical cache reconstructs after catalogue migration");
			auto cache = (*originalCache)->open({ .ApplicationSession = core::application_session_id{ 91U }, .Authority = authority, .ObjectFormat = core::git_hash_algorithm::sha1 }, { });
			require(cache && *cache, "original real Git cache reopens after copied catalogue migration");
			const auto refs = (*cache)->inspect_retained(epoch.Ordinal, { });
			require(refs && refs->Scope == epoch.Authority && refs->Ordinal == epoch.Ordinal
						&& refs->Base == anchors.Anchors[0].Commit && refs->Head == anchors.Anchors[1].Commit,
					"original retained Git ref names physically resolve exact migrated anchors after restart");
		}
	}

	auto seed_review_statistics(const std::filesystem::path& aRoot) -> std::unique_ptr<application::insights_storage_port>
	{
		auto                        store = bpr::adapters::sqlite::make_insights_storage(aRoot, [] { return std::vector<application::insights_storage_row>{ }; });
		core::insights_review_event event;
		event.Kind        = core::insights_review_kind::submitted;
		event.Host        = "github.com";
		event.Repository  = "owner/lifetime";
		event.PullRequest = 1;
		event.Identity    = "review1";
		event.LocalDay    = 20000;
		event.UnixSeconds = 1728000000;
		store->record_reviews({ event });
		return store;
	}
	void require_review_statistics(application::insights_storage_port& aStore)
	{
		require(aStore.reviews(2000000000).Ranges[2][static_cast<std::size_t>(core::insights_review_metric::submitted)] == 1, "real cache cleanup preserves the independent lifetime review total");
	}

	auto verify_real_prune() -> void
	{
		real_fixture value{ "prune" };
		auto         statistics = seed_review_statistics(value.Root.Path / "git");
		value.start();
		const auto epoch     = value.inserted_epoch();
		const auto authority = *value.Host->state().Authority;
		value.Host->dispatch(core::prune_review_epochs_intent{
			.Catalog = *value.Host->state().Catalog,
			.Epochs  = { epoch },
		});
		value.drain();
		require_review_statistics(*statistics);
		require(value.Host->state().Catalog->Revision
					== core::epoch_catalog_revision{ 2 },
				"real prune advances the visible catalog exactly once");
		value.close_host();

		auto catalog = value.Catalogs->open(
			application::epoch_catalog_open_request{
				.ApplicationSession = core::application_session_id{ 91U },
				.Authority          = catalog_key(),
			},
			{ });
		require(catalog.has_value() && *catalog, "reopen real catalog after prune");
		const auto anchors = exact_epoch_anchors(**catalog, epoch);
		require(anchors.Anchors.size() == 2U
					&& std::ranges::all_of(anchors.Anchors,
										   [](const auto& aAnchor) {
											   return aAnchor.Availability
												   == core::anchor_availability::pruned;
										   }),
				"real catalog retains only pruned tombstones after paired cleanup");

		auto cache = value.Caches->open(
			application::git_cache_open_request{
				.ApplicationSession = core::application_session_id{ 91U },
				.Authority          = authority,
				.ObjectFormat       = core::git_hash_algorithm::sha1,
			},
			{ });
		require(cache.has_value() && *cache, "reopen real cache after prune");
		const auto retained = (*cache)->inspect_retained(epoch.Ordinal, { });
		require(!retained
					&& retained.error() == application::git_cache_failure::ref_conflict,
				"real prune removes exactly the retained base/head pair");
	}

	auto verify_real_cache_delete() -> void
	{
		real_fixture value{ "delete" };
		auto         statistics = seed_review_statistics(value.Root.Path / "git");
		value.start();
		const auto epoch     = value.inserted_epoch();
		const auto authority = *value.Host->state().Authority;
		value.Host->dispatch(core::delete_repository_cache_intent{
			.Catalog = *value.Host->state().Catalog,
			.Cache   = authority.Cache,
		});
		value.drain();
		require_review_statistics(*statistics);
		require(value.Host->state().Catalog->Revision
					== core::epoch_catalog_revision{ 2 },
				"real cache deletion advances the visible catalog exactly once");
		value.close_host();

		auto catalog = value.Catalogs->open(
			application::epoch_catalog_open_request{
				.ApplicationSession = core::application_session_id{ 91U },
				.Authority          = catalog_key(),
			},
			{ });
		require(catalog.has_value() && *catalog,
				"reopen real catalog after delete");
		const auto anchors = exact_epoch_anchors(**catalog, epoch);
		require(anchors.Anchors.size() == 2U
					&& std::ranges::all_of(anchors.Anchors,
										   [](const auto& aAnchor) {
											   return aAnchor.Availability
												   == core::anchor_availability::cache_deleted;
										   }),
				"real catalog retains cache-deleted tombstones after repository "
				"cleanup");

		const auto reopened = value.Caches->open(
			application::git_cache_open_request{
				.ApplicationSession = core::application_session_id{ 91U },
				.Authority          = authority,
				.ObjectFormat       = core::git_hash_algorithm::sha1,
			},
			{ });
		const auto cacheText  = lower_hex(authority.Cache.bytes());
		const auto repository = value.Root.Path / "git" / "repositories" / "v1" / (cacheText + ".git");
		const auto tombstone  = value.Root.Path / "git" / "quarantine" / "v1"
			/ (cacheText + ".deleted");
		require(!reopened
					&& reopened.error()
						== application::git_cache_failure::repair_required
					&& !std::filesystem::exists(repository)
					&& std::filesystem::is_regular_file(tombstone),
				"real whole-cache deletion removes the repository and leaves a "
				"terminal tombstone that prevents cache-scope reuse");
	}
} // namespace

auto main(int argc, char** argv) -> int
try {
	QCoreApplication application{ argc, argv };
	if (argc == 3) {
		const auto phase = std::string_view{ argv[1] };
		if (phase.starts_with("--kill-transaction-")) {
			kill_journal_transaction(phase, argv[2]);
		} else if (phase.starts_with("--kill-")) {
			kill_epoch(phase, argv[2]);
		} else {
			require(phase == "--recover-epoch", "known epoch recovery course");
			recover_epoch(argv[2]);
		}
		return EXIT_SUCCESS;
	}
	verify_real_migrated_retained_refs();
	verify_real_prune();
	verify_real_cache_delete();
	std::cout << "Epoch maintenance integration passed\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
