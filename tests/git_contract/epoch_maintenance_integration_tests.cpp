#include "bpr/adapters/git/installed_git_cache.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/application/epoch_application_host.hpp"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
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
		if (!aCondition) {
			std::cerr << "FAIL: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
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
			output.push_back(digits[(byte >> 4U) & 0x0fU]);
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
		explicit fixture_transport(application::git_smart_request_context aContext)
			: Context{ std::move(aContext) }
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
			auto body = packet("version 2\n");
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
			auto body = packet(std::string{ commit_hex } + " refs/heads/main\n");
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
		bool                                   Revoked = false;
	};

	class fixture_authority final : public application::git_smart_authority_lease
	{
	public:
		explicit fixture_authority(application::git_smart_authority_binding aBinding)
			: Binding{ std::move(aBinding) }
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
			return std::make_shared<fixture_transport>(aContext);
		}

		void revoke() noexcept override { Revoked = true; }

		[[nodiscard]] auto revoked() const noexcept -> bool override
		{
			return Revoked;
		}

	private:
		application::git_smart_authority_binding Binding;
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
		}

		std::vector<application::epoch_presentation_update> Updates;
	};

	class temporary_root final
	{
	public:
		explicit temporary_root(const std::string_view aLabel)
		{
			Path = std::filesystem::temp_directory_path()
				/ (std::string{ "bpr-maintenance-" } + std::string{ aLabel } + "-"
				   + std::to_string(std::chrono::steady_clock::now()
										.time_since_epoch()
										.count()));
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
			Path = std::filesystem::canonical(Path);
		}

		~temporary_root()
		{
			std::error_code error;
			std::filesystem::remove_all(Path, error);
		}

		temporary_root(const temporary_root&)                    = delete;
		auto operator=(const temporary_root&) -> temporary_root& = delete;

		std::filesystem::path Path;
	};

	[[nodiscard]] auto make_handoff() -> application::accepted_pull_request_handoff
	{
		const auto commit  = core::parse_git_commit_oid(commit_hex);
		const auto baseRef = application::parse_git_base_branch_ref("refs/heads/main");
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
			.Snapshot              = core::provider_snapshot_id{ 1U },
			.IdentityGeneration    = core::github_identity_generation{ 3U },
			.PullRequestGeneration = core::request_generation{ 4U },
			.Origin                = core::pull_request_revision_origin::live,
			.ObjectFormat          = core::git_hash_algorithm::sha1,
			.BaseBranch            = identity<core::base_branch_id>(30U),
			.BaseRef               = *baseRef,
			.Base                  = *commit,
			.Head                  = *commit,
		};
		handoff.SmartGit = std::make_unique<fixture_authority>(
			application::authority_binding(handoff));
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
		explicit real_fixture(const std::string_view aLabel)
			: Root{ aLabel }
		{
			auto madeCache   = git_adapter::make_installed_git_cache({
				.GitExecutable = std::filesystem::canonical(BPR_TEST_GIT_EXECUTABLE),
				.StorageRoot   = Root.Path / "git",
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
				Projection);
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

		void start()
		{
			Host->accept(make_handoff());
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

	auto verify_real_prune() -> void
	{
		real_fixture value{ "prune" };
		value.start();
		const auto epoch     = value.inserted_epoch();
		const auto authority = *value.Host->state().Authority;
		value.Host->dispatch(core::prune_review_epochs_intent{
			.Catalog = *value.Host->state().Catalog,
			.Epochs  = { epoch },
		});
		value.drain();
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
		value.start();
		const auto epoch     = value.inserted_epoch();
		const auto authority = *value.Host->state().Authority;
		value.Host->dispatch(core::delete_repository_cache_intent{
			.Catalog = *value.Host->state().Catalog,
			.Cache   = authority.Cache,
		});
		value.drain();
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
{
	QCoreApplication application{ argc, argv };
	verify_real_prune();
	verify_real_cache_delete();
	std::cout << "Epoch maintenance integration passed\n";
	return EXIT_SUCCESS;
}
