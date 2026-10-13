#include "bpr/application/manual_executor.hpp"
#include "bpr/application/workbench_application_host.hpp"
#include "bpr/core/comparison_preparation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto expect(const bool             aCondition,
				const std::string_view aLabel,
				const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aLabel << ": " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] constexpr auto file(const std::uint8_t aSeed)
		-> core::workbench_file_id
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0x5AU);
		return core::workbench_file_id{ bytes };
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	template <typename Oid>
	[[nodiscard]] constexpr auto oid(const std::uint8_t aSeed) -> Oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return Oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto authority(const std::uint32_t aNumber = 17U)
		-> application::workbench_pull_request_authority
	{
		return {
			.ApplicationSession    = core::application_session_id{ 3U },
			.Profile               = core::host_profile_id{ 10U },
			.ProfileRevision       = core::github_profile_revision{ 20U },
			.Host                  = core::git_host_id{ 30U },
			.HostAuthority         = opaque<core::host_authority_fingerprint>(13U),
			.Account               = core::github_account_id{ 40U },
			.Viewer                = core::github_viewer_actor_id{ 50U },
			.Repository            = core::repository_id{ 101U },
			.PullRequest           = core::pull_request_id{ 202U },
			.Number                = core::pull_request_number{ aNumber },
			.Snapshot              = core::provider_snapshot_id{ 60U },
			.IdentityGeneration    = core::github_identity_generation{ 70U },
			.PullRequestGeneration = core::request_generation{ 80U },
		};
	}

	[[nodiscard]] constexpr auto command_index(const core::workbench_command_id aId)
		-> std::size_t
	{
		return static_cast<std::size_t>(aId);
	}

	/// \brief Records every observation request and answers a scripted result.
	class fake_observations final : public application::workbench_observation_port
	{
	public:
		[[nodiscard]] auto assemble(
			const application::workbench_observation_request& aRequest,
			const std::stop_token                             aStop)
			-> application::workbench_file_observation override
		{
			Requests.push_back(aRequest);
			StopObserved.push_back(aStop.stop_requested());
			if (Throw) {
				throw std::runtime_error{ "observation failure" };
			}
			auto files = std::vector<application::workbench_file_observation_entry>{ };
			files.reserve(Files.size());
			for (const auto& file : Files) {
				const auto path = "file-" + std::to_string(file.bytes().front()) + ".cpp";
				files.push_back({
					.File             = file,
					.PresentationPath = path,
					.CurrentPath      = std::filesystem::path{ path },
					.Change           = application::workbench_file_change_kind::modified,
					.ContentKind      = std::ranges::find(Binary, file) != Binary.end()
						? application::workbench_file_content_kind::binary
						: application::workbench_file_content_kind::text,
				});
			}
			auto observation = application::workbench_file_observation{
				.Context    = aRequest.Context,
				.Authority  = aRequest.Authority,
				.Status     = Status,
				.Actionable = std::move(files),
			};
			if (Entries) {
				observation.Actionable = *Entries;
			}
			observation.Withheld = Withheld;
			if (MismatchAuthority) {
				observation.Authority.Number = core::pull_request_number{ 999U };
			}
			// Forging the identity a terminal claims is the only way a test can
			// reach the host's exactness guard: every honest terminal repeats the
			// context the host itself issued.
			if (ForgeGeneration) {
				observation.Context.Generation = core::request_generation{
					aRequest.Context.Generation.value() + 1'000U
				};
			}
			if (ForgeSession) {
				observation.Context.Session = core::application_session_id{ 999U };
			}
			if (ForgeRequest.has_value()) {
				observation.Context.Request = *ForgeRequest;
			}
			return observation;
		}

		std::optional<std::vector<application::workbench_file_observation_entry>> Entries;
		std::vector<application::workbench_file_observation_entry>                Withheld;
		std::vector<core::workbench_file_id>                                      Files;
		/// [[BPR-1327]]. Files observed as binary.
		std::vector<core::workbench_file_id>                    Binary;
		application::workbench_observation_status               Status            = application::workbench_observation_status::available;
		bool                                                    Throw             = false;
		bool                                                    MismatchAuthority = false;
		bool                                                    ForgeGeneration   = false;
		bool                                                    ForgeSession      = false;
		std::optional<core::request_id>                         ForgeRequest;
		std::vector<application::workbench_observation_request> Requests;
		std::vector<bool>                                       StopObserved;
	};

	/// \brief Stands in for the durable settings path across host lifetimes.
	class fake_settings final : public application::workbench_settings_port
	{
	public:
		[[nodiscard]] auto load(const std::stop_token aStop)
			-> application::workbench_settings_result<
				application::workbench_layout_settings> override
		{
			++Loads;
			LoadStopObserved.push_back(aStop.stop_requested());
			if (!Available) {
				return std::unexpected{ application::workbench_settings_error{
					.Failure = application::workbench_settings_failure::unavailable } };
			}
			return Stored;
		}

		[[nodiscard]] auto store(
			const application::workbench_layout_settings& aSettings,
			const std::stop_token                         aStop)
			-> application::workbench_settings_result<void> override
		{
			++Stores;
			StoreStopObserved.push_back(aStop.stop_requested());
			if (!Available) {
				return std::unexpected{ application::workbench_settings_error{
					.Failure = application::workbench_settings_failure::unavailable } };
			}
			Stored = aSettings;
			return { };
		}

		[[nodiscard]] auto profile_scoped() const noexcept -> bool override { return ProfileScoped; }
		[[nodiscard]] auto window() const noexcept -> core::bpr_window_id override { return Window; }

		application::workbench_layout_settings Stored{ };
		// [[BPR-0783]]. The profile's defaults record is this port scoped to a
		// profile with no window: the shape that must never carry a Home view.
		bool                ProfileScoped = false;
		core::bpr_window_id Window{ };
		bool                Available = true;
		std::size_t         Loads     = 0U;
		std::size_t         Stores    = 0U;
		std::vector<bool>   LoadStopObserved;
		std::vector<bool>   StoreStopObserved;
	};

	class recording_projections final : public application::workbench_projection_port
	{
	public:
		void publish(const application::workbench_projection& aProjection) override
		{
			Events.emplace_back("publish");
			Published.push_back(aProjection);
		}

		void fail(const application::workbench_failure_publication& aFailure) override
		{
			Events.emplace_back("fail");
			Failed.push_back(aFailure);
		}

		std::vector<application::workbench_projection>          Published;
		std::vector<application::workbench_failure_publication> Failed;
		std::vector<std::string_view>                           Events;
	};

	[[nodiscard]] auto selected_diff_context()
		-> application::workbench_selected_diff_context
	{
		const auto workbench = authority();
		const auto owner     = core::pull_request_authority{
			.Profile         = workbench.Profile,
			.ProfileRevision = workbench.ProfileRevision,
			.Host            = workbench.Host,
			.HostAuthority   = workbench.HostAuthority,
			.Account         = workbench.Account,
			.Viewer          = workbench.Viewer,
			.Repository      = workbench.Repository,
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = workbench.PullRequest,
			.Number          = workbench.Number,
			.Scope           = opaque<core::pull_request_authority_scope_id>(15U),
		};
		const auto incarnation = opaque<core::epoch_catalog_incarnation_id>(16U);
		const auto options     = core::normalize_diff_options(core::diff_options{ });
		expect(options.has_value(), "default diff options normalize");
		const auto digest = core::options_digest(*options);
		expect(digest.has_value(), "default diff options have a digest");
		return {
			.ApplicationSession = workbench.ApplicationSession,
			.Authority          = owner,
			.Comparison         = {
				.Left = {
					.Identity = {
						.Authority   = owner.Scope,
						.Incarnation = incarnation,
						.Kind        = core::comparison_anchor_kind::epoch_base,
						.Epoch       = core::epoch_ordinal{ 1 },
					},
					.Commit       = oid<core::git_commit_oid>(20U),
					.Availability = core::anchor_availability::available,
				},
				.Right = {
					.Identity = {
						.Authority   = owner.Scope,
						.Incarnation = incarnation,
						.Kind        = core::comparison_anchor_kind::epoch_head,
						.Epoch       = core::epoch_ordinal{ 1 },
					},
					.Commit       = oid<core::git_commit_oid>(30U),
					.Availability = core::anchor_availability::available,
				},
			},
			.ComparisonRevision = core::comparison_revision{ 1 },
			.Options            = *options,
			.OptionsDigest      = *digest,
			.Algorithm          = core::supported_diff_algorithm_revision,
		};
	}

	class fake_selected_diffs final : public application::workbench_selected_diff_port
	{
	public:
		[[nodiscard]] auto context(
			const application::workbench_pull_request_authority& aAuthority) const
			-> std::optional<application::workbench_selected_diff_context> override
		{
			return aAuthority == authority() ? Context : std::nullopt;
		}

		[[nodiscard]] auto resolve(
			const application::workbench_selected_diff_request& aRequest,
			const std::stop_token                               aStop)
			-> application::workbench_selected_diff_observation override
		{
			Requests.push_back(aRequest);
			StopObserved.push_back(aStop.stop_requested());
			auto result = application::workbench_selected_diff_observation{
				.Request = aRequest,
				.Status  = aStop.stop_requested()
					? application::workbench_selected_diff_status::cancelled
					: Status,
			};
			if (result.Status
				== application::workbench_selected_diff_status::available) {
				result.LeftBlob  = oid<core::git_blob_oid>(40U);
				result.RightBlob = oid<core::git_blob_oid>(50U);
				result.LeftPath  = aRequest.File.CurrentPath;
				result.RightPath = aRequest.File.CurrentPath;
				result.LeftEndpoint
					= application::make_workbench_selected_diff_endpoint_for_testing(
						aRequest,
						core::diff_side::left,
						*result.LeftBlob,
						*result.LeftPath);
				result.RightEndpoint
					= application::make_workbench_selected_diff_endpoint_for_testing(
						aRequest,
						core::diff_side::right,
						*result.RightBlob,
						*result.RightPath);
				if (ForgeRightPath) {
					result.RightPath = "invented/third.cpp";
				}
			}
			return result;
		}

		std::optional<application::workbench_selected_diff_context> Context
			= selected_diff_context();
		application::workbench_selected_diff_status Status
			= application::workbench_selected_diff_status::available;
		std::vector<application::workbench_selected_diff_request> Requests;
		std::vector<bool>                                         StopObserved;
		bool                                                      ForgeRightPath = false;
	};

	class recording_selected_diff_projections final
		: public application::workbench_selected_diff_projection_port
	{
	public:
		void begin(
			const application::workbench_selected_diff_request& aRequest) override
		{
			Begun.push_back(aRequest);
		}

		void publish(
			const application::workbench_selected_diff_publication& aPublication) override
		{
			Published.push_back(aPublication);
		}

		void fail(
			const application::workbench_selected_diff_observation& aFailure) override
		{
			Failed.push_back(aFailure);
		}

		std::vector<application::workbench_selected_diff_request>     Begun;
		std::vector<application::workbench_selected_diff_publication> Published;
		std::vector<application::workbench_selected_diff_observation> Failed;
	};

	/// \brief Provides a decision lane that can withhold, reorder, and replay.
	///
	/// \details The shared manual executor moves each task out before running it,
	/// which cannot express a duplicate terminal. This lane retains a copy so the
	/// same accepted terminal can be delivered twice on purpose.
	class replaying_lane final : public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			Tasks.push_back(std::move(aTask));
		}

		[[nodiscard]] auto pending() const noexcept -> std::size_t
		{
			return Tasks.size();
		}

		void run(const std::size_t aIndex)
		{
			take(aIndex)();
		}

		void run_twice(const std::size_t aIndex)
		{
			const auto task = take(aIndex);
			task();
			task();
		}

		void drain()
		{
			while (!Tasks.empty()) {
				run(0U);
			}
		}

	private:
		[[nodiscard]] auto take(const std::size_t aIndex) -> std::function<void()>
		{
			if (aIndex >= Tasks.size()) {
				throw std::out_of_range{ "Lane task index is out of range" };
			}
			auto task = Tasks[aIndex];
			Tasks.erase(Tasks.begin() + static_cast<std::ptrdiff_t>(aIndex));
			return task;
		}

		std::vector<std::function<void()>> Tasks;
	};

	struct harness
	{
		explicit harness(const application::workbench_host_limits aLimits   = { },
						 fake_settings*                           aSettings = nullptr)
			: Settings{ aSettings == nullptr ? &OwnedSettings : aSettings }
			, Host{ { },
					Observations,
					*Settings,
					Effects,
					Lane,
					Projections,
					core::application_session_id{ 7U },
					aLimits }
		{
		}

		void settle()
		{
			while (Effects.pending() != 0U || Lane.pending() != 0U) {
				Effects.drain();
				Lane.drain();
			}
		}

		fake_observations                       Observations;
		fake_settings                           OwnedSettings;
		fake_settings*                          Settings;
		recording_projections                   Projections;
		application::manual_executor            Effects;
		replaying_lane                          Lane;
		application::workbench_application_host Host;
	};

	struct selected_harness
	{
		void settle()
		{
			while (Effects.pending() != 0U || Lane.pending() != 0U) {
				Effects.drain();
				Lane.drain();
			}
		}

		fake_observations                       Observations;
		fake_settings                           Settings;
		fake_selected_diffs                     SelectedDiffs;
		recording_projections                   Projections;
		recording_selected_diff_projections     SelectedProjections;
		application::manual_executor            Effects;
		replaying_lane                          Lane;
		application::workbench_application_host Host{
			{ },
			Observations,
			Settings,
			Effects,
			Lane,
			Projections,
			core::application_session_id{ 7U },
			{ },
			&SelectedDiffs,
			&SelectedProjections,
		};
	};

	/// \brief [[BPR-0783]]. A Home view away from the default in every field.
	[[nodiscard]] auto home_view() -> application::home_view_state
	{
		return {
			.Grouping     = application::home_grouping::repository,
			.StateHeaders = false,
			.Collapsed    = static_cast<std::uint8_t>(application::home_collapsed_panel::needs_me
													  | application::home_collapsed_panel::recently_viewed),
			.FilterOwned  = true,
			.Filter       = {
				.Role    = core::github_role::author,
				.AddedBy = core::observer_rule_kind::path,
				.Quiet   = application::queue_quiet_filter::quiet,
				.Sort    = application::queue_sort_order::pull_request_number,
			},
		};
	}

	/// \brief [[BPR-0783]]. A durable record no default could be mistaken for.
	///
	/// \details Home is persisted beside the rest of the window's layout, so a
	/// test that changes only Home has to be able to see the drawers and widths
	/// it must leave alone.
	[[nodiscard]] auto stored_layout() -> application::workbench_layout_settings
	{
		return {
			.Files              = core::workbench_drawer_state::collapsed,
			.Conversations      = core::workbench_drawer_state::collapsed,
			.Repositories       = core::workbench_drawer_state::collapsed,
			.LineNumbers        = core::workbench_line_numbers::hidden,
			.OnePane            = core::workbench_one_pane::enabled,
			.FilesWidth         = 500U,
			.ConversationsWidth = 240U,
			.RepositoriesWidth  = 280U,
		};
	}

	void test_change_navigation_observation_is_exact_and_bounded()
	{
		auto value               = selected_harness{ };
		value.Observations.Files = { file(1U) };
		value.Host.observe(authority());
		value.settle();
		value.Host.apply(core::select_workbench_file_intent{ .File = file(1U) });
		value.settle();
		expect(value.SelectedProjections.Published.size() == 1U,
			   "one selected diff is accepted before change observations");

		const auto request   = value.SelectedProjections.Published.back().Request;
		const auto published = value.Projections.Published.size();
		value.Host.observe_change_navigation(request, std::nullopt);
		expect(value.Projections.Published.size() == published,
			   "an unchanged absent change cursor publishes nothing");

		const auto observe = [&value, &request](
								 const std::uint32_t aOrdinal,
								 const bool          aPrevious,
								 const bool          aNext) {
			value.Host.observe_change_navigation(
				request,
				core::workbench_change_cursor{
					.Group = opaque<core::change_group_id>(
						static_cast<std::uint8_t>(aOrdinal)),
					.Ordinal           = aOrdinal,
					.ChangeCount       = 3U,
					.PreviousAvailable = aPrevious,
					.NextAvailable     = aNext,
				});
			const auto& projection = value.Projections.Published.back();
			expect(projection.Commands.has_value()
					   && !projection.Selection.has_value()
					   && !projection.Drawers.has_value()
					   && !projection.Layout.has_value()
					   && !projection.Files.has_value(),
				   "a change cursor publishes only changed command legality");
			const auto& previousCommand
				= projection.Commands->Commands[command_index(
					core::workbench_command_id::previous_change)];
			const auto& nextCommand = projection.Commands->Commands[command_index(
				core::workbench_command_id::next_change)];
			// [[BPR-1328]]. Both change commands stay enabled while a pull
			// request is open; a missing neighbour is a bound, not a dead arrow.
			expect(previousCommand.Enabled && nextCommand.Enabled,
				   "the change commands stay enabled at either end");
			expect(previousCommand.Bound
						   == (aPrevious ? core::workbench_reason_code::none
										 : core::workbench_reason_code::at_first_change)
					   && nextCommand.Bound
						   == (aNext ? core::workbench_reason_code::none
									 : core::workbench_reason_code::at_last_change),
				   "the core projects the exact observed change neighbours");
		};

		observe(1U, false, true);
		observe(2U, true, true);
		observe(3U, true, false);
		const auto atLastPublished = value.Projections.Published.size();
		value.Host.observe_change_navigation(
			request,
			core::workbench_change_cursor{
				.Group             = opaque<core::change_group_id>(3U),
				.Ordinal           = 3U,
				.ChangeCount       = 3U,
				.PreviousAvailable = true,
				.NextAvailable     = false,
			});
		expect(value.Projections.Published.size() == atLastPublished,
			   "an identical bounded cursor is not overpublished");

		value.Host.observe_change_navigation(request, std::nullopt);
		expect(!value.Host.state().Change.has_value(),
			   "an exact absent observation closes change navigation");
		const auto closedPublished = value.Projections.Published.size();

		auto staleGeneration               = request;
		staleGeneration.Context.Generation = core::request_generation{
			request.Context.Generation.value() + 1U
		};
		value.Host.observe_change_navigation(
			staleGeneration,
			core::workbench_change_cursor{
				.Group             = opaque<core::change_group_id>(1U),
				.Ordinal           = 1U,
				.ChangeCount       = 3U,
				.PreviousAvailable = false,
				.NextAvailable     = true,
			});
		auto foreignAuthority                        = request;
		foreignAuthority.WorkbenchAuthority.Snapshot = core::provider_snapshot_id{
			request.WorkbenchAuthority.Snapshot.value() + 1U
		};
		value.Host.observe_change_navigation(
			foreignAuthority,
			core::workbench_change_cursor{
				.Group             = opaque<core::change_group_id>(1U),
				.Ordinal           = 1U,
				.ChangeCount       = 3U,
				.PreviousAvailable = false,
				.NextAvailable     = true,
			});
		expect(value.Projections.Published.size() == closedPublished
				   && !value.Host.state().Change.has_value(),
			   "stale generation and foreign authority observations are ignored");

		value.Host.observe_change_navigation(
			request,
			core::workbench_change_cursor{
				.Group             = opaque<core::change_group_id>(1U),
				.Ordinal           = 1U,
				.ChangeCount       = 3U,
				.PreviousAvailable = false,
				.NextAvailable     = true,
			});
		value.Host.refresh_selected_diff();
		expect(!value.Host.state().Change.has_value(),
			   "superseding selected-diff work clears the old change cursor");
		const auto supersededPublished = value.Projections.Published.size();
		value.Host.observe_change_navigation(
			request,
			core::workbench_change_cursor{
				.Group             = opaque<core::change_group_id>(2U),
				.Ordinal           = 2U,
				.ChangeCount       = 3U,
				.PreviousAvailable = true,
				.NextAvailable     = true,
			});
		expect(value.Projections.Published.size() == supersededPublished
				   && !value.Host.state().Change.has_value(),
			   "a superseded selected-diff request cannot restore cursor facts");
	}

	// [[BPR-0715]]. A signed-out read names no account and no viewer; the
	// workbench accepts that authority as the credential-free one, and still
	// refuses one zero without the other.
	void test_the_signed_out_authority_is_valid_and_a_half_named_one_is_not()
	{
		auto anonymous    = authority();
		anonymous.Account = core::github_account_id{ };
		anonymous.Viewer  = core::github_viewer_actor_id{ };
		expect(application::anonymous(anonymous) && application::valid(anonymous),
			   "the signed-out workbench authority is valid");
		expect(!application::anonymous(authority()) && application::valid(authority()),
			   "the account authority stays valid and is not signed out");
		auto accountOnly   = authority();
		accountOnly.Viewer = core::github_viewer_actor_id{ };
		auto viewerOnly    = authority();
		viewerOnly.Account = core::github_account_id{ };
		expect(!application::valid(accountOnly) && !application::valid(viewerOnly),
			   "an authority naming one of account and viewer is refused");
	}

	void test_invalid_limits_are_rejected()
	{
		auto observations = fake_observations{ };
		auto settings     = fake_settings{ };
		auto projections  = recording_projections{ };
		auto effects      = application::manual_executor{ };
		auto lane         = replaying_lane{ };
		auto threw        = false;
		try {
			const auto host = std::make_unique<application::workbench_application_host>(
				core::workbench_interaction_state{ },
				observations,
				settings,
				effects,
				lane,
				projections,
				core::application_session_id{ 7U },
				application::workbench_host_limits{ .ActionableFiles = 0U });
			expect(host == nullptr, "invalid workbench host limits must be rejected");
		} catch (const std::invalid_argument&) {
			threw = true;
		}
		expect(threw, "invalid workbench host limits are rejected");
	}

	void test_first_observation_publishes_bounded_cursor_facts()
	{
		auto harnessed               = harness{ };
		harnessed.Observations.Files = { file(1U), file(2U), file(3U) };
		harnessed.Host.observe(authority());
		harnessed.settle();

		expect(harnessed.Observations.Requests.size() == 1U,
			   "one observation request reaches the port");
		expect(harnessed.Observations.Requests.front().MaximumFiles
				   == application::workbench_host_limits{ }.ActionableFiles,
			   "the request carries the host bound");
		expect(harnessed.Projections.Published.size() == 1U,
			   "the first accepted observation publishes once");

		const auto& projection = harnessed.Projections.Published.front();
		expect(projection.Context.Session == core::application_session_id{ 7U },
			   "the projection carries the host session");
		expect(projection.Context.Generation == harnessed.Host.generation(),
			   "the projection carries the current incarnation");
		expect(projection.Selection.has_value() && projection.Drawers.has_value()
				   && projection.Commands.has_value() && projection.Layout.has_value(),
			   "the first publication carries every domain");

		const auto& cursor = projection.Selection->Cursor;
		expect(cursor.has_value(), "the workbench became active");
		expect(!cursor->File.has_value() && cursor->Ordinal == 0U
				   && cursor->ActionableCount == 3U && cursor->PreviousAvailable
				   && cursor->NextAvailable,
			   "the host assembled bounded cursor facts from its own order");
		expect(harnessed.Host.actionable().size() == 3U,
			   "the host retains the ordered actionable set");
		expect(harnessed.Host.state().Navigation.has_value(),
			   "the core retains only the cursor");
	}

	void test_navigation_resolves_through_the_host_order()
	{
		auto harnessed               = harness{ };
		harnessed.Observations.Files = { file(1U), file(2U), file(3U) };
		harnessed.Host.observe(authority());
		harnessed.settle();

		harnessed.Host.apply(
			core::select_workbench_file_intent{ .File = file(2U) });
		expect(harnessed.Projections.Published.size() == 2U,
			   "selecting a file publishes");
		const auto& selection = harnessed.Projections.Published.back();
		expect(selection.Selection.has_value() && selection.Commands.has_value()
				   && !selection.Drawers.has_value() && !selection.Layout.has_value(),
			   "a bounded selection change publishes selection and counterpart availability only");
		expect(selection.Commands->Commands[static_cast<std::size_t>(core::workbench_command_id::go_to_header_source)].Enabled,
			   "selecting a supported path offers counterpart navigation through the single command projection");
		expect(selection.Selection->Cursor->Ordinal == 2U
				   && selection.Selection->Cursor->File == file(2U),
			   "the host resolved the exact file to its ordinal");

		harnessed.Host.apply(core::navigate_workbench_file_intent{
			.Direction = core::change_navigation_direction::next });
		expect(harnessed.Projections.Published.size() == 3U,
			   "navigating publishes");
		const auto& navigated = harnessed.Projections.Published.back();
		expect(navigated.Selection.has_value() && navigated.Commands.has_value(),
			   "reaching the last file changes the selection and the commands");
		expect(navigated.Selection->Cursor->File == file(3U)
				   && navigated.Selection->Cursor->Ordinal == 3U,
			   "the host resolved the next ordinal to an identity");
		const auto& next = navigated.Commands->Commands[command_index(
			core::workbench_command_id::next_file)];
		// [[BPR-1328]]. The exhausted command stays enabled and is bounded.
		expect(next.Enabled
				   && next.Bound == core::workbench_reason_code::at_last_file,
			   "the core bounds the exhausted command");

		harnessed.Host.apply(core::navigate_workbench_file_intent{
			.Direction = core::change_navigation_direction::next });
		// [[BPR-0727]]. A rejected gesture publishes its refusal and nothing
		// else, so presentation can explain it where the reviewer looks.
		expect(harnessed.Projections.Published.size() == 4U
				   && harnessed.Projections.Published.back().Refusal
					   == core::workbench_reason_code::at_last_file
				   && !harnessed.Projections.Published.back().Selection.has_value()
				   && !harnessed.Projections.Published.back().Commands.has_value()
				   && !harnessed.Projections.Published.back().Drawers.has_value(),
			   "a rejected gesture publishes only its refusal");
		expect(harnessed.Host.last_reason()
				   == core::workbench_reason_code::at_last_file,
			   "the host reports the core rejection without re-deciding it");
	}

	/// [[BPR-1327]]. The files around the cursor, in preparation order.
	void test_preparation_window_follows_the_cursor()
	{
		auto value = selected_harness{ };
		for (std::uint8_t seed = 1U; seed <= 40U; ++seed) {
			value.Observations.Files.push_back(file(seed));
		}
		value.Observations.Binary = { file(3U) };
		value.Host.observe(authority());
		value.settle();
		auto window = value.Host.preparation_window();
		expect(window.size() == core::prepared_comparison_window - 1U && window.front().File.File == file(1U)
				   && window[1].File.File == file(2U) && window[2].File.File == file(4U),
			   "with no selection the window starts at the first file and leaves binary files out");
		expect(std::ranges::all_of(window,
								   [&value](const application::workbench_selected_diff_request& aRequest) {
									   return aRequest.WorkbenchAuthority == authority()
										   && aRequest.Diff == *value.SelectedDiffs.Context
										   && aRequest.Context.Request.value() != 0U
										   && aRequest.Context.Session == core::application_session_id{ 7U }
									   && aRequest.Context.Generation.value() != 0U;
								   }),
			   "every request names the comparison the port serves now, under a complete request identity");
		value.Host.apply(core::select_workbench_file_intent{ .File = file(20U) });
		value.settle();
		window        = value.Host.preparation_window();
		auto expected = std::vector<core::workbench_file_id>{ file(20U) };
		for (std::uint8_t seed = 21U; seed <= 32U; ++seed) {
			expected.push_back(file(seed));
		}
		expected.push_back(file(19U));
		expected.push_back(file(18U));
		expected.push_back(file(17U));
		expect(window.size() == expected.size()
				   && std::ranges::equal(window, expected, { }, [](const application::workbench_selected_diff_request& aRequest) {
						  return aRequest.File.File;
					  }),
			   "a forty-file review is prepared as a sixteen-file window around the cursor, forward first");
		expect(value.Host.preparation_window(4U).size() == 4U, "a caller may narrow the window");
		value.SelectedDiffs.Context.reset();
		expect(value.Host.preparation_window().empty(), "without an available comparison nothing is prepared");
	}

	void test_selected_diff_lifecycle_is_exactly_once_and_typed()
	{
		auto value               = selected_harness{ };
		value.Observations.Files = { file(1U), file(2U) };
		value.Host.observe(authority());
		value.Effects.drain();
		value.Lane.drain();

		value.Host.apply(core::select_workbench_file_intent{ .File = file(1U) });
		value.Host.apply(core::select_workbench_file_intent{ .File = file(2U) });
		expect(
			value.SelectedProjections.Begun.size() == 2U
				&& value.Effects.pending() == 2U,
			"a replacement selection supersedes one in-flight lookup and begins another");
		value.Effects.drain();
		expect(value.Lane.pending() == 2U, "both immutable terminals reach the lane");
		value.Lane.run(0U);
		value.Lane.run_twice(0U);
		expect(
			value.SelectedDiffs.StopObserved == std::vector{ false }
				&& value.SelectedDiffs.Requests.size() == 1U
				&& value.SelectedProjections.Published.size() == 1U
				&& value.SelectedProjections.Published.front().Key.File == file(2U)
				&& value.SelectedProjections.Published.front().LeftPath
					== std::optional{ std::filesystem::path{ "file-2.cpp" } }
				&& value.SelectedProjections.Published.front().RightPath
					== std::optional{ std::filesystem::path{ "file-2.cpp" } }
				&& value.SelectedProjections.Failed.empty()
				&& value.Host.outstanding_requests() == 0U,
			"superseded and replayed terminals cannot duplicate or replace publication");

		value.SelectedDiffs.Status
			= application::workbench_selected_diff_status::missing_left;
		value.Host.apply(core::select_workbench_file_intent{ .File = file(1U) });
		value.Effects.drain();
		value.Lane.drain();
		expect(
			value.SelectedProjections.Failed.size() == 1U
				&& value.SelectedProjections.Failed.front().Status
					== application::workbench_selected_diff_status::missing_left,
			"a typed missing side reaches selected-diff presentation unchanged");

		value.SelectedDiffs.Status
			= application::workbench_selected_diff_status::available;
		value.Host.apply(core::select_workbench_file_intent{ .File = file(2U) });
		value.Host.cancel();
		value.Effects.drain();
		value.Lane.drain();
		expect(
			value.SelectedDiffs.Requests.size() == 2U,
			"cancellation withholds the selected effect before it reaches the port");
		expect(
			value.SelectedProjections.Published.size() == 1U
				&& value.SelectedProjections.Failed.size() == 1U,
			"cancelled selected effect publishes no terminal");
		expect(
			value.Host.outstanding_requests() == 0U,
			"cancelled selected effect retires its request");

		value.SelectedDiffs.ForgeRightPath = true;
		value.Host.apply(core::select_workbench_file_intent{ .File = file(1U) });
		value.Effects.drain();
		value.Lane.drain();
		expect(
			value.SelectedProjections.Published.size() == 1U
				&& value.SelectedProjections.Failed.size() == 2U
				&& value.SelectedProjections.Failed.back().Status
					== application::workbench_selected_diff_status::failure,
			"an arbitrary third right path cannot replace the epoch-sealed endpoint");

		value.SelectedDiffs.ForgeRightPath = false;
		value.SelectedDiffs.Context        = selected_diff_context();
		value.Host.apply(core::select_workbench_file_intent{ .File = file(2U) });
		value.Effects.drain();
		auto replacement                    = selected_diff_context();
		replacement.ComparisonRevision      = core::comparison_revision{ 2U };
		replacement.Comparison.Right.Commit = oid<core::git_commit_oid>(31U);
		value.SelectedDiffs.Context         = replacement;
		value.Lane.drain();
		expect(
			value.SelectedProjections.Failed.size() == 3U
				&& value.SelectedProjections.Failed.back().Status
					== application::workbench_selected_diff_status::stale,
			"a comparison replacement still rejects the in-flight selected-file terminal as stale");
	}

	// RFC-BPR-0025 AC-03, the one view RFC-BPR-0028 kept: a file chosen and
	// the next and previous file each look their diff up under the one
	// comparison the epoch host accepted. Navigation carries no comparison, so
	// it changes the file and never retargets the pair.
	void test_file_navigation_keeps_the_accepted_comparison()
	{
		auto value               = selected_harness{ };
		value.Observations.Files = { file(1U), file(2U), file(3U) };
		value.Host.observe(authority());
		value.settle();

		value.Host.apply(core::select_workbench_file_intent{ .File = file(1U) });
		value.settle();
		value.Host.apply(core::navigate_workbench_file_intent{
			.Direction = core::change_navigation_direction::next });
		value.settle();
		value.Host.apply(core::navigate_workbench_file_intent{
			.Direction = core::change_navigation_direction::previous });
		value.settle();

		const auto& requests = value.SelectedDiffs.Requests;
		expect(requests.size() == 3U && requests[0].File.File == file(1U)
				   && requests[1].File.File == file(2U) && requests[2].File.File == file(1U),
			   "a file click, next file and previous file each look up the file they reached");
		expect(std::ranges::all_of(requests,
								   [](const application::workbench_selected_diff_request& aRequest) {
									   return aRequest.Diff == selected_diff_context();
								   }),
			   "every lookup carries the exact accepted comparison: navigation never retargets the pair");
		expect(value.SelectedProjections.Published.size() == 3U
				   && value.SelectedProjections.Published.back().Key.File == file(1U)
				   && value.SelectedProjections.Failed.empty(),
			   "each navigation publishes its file's diff in the one view");
	}

	void test_selected_diff_replays_after_context_arrives_and_changes()
	{
		auto value               = selected_harness{ };
		value.Observations.Files = { file(1U) };
		value.SelectedDiffs.Context.reset();
		value.Host.observe(authority());
		value.Effects.drain();
		value.Lane.drain();

		value.Host.apply(core::select_workbench_file_intent{ .File = file(1U) });
		expect(value.Host.selected() == file(1U),
			   "selection remains retained while comparison context is unavailable");
		expect(value.SelectedProjections.Begun.empty()
				   && value.SelectedDiffs.Requests.empty(),
			   "an unavailable comparison starts no selected Git effect");
		expect(value.Projections.Events.size() >= 2U
				   && value.Projections.Events[value.Projections.Events.size() - 2U]
					   == "publish"
				   && value.Projections.Events.back() == "fail"
				   && value.Projections.Failed.back().Failure
					   == application::workbench_host_failure::selected_file_comparison_unavailable,
			   "the accepted selection is published before its typed unavailable-comparison failure");

		value.SelectedDiffs.Context = selected_diff_context();
		value.Host.refresh_selected_diff();
		expect(value.SelectedProjections.Begun.size() == 1U,
			   "an available epoch context rearms the retained selection");
		value.Effects.drain();
		value.Lane.drain();
		expect(value.SelectedProjections.Published.size() == 1U,
			   "the rearmed selection publishes through the selected-diff path");

		auto changed                    = selected_diff_context();
		changed.ComparisonRevision      = core::comparison_revision{ 2U };
		changed.Comparison.Right.Commit = oid<core::git_commit_oid>(31U);
		value.SelectedDiffs.Context     = changed;
		value.Host.refresh_selected_diff();
		expect(value.SelectedProjections.Begun.size() == 2U
				   && value.SelectedDiffs.Requests.size() == 1U,
			   "a changed accepted comparison starts one replacement request");
		value.Effects.drain();
		value.Lane.drain();
		expect(value.SelectedDiffs.Requests.size() == 2U
				   && value.SelectedDiffs.Requests.back().Diff == changed
				   && value.SelectedProjections.Published.size() == 2U,
			   "the replay uses the exact changed comparison context");
	}

	/// [[BPR-1277]]. A refresh asks again for the same diff, and the composition
	/// keeps the reviewer's selection and draft only for that: another comparison
	/// or another file is a different diff.
	void test_same_selected_diff_names_only_the_diff_asked_again()
	{
		auto value               = selected_harness{ };
		value.Observations.Files = { file(1U), file(2U) };
		value.Host.observe(authority());
		value.Effects.drain();
		value.Lane.drain();
		value.Host.apply(core::select_workbench_file_intent{ .File = file(1U) });
		value.Effects.drain();
		value.Lane.drain();
		expect(value.SelectedProjections.Begun.size() == 1U, "the selected file begins one diff");
		const auto shown = value.SelectedProjections.Begun.back();

		value.Host.refresh_selected_diff();
		value.Effects.drain();
		value.Lane.drain();
		expect(value.SelectedProjections.Begun.size() == 2U, "a refresh asks for the selected file again");
		expect(application::same_selected_diff(shown, value.SelectedProjections.Begun.back()),
			   "the refreshed request names the same diff");

		auto changed                    = selected_diff_context();
		changed.ComparisonRevision      = core::comparison_revision{ 2U };
		changed.Comparison.Right.Commit = oid<core::git_commit_oid>(31U);
		value.SelectedDiffs.Context     = changed;
		value.Host.refresh_selected_diff();
		value.Effects.drain();
		value.Lane.drain();
		expect(value.SelectedProjections.Begun.size() == 3U
				   && !application::same_selected_diff(shown, value.SelectedProjections.Begun.back()),
			   "another accepted comparison is a different diff");

		value.Host.apply(core::select_workbench_file_intent{ .File = file(2U) });
		value.Effects.drain();
		value.Lane.drain();
		expect(value.SelectedProjections.Begun.size() == 4U
				   && !application::same_selected_diff(shown, value.SelectedProjections.Begun.back()),
			   "another file is a different diff");
	}

	void test_actionability_refresh_preserves_selected_diff_identity()
	{
		auto value               = selected_harness{ };
		value.Observations.Files = { file(1U), file(2U) };
		value.Host.observe(authority());
		value.Effects.drain();
		value.Lane.drain();
		value.Host.apply(core::select_workbench_file_intent{ .File = file(1U) });
		value.Effects.drain();
		value.Lane.drain();

		const auto generation       = value.Host.generation();
		const auto selectedRequests = value.SelectedDiffs.Requests.size();
		const auto selectedResults  = value.SelectedProjections.Published.size();
		value.Observations.Files    = { file(2U) };
		value.Host.refresh_actionability();
		expect(value.Host.generation() == generation
				   && value.Host.selected() == file(1U),
			   "an actionability refresh preserves generation and selection in flight");
		value.Effects.drain();
		value.Lane.drain();

		expect(value.Host.actionable().size() == 1U
				   && value.Host.actionable().front() == file(2U)
				   && value.Host.selected() == file(1U),
			   "filtering a selected file preserves its semantic selection");
		expect(value.Host.state().Navigation.has_value()
				   && value.Host.state().Navigation->File == file(1U)
				   && value.Host.state().Navigation->ActionableCount == 1U
				   && value.Host.state().Navigation->Ordinal == 0U,
			   "the core sees the retained selection outside the filtered order");
		expect(value.SelectedDiffs.Requests.size() == selectedRequests
				   && value.SelectedProjections.Published.size() == selectedResults
				   && value.SelectedProjections.Failed.empty(),
			   "actionability refresh neither restarts nor stales the selected diff");

		value.Observations.Files = { file(1U), file(2U) };
		value.Host.refresh_actionability();
		value.Effects.drain();
		value.Lane.drain();
		expect(value.Host.actionable().size() == 2U
				   && value.Host.selected() == file(1U)
				   && value.Host.state().Navigation->Ordinal == 1U,
			   "changed material can re-enter actionability without detaching selection");
	}

	void test_failed_authority_refresh_clears_rows_and_old_selection()
	{
		auto value               = selected_harness{ };
		value.Observations.Files = { file(1U) };
		value.Host.observe(authority(17U));
		value.Effects.drain();
		value.Lane.drain();
		value.Host.apply(core::select_workbench_file_intent{ .File = file(1U) });
		value.Effects.drain();
		value.Lane.drain();
		const auto selectedRequests = value.SelectedDiffs.Requests.size();

		value.Observations.Status = application::workbench_observation_status::unavailable;
		value.Observations.Files  = { file(2U) };
		value.Host.observe(authority(18U));
		expect(value.Host.actionable().empty()
				   && !value.Host.selected().has_value()
				   && !value.Host.state().Navigation.has_value(),
			   "a new authority retires old rows, selection, and cursor synchronously");
		expect(value.Projections.Published.back().Files.has_value()
				   && value.Projections.Published.back().Files->empty()
				   && value.Projections.Published.back().Selection.has_value()
				   && !value.Projections.Published.back().Selection->Cursor.has_value(),
			   "the retired authority publishes an empty row and selection projection");

		value.Host.apply(core::select_workbench_file_intent{ .File = file(1U) });
		expect(value.SelectedDiffs.Requests.size() == selectedRequests
				   && !value.Host.selected().has_value(),
			   "an old row identity cannot start resolution in the new generation");
		value.Effects.drain();
		value.Lane.drain();
		expect(value.Projections.Failed.back().Failure
					   == application::workbench_host_failure::observation_unavailable
				   && value.Host.actionable().empty()
				   && !value.Host.selected().has_value(),
			   "a failed replacement observation cannot restore prior authority facts");
	}

	void test_exact_invalidation_clears_authority_bound_selection()
	{
		auto harnessed               = harness{ };
		harnessed.Observations.Files = { file(1U) };
		harnessed.Host.observe(authority(17U));
		harnessed.settle();
		harnessed.Host.apply(
			core::select_workbench_file_intent{ .File = file(1U) });
		expect(harnessed.Host.selected() == file(1U),
			   "the first authority retains its selected path identity");

		const auto generation = harnessed.Host.generation();
		const auto published  = harnessed.Projections.Published.size();
		harnessed.Host.invalidate(authority(18U));
		expect(harnessed.Host.generation() == generation
				   && harnessed.Host.selected() == file(1U)
				   && harnessed.Projections.Published.size() == published,
			   "a mismatched authority cannot invalidate the active workbench");

		harnessed.Host.invalidate(authority(17U));
		expect(harnessed.Host.generation().value() == generation.value() + 1U,
			   "exact invalidation advances the workbench incarnation");
		expect(harnessed.Host.actionable().empty()
				   && !harnessed.Host.selected().has_value()
				   && !harnessed.Host.state().Navigation.has_value(),
			   "exact invalidation clears the order, selection, and core cursor");
		expect(harnessed.Projections.Published.back().Selection.has_value()
				   && !harnessed.Projections.Published.back().Selection->Cursor.has_value(),
			   "exact invalidation projects an inactive workbench");

		harnessed.Host.observe(authority(18U));
		harnessed.settle();
		expect(!harnessed.Host.selected().has_value()
				   && harnessed.Host.state().Navigation.has_value()
				   && !harnessed.Host.state().Navigation->File.has_value()
				   && harnessed.Host.state().Navigation->ActionableCount == 1U,
			   "a later authority cannot inherit the invalidated path selection");
	}

	void test_drawer_gesture_publishes_and_persists()
	{
		auto harnessed               = harness{ };
		harnessed.Observations.Files = { file(1U) };
		harnessed.Host.observe(authority());
		harnessed.settle();

		harnessed.Host.apply(core::toggle_workbench_drawer_intent{
			.Drawer = core::workbench_drawer::files });
		expect(harnessed.Projections.Published.size() == 2U,
			   "a drawer gesture publishes");
		const auto& drawers = harnessed.Projections.Published.back();
		expect(drawers.Drawers.has_value() && !drawers.Selection.has_value()
				   && !drawers.Commands.has_value() && !drawers.Layout.has_value(),
			   "a drawer change publishes the drawer domain only");
		expect(drawers.Drawers->Files == core::workbench_drawer_state::collapsed,
			   "the core decided the drawer state");

		harnessed.settle();
		expect(harnessed.Settings->Stores == 1U,
			   "the drawer change issued one persistence request");
		expect(harnessed.Settings->Stored.Files
				   == core::workbench_drawer_state::collapsed,
			   "the persisted record carries the collapsed drawer");
		expect(harnessed.Host.persisted_layouts() == 1U,
			   "the store terminal was accepted exactly once");
	}

	void test_resize_clamps_and_persists_the_width()
	{
		auto       harnessed = harness{ };
		const auto limits    = application::workbench_host_limits{ };
		harnessed.Host.resize(core::workbench_drawer::files, 100'000U);
		expect(harnessed.Projections.Published.size() == 1U, "a resize publishes");
		expect(harnessed.Projections.Published.back().Layout.has_value(),
			   "a resize publishes the layout domain");
		expect(harnessed.Projections.Published.back().Layout->FilesWidth
				   == limits.DrawerWidth,
			   "an out-of-range width is clamped to the host bound");
		harnessed.settle();
		expect(harnessed.Settings->Stores == 1U, "a resize persists the layout");
		expect(harnessed.Settings->Stored.FilesWidth == limits.DrawerWidth,
			   "the persisted width is the clamped width");

		harnessed.Host.resize(core::workbench_drawer::files, 100'000U);
		harnessed.settle();
		expect(harnessed.Projections.Published.size() == 1U,
			   "an unchanged width publishes nothing");
		expect(harnessed.Settings->Stores == 1U,
			   "an unchanged width persists nothing");
	}

	// [[BPR-0661]]. The review-behaviour preferences: a valid record is
	// persisted and published as its own domain, an unchanged one does
	// nothing, a record past a vocabulary is refused and the held record
	// republished so the page reverts, and a restart restores and publishes
	// what was stored.
	void test_review_behaviour_persists_publishes_and_refuses_outside_the_vocabulary()
	{
		auto       durable = fake_settings{ };
		const auto chosen  = application::review_behaviour_settings{
			.OpenModality     = application::review_open_modality::review,
			.MarkRead         = application::review_mark_read_behaviour::when_fully_seen,
			.ReturnComparison = application::review_return_comparison::since_last_review,
			.CommentsFilter   = application::review_comments_filter::unread,
		};
		{
			auto harnessed = harness{ { }, &durable };
			harnessed.Host.restore();
			harnessed.settle();
			harnessed.Projections.Published.clear();
			harnessed.Host.apply_behaviour(chosen);
			expect(harnessed.Host.layout().Behaviour != chosen && durable.Stores == 0U, "behaviour is accepted only after durable completion");
			harnessed.settle();
			expect(harnessed.Projections.Published.size() == 1U
					   && harnessed.Projections.Published.back().Behaviour == std::optional{ chosen },
				   "a behaviour change publishes the chosen record");
			harnessed.settle();
			expect(durable.Stores == 1U && durable.Stored.Behaviour == chosen,
				   "the change is persisted beside the layout");
			auto narrowed           = chosen;
			narrowed.CommentsFilter = application::review_comments_filter::unresolved;
			harnessed.Host.apply_behaviour(narrowed);
			harnessed.settle();
			const auto& only = harnessed.Projections.Published.back();
			expect(harnessed.Projections.Published.size() == 2U
					   && only.Behaviour == std::optional{ narrowed } && !only.Drawers.has_value()
					   && !only.Selection.has_value() && !only.Layout.has_value()
					   && !only.Commands.has_value() && !only.Files.has_value(),
				   "a later change publishes the behaviour domain only");
			harnessed.settle();
			harnessed.Host.apply_behaviour(narrowed);
			harnessed.settle();
			expect(harnessed.Projections.Published.size() == 2U && durable.Stores == 2U,
				   "an unchanged record publishes nothing and persists nothing");
			auto outside             = narrowed;
			outside.ReturnComparison = static_cast<application::review_return_comparison>(9);
			harnessed.Host.apply_behaviour(outside);
			harnessed.settle();
			expect(harnessed.Projections.Published.size() == 3U
					   && harnessed.Projections.Published.back().Behaviour == std::optional{ narrowed }
					   && durable.Stores == 2U && harnessed.Host.layout().Behaviour == narrowed,
				   "a record past a vocabulary is refused: nothing stored, the held record republished");
		}
		const auto stored = durable.Stored.Behaviour;
		auto       second = harness{ { }, &durable };
		second.Host.restore();
		second.settle();
		expect(stored.CommentsFilter == application::review_comments_filter::unresolved
				   && second.Host.layout().Behaviour == stored,
			   "a restarted host restores the preferences");
		expect(!second.Projections.Published.empty()
				   && second.Projections.Published.back().Behaviour
					   == std::optional{ stored },
			   "the restored preferences are published so the window can honour them");
	}

	void test_superseded_terminal_cannot_publish()
	{
		auto harnessed               = harness{ };
		harnessed.Observations.Files = { file(1U), file(2U), file(3U) };
		harnessed.Host.observe(authority());
		harnessed.Effects.run_next();

		harnessed.Observations.Files = { file(4U) };
		harnessed.Host.observe(authority());
		harnessed.Effects.run_next();
		expect(harnessed.Lane.pending() == 2U, "both terminals are withheld");
		expect(harnessed.Host.outstanding_requests() == 2U,
			   "both requests are still owed a terminal");

		// Deliver the newer terminal first, then the superseded one.
		harnessed.Lane.run(1U);
		expect(harnessed.Projections.Published.size() == 1U,
			   "the current terminal publishes");
		expect(harnessed.Host.actionable().size() == 1U,
			   "the current terminal supplied the retained order");

		harnessed.Lane.run(0U);
		expect(harnessed.Projections.Published.size() == 1U,
			   "the superseded terminal publishes nothing");
		expect(harnessed.Projections.Failed.empty(),
			   "the superseded terminal reports no failure either");
		expect(harnessed.Host.rejected_terminals() == 1U,
			   "the superseded terminal was refused");
		expect(harnessed.Host.last_rejection()
				   == application::workbench_terminal_rejection::superseded,
			   "the refusal names supersession");
		expect(harnessed.Host.actionable().size() == 1U,
			   "the superseded order did not replace the retained one");
		expect(harnessed.Host.outstanding_requests() == 0U,
			   "both terminals were accounted exactly once");
	}

	void test_duplicate_terminal_is_delivered_once()
	{
		auto harnessed               = harness{ };
		harnessed.Observations.Files = { file(1U), file(2U) };
		harnessed.Host.observe(authority());
		harnessed.Effects.drain();
		harnessed.Lane.run_twice(0U);

		expect(harnessed.Projections.Published.size() == 1U,
			   "a replayed terminal publishes once");
		expect(harnessed.Host.rejected_terminals() == 1U,
			   "the duplicate delivery was refused");
		expect(harnessed.Host.last_rejection()
				   == application::workbench_terminal_rejection::unknown_request,
			   "the duplicate found no outstanding request");
	}

	/// \brief Refuses one observation terminal that forged its request identity.
	///
	/// \details The forgery keeps the outstanding request id and the observation
	/// domain, so the terminal reaches the host's exactness guard instead of the
	/// unknown-request path. Only that guard can tell it from an honest answer.
	void expect_forged_context_refused(const std::string_view aLabel,
									   const bool             aForgeGeneration,
									   const bool             aForgeSession)
	{
		auto harnessed               = harness{ };
		harnessed.Observations.Files = { file(1U), file(2U) };
		harnessed.Host.observe(authority());
		harnessed.settle();
		const auto published = harnessed.Projections.Published.size();
		expect(published == 1U, aLabel, "the honest observation was adopted");

		harnessed.Observations.ForgeGeneration = aForgeGeneration;
		harnessed.Observations.ForgeSession    = aForgeSession;
		harnessed.Observations.Files           = { file(3U) };
		harnessed.Host.observe(authority());
		harnessed.settle();

		expect(harnessed.Projections.Published.size() == published + 1U,
			   aLabel,
			   "the replacement authority publishes one clear before its terminal");
		expect(harnessed.Projections.Published.back().Files.has_value()
				   && harnessed.Projections.Published.back().Files->empty()
				   && harnessed.Projections.Published.back().Selection.has_value()
				   && !harnessed.Projections.Published.back().Selection->Cursor.has_value(),
			   aLabel,
			   "the clear removes every selectable fact from the prior generation");
		expect(harnessed.Projections.Failed.empty(),
			   aLabel,
			   "a forged terminal identity reports no failure either");
		expect(harnessed.Host.rejected_terminals() == 1U,
			   aLabel,
			   "the forged terminal was refused");
		expect(harnessed.Host.last_rejection()
				   == application::workbench_terminal_rejection::inexact_terminal,
			   aLabel,
			   "the refusal names the inexact terminal");
		expect(harnessed.Host.actionable().empty(),
			   aLabel,
			   "the prior generation order remains retired");
		expect(!harnessed.Host.state().Navigation.has_value(),
			   aLabel,
			   "the prior generation cursor remains retired");
		expect(harnessed.Host.outstanding_requests() == 0U,
			   aLabel,
			   "the forged terminal was accounted exactly once");
	}

	void test_forged_terminal_context_is_refused()
	{
		expect_forged_context_refused("forged generation", true, false);
		expect_forged_context_refused("forged session", false, true);
	}

	void test_terminal_domain_mismatch_is_refused()
	{
		auto harnessed               = harness{ };
		harnessed.Observations.Files = { file(1U) };
		harnessed.Host.observe(authority());
		harnessed.Host.restore();
		expect(harnessed.Host.outstanding_requests() == 2U,
			   "an observation and a settings load are both outstanding");

		// The observation answers with the settings-load request's exact context,
		// so the recorded domain is the only thing that separates the two.
		harnessed.Observations.ForgeRequest = core::request_id{ 2U };
		harnessed.Effects.run(0U);
		harnessed.Lane.run(0U);

		expect(harnessed.Projections.Published.empty(),
			   "a terminal claiming another domain's request publishes nothing");
		expect(harnessed.Projections.Failed.empty(),
			   "a terminal claiming another domain's request reports no failure");
		expect(harnessed.Host.rejected_terminals() == 1U,
			   "the domain mismatch was refused");
		expect(harnessed.Host.last_rejection()
				   == application::workbench_terminal_rejection::inexact_terminal,
			   "the refusal names the inexact terminal");
		expect(harnessed.Host.actionable().empty(),
			   "the misclaimed order was discarded");
		expect(!harnessed.Host.state().Navigation.has_value(),
			   "the workbench stayed inactive");
		expect(harnessed.Host.outstanding_requests() == 1U,
			   "only the misclaimed record was consumed");
	}

	void test_cancellation_withholds_publication()
	{
		auto harnessed               = harness{ };
		harnessed.Observations.Files = { file(1U) };
		harnessed.Host.observe(authority());
		harnessed.Host.cancel();
		harnessed.Effects.drain();

		expect(harnessed.Observations.Requests.empty(),
			   "cancellation is honoured before the port is entered");
		expect(harnessed.Lane.pending() == 1U,
			   "cancellation does not suppress the terminal");

		harnessed.Lane.drain();
		expect(harnessed.Projections.Published.empty(),
			   "a cancelled terminal publishes nothing");
		expect(harnessed.Host.last_rejection()
				   == application::workbench_terminal_rejection::cancelled,
			   "the refusal names cancellation");
		expect(harnessed.Host.outstanding_requests() == 0U,
			   "the cancelled request was accounted");
	}

	void test_failed_and_inexact_observations()
	{
		auto harnessed                = harness{ };
		harnessed.Observations.Status = application::workbench_observation_status::unavailable;
		harnessed.Host.observe(authority());
		harnessed.settle();
		expect(harnessed.Projections.Published.empty()
				   && harnessed.Projections.Failed.size() == 1U,
			   "an unavailable observation fails instead of publishing");
		expect(harnessed.Projections.Failed.back().Failure
				   == application::workbench_host_failure::observation_unavailable,
			   "the failure names the unavailable observation");

		auto bounded               = harness{ application::workbench_host_limits{
			.ActionableFiles = 2U } };
		bounded.Observations.Files = { file(1U), file(2U), file(3U) };
		bounded.Host.observe(authority());
		bounded.settle();
		expect(bounded.Projections.Published.empty()
				   && bounded.Projections.Failed.size() == 1U,
			   "an over-bound observation fails instead of publishing");
		expect(bounded.Projections.Failed.back().Failure
				   == application::workbench_host_failure::observation_limit,
			   "the failure names the observation bound");

		auto misrouted                           = harness{ };
		misrouted.Observations.Files             = { file(1U) };
		misrouted.Observations.MismatchAuthority = true;
		misrouted.Host.observe(authority());
		misrouted.settle();
		expect(misrouted.Projections.Published.empty()
				   && misrouted.Projections.Failed.size() == 1U,
			   "an observation for a foreign authority is not adopted");
		expect(misrouted.Host.actionable().empty(),
			   "the misrouted order was discarded");
	}

	void test_restore_applies_when_the_workbench_activates()
	{
		auto harnessed             = harness{ };
		harnessed.Settings->Stored = {
			.Files              = core::workbench_drawer_state::collapsed,
			.Conversations      = core::workbench_drawer_state::expanded,
			.FilesWidth         = 500U,
			.ConversationsWidth = 240U,
		};
		harnessed.Host.restore();
		harnessed.settle();

		expect(harnessed.Settings->Loads == 1U, "restore reads the durable path");
		expect(harnessed.Host.layout().FilesWidth == 500U,
			   "the host restored the persisted width");
		expect(harnessed.Projections.Published.size() == 1U
				   && harnessed.Projections.Published.back().Layout.has_value(),
			   "the restored width publishes before any workbench exists");
		expect(harnessed.Host.state().FilesDrawer
				   == core::workbench_drawer_state::expanded,
			   "a drawer state cannot be decided without an active workbench");

		harnessed.Observations.Files = { file(1U) };
		harnessed.Host.observe(authority());
		harnessed.settle();
		expect(harnessed.Host.state().FilesDrawer
				   == core::workbench_drawer_state::collapsed,
			   "the restored drawer state reaches the core on activation");
		expect(harnessed.Projections.Published.back().Drawers.has_value()
				   && harnessed.Projections.Published.back().Drawers->Files
					   == core::workbench_drawer_state::collapsed,
			   "the activating publication carries the restored drawer state");
		expect(harnessed.Settings->Stores == 0U, "restoring never writes back");
	}

	void test_layout_survives_a_restart()
	{
		auto durable = fake_settings{ };
		{
			auto first               = harness{ { }, &durable };
			first.Observations.Files = { file(1U) };
			first.Host.observe(authority());
			first.settle();
			first.Host.apply(core::toggle_workbench_drawer_intent{
				.Drawer = core::workbench_drawer::conversations });
			first.Host.resize(core::workbench_drawer::conversations, 500U);
			first.Host.apply(core::toggle_workbench_line_numbers_intent{ });
			first.Host.apply(core::toggle_workbench_one_pane_intent{ });
			first.settle();
		}
		expect(durable.Stored.Conversations == core::workbench_drawer_state::collapsed,
			   "the durable record kept the collapsed drawer");
		expect(durable.Stored.ConversationsWidth == 500U,
			   "the durable record kept the width");
		expect(durable.Stored.LineNumbers == core::workbench_line_numbers::hidden,
			   "the durable record kept the source-pane numbering");

		expect(durable.Stored.OnePane == core::workbench_one_pane::enabled, "one pane persists");
		const auto writes         = durable.Stores;
		auto       second         = harness{ { }, &durable };
		second.Observations.Files = { file(1U) };
		second.Host.restore();
		second.Host.observe(authority());
		second.settle();
		expect(second.Host.state().ConversationsDrawer
				   == core::workbench_drawer_state::collapsed,
			   "a restarted host restores the drawer state");
		expect(second.Host.layout().ConversationsWidth == 500U,
			   "a restarted host restores the drawer width");
		expect(second.Host.state().LineNumbers == core::workbench_line_numbers::hidden,
			   "a restarted host restores the source-pane numbering into the "
			   "core rather than leaving it at the default");
		expect(second.Host.state().OnePane == core::workbench_one_pane::enabled, "restart restores one pane");
		expect(std::ranges::any_of(second.Projections.Published, [](const auto& aProjection) { return aProjection.OnePane && aProjection.OnePane->State == core::workbench_one_pane::enabled; }), "restart carries one pane through the window projection");
		expect(durable.Stores == writes, "a restart performs no write-back");
	}

	void test_settings_failure_is_reported()
	{
		auto harnessed                = harness{ };
		harnessed.Settings->Available = false;
		harnessed.Host.restore();
		harnessed.settle();
		expect(harnessed.Projections.Failed.size() == 1U
				   && harnessed.Projections.Failed.back().Failure
					   == application::workbench_host_failure::settings_unavailable,
			   "an unavailable settings path is reported");
		expect(harnessed.Projections.Published.empty(),
			   "a failed restore publishes no layout");
	}

	void test_orderly_shutdown_flushes_latest_layout_serially()
	{
		auto value = harness{ };
		value.Host.resize(core::workbench_drawer::files, 400U);
		value.Host.resize(core::workbench_drawer::files, 500U);
		value.Host.resize(core::workbench_drawer::files, 600U);
		expect(value.Effects.pending() == 1 && value.Host.outstanding_requests() == 1,
			   "layout bursts retain one active writer and one latest candidate");
		value.Host.shutdown_orderly();
		const auto publications = value.Projections.Published.size();
		value.Host.resize(core::workbench_drawer::files, 700U);
		value.Host.restore();
		value.Host.observe(authority());
		expect(value.Host.is_closing() && !value.Host.is_quiescent() && value.Host.layout().FilesWidth == 600U,
			   "orderly close rejects ingress while final accepted layout drains");
		value.Effects.drain();
		expect(value.Settings->Stores == 1 && value.Settings->Stored.FilesWidth == 400U && value.Effects.pending() == 0,
			   "later writer cannot overtake the first write before its decision terminal");
		value.Lane.run_twice(0);
		expect(value.Effects.pending() == 1 && value.Host.persisted_layouts() == 1,
			   "exact store terminal schedules latest candidate once despite duplicate completion");
		value.settle();
		expect(value.Settings->Stores == 2 && value.Settings->Stored.FilesWidth == 600U
				   && value.Host.persisted_layouts() == 2 && value.Host.is_quiescent(),
			   "orderly shutdown persists only active and final accepted width before quiescence");
		expect(value.Projections.Published.size() == publications && value.Projections.Failed.empty(),
			   "draining layout terminals cannot republish a closing window");
	}

	void test_summary_fraction_load_hold_validation_and_orderly_drain()
	{
		auto durable                   = fake_settings{ };
		durable.Stored.FilesWidth      = 410U;
		durable.Stored.SummaryFraction = 5500U;
		{
			auto first = harness{ { }, &durable };
			first.Host.restore();
			first.Host.resize_summary(6500U);
			first.Host.resize_summary(7000U);
			expect(durable.Stores == 0U, "summary preference waits for the unread window record");
			first.Host.shutdown_orderly();
			first.settle();
			expect(first.Host.is_quiescent() && durable.Stored.SummaryFraction == 7000U && durable.Stored.FilesWidth == 410U, "orderly close merges latest summary choice into the loaded record");
		}
		{
			auto next = harness{ { }, &durable };
			next.Host.restore();
			next.settle();
			expect(next.Host.layout().SummaryFraction == 7000U && next.Projections.Published.back().Layout->SummaryFraction == std::optional{ std::optional<std::uint32_t>{ 7000U } }, "restart projects window fraction");
			const auto stores = durable.Stores;
			next.Host.resize_summary(10000U);
			next.settle();
			expect(durable.Stores == stores && next.Host.layout().SummaryFraction == 7000U, "invalid summary share cannot alter or write the preference");
			next.Host.resize_summary(std::nullopt);
			next.settle();
			expect(!durable.Stored.SummaryFraction && next.Projections.Published.back().Layout->SummaryFraction.has_value() && !*next.Projections.Published.back().Layout->SummaryFraction, "Reset persists and explicitly projects no preference");
		}
	}

	/// \brief [[BPR-1334]]. The window's Line details height takes the
	/// summary's path: held until the record loads, merged on an orderly
	/// close, validated, and Reset to the default.
	void test_line_details_height_load_hold_validation_and_orderly_drain()
	{
		auto durable                     = fake_settings{ };
		durable.Stored.FilesWidth        = 410U;
		durable.Stored.SummaryFraction   = 5500U;
		durable.Stored.LineDetailsHeight = 180U;
		{
			auto first = harness{ { }, &durable };
			first.Host.restore();
			first.Host.resize_line_details(200U);
			first.Host.resize_line_details(240U);
			expect(durable.Stores == 0U, "the height waits for the unread window record");
			first.Host.shutdown_orderly();
			first.settle();
			expect(first.Host.is_quiescent() && durable.Stored.LineDetailsHeight == 240U && durable.Stored.SummaryFraction == 5500U
					   && durable.Stored.FilesWidth == 410U,
				   "orderly close merges the latest height into the loaded record and keeps the rest");
		}
		{
			auto next = harness{ { }, &durable };
			next.Host.restore();
			next.settle();
			expect(next.Host.layout().LineDetailsHeight == 240U
					   && next.Projections.Published.back().Layout->LineDetailsHeight == std::optional{ std::optional<std::uint32_t>{ 240U } },
				   "a restart projects the window's height");
			const auto stores = durable.Stores;
			next.Host.resize_line_details(application::maximum_line_details_height + 1U);
			next.Host.resize_line_details(0U);
			next.settle();
			expect(durable.Stores == stores && next.Host.layout().LineDetailsHeight == 240U,
				   "a height past the bound or zero cannot alter or write the preference");
			next.Host.resize_line_details(std::nullopt);
			next.settle();
			expect(!durable.Stored.LineDetailsHeight && next.Projections.Published.back().Layout->LineDetailsHeight.has_value()
					   && !*next.Projections.Published.back().Layout->LineDetailsHeight,
				   "Reset persists and projects the default height");
		}
	}

	void test_summary_reset_before_load_preserves_the_rest_of_the_record()
	{
		auto durable                   = fake_settings{ };
		durable.Stored.FilesWidth      = 450U;
		durable.Stored.SummaryFraction = 6500U;
		auto first                     = harness{ { }, &durable };
		first.Host.restore();
		first.Host.resize_summary(std::nullopt);
		first.Host.shutdown_orderly();
		first.settle();
		expect(!durable.Stored.SummaryFraction && durable.Stored.FilesWidth == 450U && first.Host.is_quiescent(), "startup Reset clears saved preference after load without overwriting other fields");
	}

	void test_geometry_load_observation_and_orderly_drain()
	{
		auto       value                = harness{ };
		const auto restored             = application::workbench_window_geometry{ -120, 90, 1100, 700, true };
		value.Settings->Stored          = stored_layout();
		value.Settings->Stored.Geometry = restored;
		value.Host.restore();
		value.Host.observe_geometry({ 1, 2, 900, 600, false });
		value.settle();
		expect(value.Host.layout().Geometry == restored && value.Settings->Stores == 0,
			   "startup observations cannot overwrite an unread window record");
		expect(value.Projections.Published.back().Geometry == restored,
			   "the exact loaded native placement is published once");
		const auto publications = value.Projections.Published.size();
		value.Host.observe_geometry({ 2'000'000, 0, 900, 600, false });
		value.Host.observe_geometry({ });
		value.settle();
		expect(value.Settings->Stores == 0, "out-of-bound and empty native observations cannot persist");
		const auto latest = application::workbench_window_geometry{ 180, 140, 1000, 720, false };
		value.Host.observe_geometry(latest);
		value.Host.shutdown_orderly();
		value.settle();
		expect(value.Settings->Stored.Geometry == latest && value.Settings->Stored.FilesWidth == 500
				   && value.Host.is_quiescent(),
			   "Quit drains the last native geometry beside unchanged drawer layout");
		expect(value.Projections.Published.size() == publications,
			   "observing native geometry does not echo a resize or move to presentation");
		value.Host.observe_geometry(restored);
		value.settle();
		expect(value.Settings->Stored.Geometry == latest, "closed window cannot replace durable placement");
	}

	void test_orderly_shutdown_drains_layout_at_request_capacity()
	{
		auto limits                = application::workbench_host_limits{ };
		limits.OutstandingRequests = 1;
		auto value                 = harness{ limits };
		value.Host.restore();
		value.Host.resize(core::workbench_drawer::files, 530U);
		value.Host.shutdown_orderly();
		expect(value.Host.outstanding_requests() == 1 && !value.Host.is_quiescent(),
			   "latest layout remains retained while cancelled load occupies the request cap");
		value.settle();
		expect(value.Settings->Stores == 1 && value.Settings->Stored.FilesWidth == 530U
				   && value.Host.persisted_layouts() == 1 && value.Host.is_quiescent(),
			   "retired load terminal frees capacity for final orderly layout write");
		expect(value.Projections.Failed.empty(), "cancelled load cannot publish a late restore failure");
	}

	void test_orderly_shutdown_failure_and_forced_escalation()
	{
		auto failed                = harness{ };
		failed.Settings->Available = false;
		failed.Host.resize(core::workbench_drawer::files, 550U);
		failed.Host.shutdown_orderly();
		failed.settle();
		expect(failed.Host.is_quiescent() && failed.Host.persisted_layouts() == 0
				   && failed.Projections.Failed.size() == 1
				   && failed.Projections.Failed.back().Failure == application::workbench_host_failure::settings_unavailable,
			   "failed orderly write preserves safe failure without retrying indefinitely or claiming persistence");
		auto forced = harness{ };
		forced.Host.resize(core::workbench_drawer::files, 450U);
		forced.Host.resize(core::workbench_drawer::files, 650U);
		forced.Host.shutdown_orderly();
		forced.Host.shutdown();
		forced.settle();
		expect(forced.Host.is_quiescent() && forced.Settings->Stores == 0 && forced.Projections.Failed.empty(),
			   "forced shutdown escalates orderly drain by cancelling active and queued layout writes");
		auto late = harness{ };
		late.Host.resize(core::workbench_drawer::files, 450U);
		late.Effects.drain();
		late.Host.shutdown_orderly();
		late.Host.shutdown();
		late.Lane.run_twice(0);
		expect(late.Host.is_quiescent() && late.Host.persisted_layouts() == 0 && late.Projections.Failed.empty(),
			   "late committed terminal after forced escalation cannot revive closing acceptance");
	}

	void test_shutdown_withholds_publication_and_quiesces()
	{
		auto harnessed               = harness{ };
		harnessed.Observations.Files = { file(1U) };
		harnessed.Host.observe(authority());
		harnessed.Effects.drain();
		harnessed.Host.shutdown();
		expect(harnessed.Host.is_closing(), "shutdown closes the host");

		harnessed.Lane.drain();
		expect(harnessed.Projections.Published.empty(),
			   "a terminal after shutdown publishes nothing");
		expect(harnessed.Projections.Failed.empty(),
			   "a terminal after shutdown reports no failure");
		expect(harnessed.Host.is_quiescent(), "the host reaches quiescence");
	}

	// [[BPR-0783]]. The window's Home view: an accepted view is published as
	// its own domain and persisted beside the layout the record already held, an
	// unchanged one does nothing, a view outside a vocabulary is refused and the
	// held view republished so Home reverts, and a restart restores and
	// publishes what was stored before any review is open.
	void test_home_view_persists_publishes_and_refuses_outside_the_vocabulary()
	{
		auto durable      = fake_settings{ };
		durable.Stored    = stored_layout();
		const auto loaded = durable.Stored;
		const auto chosen = home_view();
		{
			auto harnessed = harness{ { }, &durable };
			harnessed.Host.restore();
			harnessed.settle();
			expect(harnessed.Host.layout().Home == application::home_view_state{ },
				   "a record stored without a Home answers exactly the declared default view");
			harnessed.Projections.Published.clear();

			harnessed.Host.apply_home_view(chosen);
			const auto& published = harnessed.Projections.Published.back();
			expect(harnessed.Projections.Published.size() == 1U
					   && published.HomeView == std::optional{ chosen }
					   && !published.Layout.has_value() && !published.Behaviour.has_value()
					   && !published.Drawers.has_value() && !published.Selection.has_value()
					   && !published.Commands.has_value() && !published.Files.has_value(),
				   "an accepted view publishes the Home domain only");
			expect(harnessed.Host.layout().Home == chosen, "the host holds the accepted view");
			harnessed.settle();
			auto expected = loaded;
			expected.Home = chosen;
			// Compared whole. A write assembled from the defaults rather than
			// from the loaded record would also carry the view and would also
			// look right field by field.
			expect(durable.Stores == 1U && durable.Stored == expected,
				   "the view is persisted beside the loaded layout, which reaches the store unchanged");
			expect(harnessed.Host.persisted_layouts() == 1U,
				   "the store terminal was accepted exactly once");

			harnessed.Host.apply_home_view(chosen);
			harnessed.settle();
			expect(harnessed.Projections.Published.size() == 1U && durable.Stores == 1U,
				   "an equal view publishes nothing and persists nothing");

			auto outside     = chosen;
			outside.Grouping = static_cast<application::home_grouping>(9);
			harnessed.Host.apply_home_view(outside);
			harnessed.settle();
			expect(harnessed.Projections.Published.size() == 2U
					   && harnessed.Projections.Published.back().HomeView == std::optional{ chosen }
					   && durable.Stores == 1U && harnessed.Host.layout().Home == chosen,
				   "a grouping past the vocabulary is refused: nothing stored, the held view republished");

			auto outsideFilter        = chosen;
			outsideFilter.Filter.Role = static_cast<core::github_role>(9);
			harnessed.Host.apply_home_view(outsideFilter);
			auto outsidePanels      = chosen;
			outsidePanels.Collapsed = static_cast<std::uint8_t>(application::home_collapsed_panel::all + 1U);
			harnessed.Host.apply_home_view(outsidePanels);
			harnessed.settle();
			expect(harnessed.Projections.Published.size() == 4U && durable.Stores == 1U
					   && harnessed.Host.layout().Home == chosen,
				   "a filter value and a collapsed bit outside the view are refused the same way");
		}

		auto second = harness{ { }, &durable };
		second.Host.restore();
		second.settle();
		expect(second.Host.layout().Home == chosen, "a restarted window restores the stored view");
		expect(std::ranges::any_of(second.Projections.Published,
								   [&chosen](const auto& aProjection) {
									   return aProjection.HomeView == std::optional{ chosen };
								   }),
			   "the restored view is published, so Home can show it");
		expect(!second.Host.state().Navigation.has_value(),
			   "and it is published with no review open, which is when Home is shown");
		expect(durable.Stores == 1U, "a restart performs no write-back");
	}

	// [[BPR-0783]]. A view applied while the window's own record is still
	// loading is neither lost nor allowed to write the defaults this window
	// never chose over the stored layout.
	void test_a_view_applied_before_the_load_merges_into_the_loaded_record()
	{
		auto durable                 = fake_settings{ };
		durable.Stored               = stored_layout();
		durable.Stored.Home.Grouping = application::home_grouping::organization;
		const auto loaded            = durable.Stored;
		const auto chosen            = home_view();
		expect(loaded.Home != chosen, "the stored view differs from the one applied in flight");

		auto harnessed = harness{ { }, &durable };
		harnessed.Host.restore();
		harnessed.Host.apply_home_view(chosen);
		expect(harnessed.Projections.Published.empty() && durable.Stores == 0U,
			   "a view applied before the record loads publishes nothing and writes nothing");

		harnessed.settle();
		auto expected = loaded;
		expected.Home = chosen;
		expect(durable.Stores == 1U && durable.Stored == expected,
			   "the held view is merged into the loaded record rather than written over it");
		expect(harnessed.Host.layout().Home == chosen, "the reviewer's later gesture wins over the stored view");
		expect(std::ranges::count_if(harnessed.Projections.Published,
									 [](const auto& aProjection) { return aProjection.HomeView.has_value(); })
				   == 1,
			   "the merged view is published exactly once");
		expect(harnessed.Projections.Published.back().HomeView == std::optional{ chosen },
			   "and what is published is the merged view, not the record's own");

		// The other half of the same claim. A window that merely repeats the
		// view it is already showing must not turn that into a write of the
		// default over the view the record holds.
		auto sync = harness{ { }, &durable };
		sync.Host.restore();
		sync.Host.apply_home_view(application::home_view_state{ });
		sync.settle();
		expect(sync.Host.layout().Home == chosen && durable.Stores == 1U,
			   "a view equal to the one already shown before the load stores nothing");
	}

	// [[BPR-0783]]. The latest view drains with the latest layout, including one
	// still waiting for the record it has to be merged into.
	void test_orderly_shutdown_flushes_the_latest_home_view()
	{
		auto durable = fake_settings{ };
		auto value   = harness{ { }, &durable };
		value.Host.restore();
		value.settle();
		const auto first  = home_view();
		auto       latest = first;
		latest.Grouping   = application::home_grouping::organization;
		value.Host.apply_home_view(first);
		value.Host.apply_home_view(latest);
		value.Host.shutdown_orderly();
		value.Host.apply_home_view(application::home_view_state{ });
		value.settle();
		expect(durable.Stores == 2U && durable.Stored.Home == latest && value.Host.layout().Home == latest
				   && value.Host.is_quiescent(),
			   "orderly shutdown drains the latest accepted view and rejects ingress after it");

		auto held          = fake_settings{ };
		held.Stored        = stored_layout();
		const auto record  = held.Stored;
		auto       closing = harness{ { }, &held };
		closing.Host.restore();
		closing.Host.apply_home_view(first);
		closing.Host.shutdown_orderly();
		closing.settle();
		auto expected = record;
		expected.Home = first;
		expect(held.Stores == 1U && held.Stored == expected && closing.Host.is_quiescent()
				   && closing.Projections.Failed.empty(),
			   "a view still held when the window closes drains with the record it was waiting for");
	}

	// [[BPR-0783]]. Home belongs to the window that shows it. The profile's
	// defaults record names no window, so it never carries one.
	void test_home_is_not_a_profile_default()
	{
		auto durable            = fake_settings{ };
		durable.ProfileScoped   = true;
		durable.Stored          = stored_layout();
		durable.Stored.Home     = home_view();
		durable.Stored.Geometry = { 100, 100, 1100, 700, true };
		auto harnessed          = harness{ { }, &durable };
		harnessed.Host.restore();
		harnessed.settle();
		expect(harnessed.Host.layout().Home == application::home_view_state{ },
			   "the profile's defaults record answers the default view whatever it held");
		expect(harnessed.Host.layout().Geometry == application::workbench_window_geometry{ },
			   "profile defaults cannot publish another window's native placement");
		harnessed.Host.observe_geometry({ 80, 60, 900, 500, false });
		harnessed.settle();
		expect(durable.Stores == 0U, "native placement cannot be written through a profile-default port");

		auto chosen     = home_view();
		chosen.Grouping = application::home_grouping::account;
		harnessed.Host.apply_home_view(chosen);
		harnessed.settle();
		expect(harnessed.Host.layout().Home == chosen && durable.Stores == 0U,
			   "a view over the defaults record is shown but never written as a default");

		harnessed.Host.resize(core::workbench_drawer::files, 480U);
		harnessed.settle();
		expect(durable.Stores == 1U && durable.Stored.FilesWidth == 480U
				   && durable.Stored.Home == application::home_view_state{ }
				   && durable.Stored.Geometry == application::workbench_window_geometry{ },
			   "a later defaults write carries the new width and no Home view");
	}

	// [[BPR-0783]]. A Home gesture the host cannot keep must not vanish. The
	// record never arrived, so there is nothing to merge the view into and
	// nothing the host could write but the default layout this window never
	// read; the view still applies in memory, is published so Home shows what
	// the reviewer chose, and reports settings_unavailable as a behaviour
	// change does when its durable path is unavailable.
	void test_a_home_view_whose_record_cannot_be_read_is_shown_and_reported()
	{
		auto durable         = fake_settings{ };
		durable.Available    = false;
		auto       harnessed = harness{ { }, &durable };
		const auto chosen    = home_view();
		harnessed.Host.restore();
		harnessed.Host.apply_home_view(chosen);
		expect(harnessed.Projections.Published.empty(),
			   "a view applied while the load is still in flight publishes nothing yet");

		harnessed.settle();
		expect(harnessed.Host.layout().Home == chosen,
			   "a view held for a record that never arrives is adopted rather than held for ever");
		expect(std::ranges::any_of(harnessed.Projections.Published,
								   [&chosen](const auto& aProjection) {
									   return aProjection.HomeView == std::optional{ chosen };
								   }),
			   "and it is published, so Home shows what the reviewer chose");
		expect(!harnessed.Projections.Failed.empty()
				   && harnessed.Projections.Failed.back().Failure
					   == application::workbench_host_failure::settings_unavailable,
			   "the record the host could not read is reported");
		expect(durable.Stores == 0U,
			   "and nothing is written, because the only record to write would be the default "
			   "layout this window never read");

		// The other half of the same claim. With no load in flight at all, a
		// later gesture is neither held nor silent.
		harnessed.Projections.Published.clear();
		harnessed.Projections.Failed.clear();
		harnessed.Projections.Events.clear();
		auto later     = chosen;
		later.Grouping = application::home_grouping::organization;
		harnessed.Host.apply_home_view(later);
		expect(harnessed.Projections.Published.size() == 1U
				   && harnessed.Projections.Published.back().HomeView == std::optional{ later }
				   && harnessed.Host.layout().Home == later,
			   "a gesture made after the failed load is applied and published at once");
		expect(harnessed.Projections.Failed.size() == 1U
				   && harnessed.Projections.Failed.back().Failure
					   == application::workbench_host_failure::settings_unavailable,
			   "and it reports that the host cannot keep it");
		expect(harnessed.Projections.Events == std::vector<std::string_view>{ "publish", "fail" },
			   "the accepted view reaches presentation before the failure that qualifies it");
		harnessed.settle();
		expect(durable.Stores == 0U, "a view the host cannot keep is not written either");
	}

	// [[BPR-0783]]. The same case during an orderly close is reported on the
	// same terms as a failed store rather than swallowed by the close.
	void test_an_orderly_close_reports_a_home_view_whose_record_never_arrived()
	{
		auto durable      = fake_settings{ };
		durable.Available = false;
		auto       value  = harness{ { }, &durable };
		const auto chosen = home_view();
		value.Host.restore();
		value.Host.apply_home_view(chosen);
		value.Host.shutdown_orderly();
		value.settle();
		expect(value.Projections.Failed.size() == 1U
				   && value.Projections.Failed.back().Failure
					   == application::workbench_host_failure::settings_unavailable,
			   "an orderly close reports the record it could not read while a view waited for it");
		expect(value.Host.layout().Home == chosen,
			   "the held view is adopted rather than dropped on the floor by the close");
		expect(durable.Stores == 0U && value.Host.is_quiescent(),
			   "and the window closes without writing a default layout over the stored one");
	}

	/// \brief [[BPR-1130]]. The Updates line's fold is this window's: the
	/// core decides it with no workbench active, the host keeps it in the
	/// window's own record, restores it on load without waiting for a
	/// workbench, merges one chosen in flight, and never makes it a default.
	void test_the_updates_line_folds_persists_and_restores_per_window()
	{
		const auto published = [](const auto& aHarness, const core::workbench_updates_line aState) {
			return std::ranges::count_if(aHarness.Projections.Published, [aState](const auto& aProjection) {
				return aProjection.UpdatesLine && aProjection.UpdatesLine->State == aState;
			});
		};

		// A gesture before the window's record arrives is held, then merged.
		auto durable      = fake_settings{ };
		durable.Stored    = stored_layout();
		const auto record = durable.Stored;
		{
			auto harnessed = harness{ { }, &durable };
			harnessed.Host.restore();
			harnessed.Host.apply(core::toggle_workbench_updates_line_intent{ });
			expect(harnessed.Host.last_reason() == core::workbench_reason_code::none
					   && harnessed.Host.state().UpdatesLine == core::workbench_updates_line::open
					   && !harnessed.Host.state().Navigation,
				   "the disclosure opens the line with no workbench active");
			expect(durable.Stores == 0U, "a fold chosen before the record loads writes nothing yet");
			harnessed.settle();
			auto expected        = record;
			expected.UpdatesLine = core::workbench_updates_line::open;
			expect(durable.Stores == 1U && durable.Stored == expected,
				   "the held fold is merged into the loaded record, which keeps its drawers and widths");
			expect(harnessed.Host.state().UpdatesLine == core::workbench_updates_line::open,
				   "the reviewer's gesture wins over the record's fold");

			// A gesture after the load is kept at once.
			harnessed.Host.apply(core::toggle_workbench_updates_line_intent{ });
			harnessed.settle();
			expect(harnessed.Projections.Published.back().UpdatesLine
					   && harnessed.Projections.Published.back().UpdatesLine->State == core::workbench_updates_line::folded
					   && !harnessed.Projections.Published.back().Drawers && !harnessed.Projections.Published.back().Layout,
				   "folding publishes the fold alone");
			expect(durable.Stores == 2U && durable.Stored.UpdatesLine == core::workbench_updates_line::folded,
				   "and keeps it with the window");
			harnessed.Host.apply(core::toggle_workbench_updates_line_intent{ });
			harnessed.settle();
		}
		expect(durable.Stored.UpdatesLine == core::workbench_updates_line::open, "the window's last choice is durable");

		// A restarted window restores its own fold from its record, before any
		// workbench is active, and writes nothing back.
		const auto writes = durable.Stores;
		{
			auto restarted = harness{ { }, &durable };
			expect(restarted.Host.state().UpdatesLine == core::workbench_updates_line::folded,
				   "until its record loads a window shows the line folded");
			restarted.Host.restore();
			restarted.settle();
			expect(restarted.Host.state().UpdatesLine == core::workbench_updates_line::open
					   && published(restarted, core::workbench_updates_line::open) == 1,
				   "the record's open line reaches the core and the window on load, with no workbench");
			expect(durable.Stores == writes, "restoring the fold writes nothing back");
		}

		// Every new window starts folded: the profile's defaults never hold a fold.
		auto defaults               = fake_settings{ };
		defaults.ProfileScoped      = true;
		defaults.Stored             = stored_layout();
		defaults.Stored.UpdatesLine = core::workbench_updates_line::open;
		auto scoped                 = harness{ { }, &defaults };
		scoped.Host.restore();
		scoped.settle();
		expect(scoped.Host.layout().UpdatesLine == core::workbench_updates_line::folded
				   && scoped.Host.state().UpdatesLine == core::workbench_updates_line::folded,
			   "the defaults record answers folded whatever it held");
		scoped.Host.apply(core::toggle_workbench_updates_line_intent{ });
		scoped.settle();
		expect(scoped.Host.state().UpdatesLine == core::workbench_updates_line::open && defaults.Stores == 0U,
			   "a fold over the defaults record is shown but never written as a default");
		scoped.Host.resize(core::workbench_drawer::files, 480U);
		scoped.settle();
		expect(defaults.Stores == 1U && defaults.Stored.FilesWidth == 480U
				   && defaults.Stored.UpdatesLine == core::workbench_updates_line::folded,
			   "a later defaults write carries the width and a folded line");
	}

	/// \brief [[BPR-1130]]. A fold still held when the window closes drains
	/// with the record it was waiting for.
	void test_orderly_shutdown_drains_a_held_fold()
	{
		auto durable       = fake_settings{ };
		durable.Stored     = stored_layout();
		const auto record  = durable.Stored;
		auto       closing = harness{ { }, &durable };
		closing.Host.restore();
		closing.Host.apply(core::toggle_workbench_updates_line_intent{ });
		closing.Host.shutdown_orderly();
		closing.settle();
		auto expected        = record;
		expected.UpdatesLine = core::workbench_updates_line::open;
		expect(durable.Stores == 1U && durable.Stored == expected && closing.Host.is_quiescent()
				   && closing.Projections.Failed.empty(),
			   "the held fold is merged into its record as the window closes");
	}
	/// \brief A catalogue port that withholds only the retained file, as the
	/// composed snapshot port does, and names and numbers what it withholds.
	class catalogue_observations final : public application::workbench_observation_port
	{
	public:
		[[nodiscard]] static auto numbered(const std::size_t aIndex) -> core::workbench_file_id
		{
			std::array<std::uint8_t, 32U> bytes{ };
			bytes[0] = static_cast<std::uint8_t>(aIndex & 0xFFU);
			bytes[1] = static_cast<std::uint8_t>((aIndex >> 8U) & 0xFFU);
			bytes[2] = 0xA5U;
			return core::workbench_file_id{ bytes };
		}

		[[nodiscard]] auto assemble(
			const application::workbench_observation_request& aRequest,
			const std::stop_token                             aStop)
			-> application::workbench_file_observation override
		{
			static_cast<void>(aStop);
			++Observations;
			auto observation = application::workbench_file_observation{
				.Context   = aRequest.Context,
				.Authority = aRequest.Authority,
				.Status    = application::workbench_observation_status::available,
			};
			for (std::size_t index = 0U; index < Count; ++index) {
				const auto path  = "src/file" + std::to_string(index) + ".cpp";
				auto       entry = application::workbench_file_observation_entry{
					.File             = numbered(index),
					.PresentationPath = path,
					.CurrentPath      = std::filesystem::path{ path },
					.Change           = application::workbench_file_change_kind::modified,
					.ContentKind      = application::workbench_file_content_kind::text,
					.Ordinal          = index,
				};
				if (Retained.has_value() && Retained->first == entry.File && !Retained->second) {
					observation.Withheld.push_back(std::move(entry));
				} else {
					observation.Actionable.push_back(std::move(entry));
				}
			}
			return observation;
		}

		std::size_t                                             Count = 0U;
		std::optional<std::pair<core::workbench_file_id, bool>> Retained;
		std::size_t                                             Observations = 0U;
	};

	// [[BPR-0972]]. The retained receipt moving one file is applied to the
	// order in place: no observation, a file delta naming only the moved
	// files, and exactly the order a complete observation answers.
	void test_retained_actionability_moves_files_in_place()
	{
		for (const auto count : { std::size_t{ 32U }, std::size_t{ 4096U } }) {
			auto catalogue         = catalogue_observations{ };
			catalogue.Count        = count;
			auto       settings    = fake_settings{ };
			auto       projections = recording_projections{ };
			auto       effects     = application::manual_executor{ };
			auto       lane        = replaying_lane{ };
			auto       host        = application::workbench_application_host{ { }, catalogue, settings, effects, lane, projections, core::application_session_id{ 7U } };
			const auto settle      = [&] {
				while (effects.pending() != 0U || lane.pending() != 0U) {
					effects.drain();
					lane.drain();
				}
			};
			host.observe(authority());
			settle();
			const auto complete = [&] {
				auto request = application::workbench_observation_request{ .Authority = authority(), .MaximumFiles = count };
				auto ids     = std::vector<core::workbench_file_id>{ };
				for (const auto& entry : catalogue.assemble(request, { }).Actionable) {
					ids.push_back(entry.File);
				}
				--catalogue.Observations;
				return ids;
			};
			const auto first    = catalogue_observations::numbered(5U);
			const auto second   = catalogue_observations::numbered(count - 2U);
			const auto observed = catalogue.Observations;
			const auto before   = host.work_statistics();
			const auto step     = [&](const core::workbench_file_id aFile, const bool aActionable) {
				catalogue.Retained = std::pair{ aFile, aActionable };
				host.retain_actionability(authority(), aFile, aActionable);
				settle();
				const auto actionable = host.actionable();
				expect(std::vector<core::workbench_file_id>{ actionable.begin(), actionable.end() } == complete(),
					   "the order in place equals a complete observation");
				return projections.Published.back();
			};
			const auto withheld = step(first, false);
			expect(withheld.FilesDelta.has_value() && withheld.FilesDelta->Removed == std::vector{ first }
					   && withheld.FilesDelta->Inserted.empty() && !withheld.Files.has_value(),
				   "a read retained file leaves as a one-file delta");
			const auto restored = step(second, true);
			expect(restored.FilesDelta.has_value() && restored.FilesDelta->Removed.empty()
					   && restored.FilesDelta->Inserted.size() == 1U && restored.FilesDelta->Inserted.front().first == 5U
					   && restored.FilesDelta->Inserted.front().second.File == first,
				   "a newly retained actionable file restores the withheld one at its place");
			const auto read = step(second, false);
			expect(read.FilesDelta.has_value() && read.FilesDelta->Removed == std::vector{ second },
				   "the next read file leaves in turn");
			const auto after = host.work_statistics();
			std::cout << "workbench-actionability files=" << count << " targeted_updates=" << after.TargetedActionabilityUpdates - before.TargetedActionabilityUpdates
					  << " moved_files=" << after.TargetedActionabilityFiles - before.TargetedActionabilityFiles
					  << " observations=" << catalogue.Observations - observed
					  << " accepted_rows=" << after.AcceptedObservationRows - before.AcceptedObservationRows
					  << " projected_rows=" << after.FileProjectionRows - before.FileProjectionRows
					  << " position_visits=" << after.FilePositionVisits - before.FilePositionVisits
					  << " published_rows_shifted=" << after.PublishedFileRowsShifted - before.PublishedFileRowsShifted << '\n';
			expect(catalogue.Observations == observed && after.AcceptedObservationRows == before.AcceptedObservationRows
					   && after.FileProjectionRows == before.FileProjectionRows && after.TargetedActionabilityFiles - before.TargetedActionabilityFiles == 3U,
				   "three moved files cost no observation and no complete file projection");
			expect(after.FilePositionVisits > before.FilePositionVisits && after.FilePositionVisits - before.FilePositionVisits <= 3U * count
					   && after.PublishedFileRowsShifted > before.PublishedFileRowsShifted && after.PublishedFileRowsShifted - before.PublishedFileRowsShifted <= 3U * count,
				   "three changed files retain linear active-order maintenance, counted separately from file projection");
			const auto published = projections.Published.size();
			host.refresh_actionability();
			settle();
			const auto actionable = host.actionable();
			expect(std::vector<core::workbench_file_id>{ actionable.begin(), actionable.end() } == complete()
					   && std::ranges::none_of(projections.Published.begin() + static_cast<std::ptrdiff_t>(published), projections.Published.end(),
											   [](const application::workbench_projection& aProjection) { return aProjection.Files.has_value() || aProjection.FilesDelta.has_value(); }),
				   "observing again answers the same order and republishes no files");
			host.shutdown();
			settle();
		}
	}

	void test_routine_selection_visits_only_the_retained_index()
	{
		for (const auto count : { 32U, 4096U }) {
			auto value = harness{ };
			for (auto index = 0U; index < count; ++index) {
				value.Observations.Files.push_back(core::file_identity_of("source/" + std::to_string(index) + ".cpp"));
			}
			value.Host.observe(authority());
			value.settle();
			const auto before   = value.Host.work_statistics();
			const auto requests = value.Observations.Requests.size();
			for (auto step = 0U; step < 16U; ++step) {
				value.Host.apply(core::select_workbench_file_intent{ .File = value.Observations.Files[count - 1U - step] });
			}
			const auto after = value.Host.work_statistics();
			std::cout << "workbench-scaling files=" << count
					  << " selections=16 lookup_comparisons=" << after.FileLookupComparisons - before.FileLookupComparisons
					  << " projected_rows=" << after.FileProjectionRows - before.FileProjectionRows
					  << " projection_capacity_growths=" << after.FileProjectionCapacityGrowths - before.FileProjectionCapacityGrowths << '\n';
			expect(after.FileProjectionRows == before.FileProjectionRows
					   && after.FileProjectionCapacityGrowths == before.FileProjectionCapacityGrowths,
				   "routine selection does not reconstruct the full file projection");
			expect(after.FileLookupComparisons - before.FileLookupComparisons <= 16U * 32U,
				   "sixteen selections use logarithmic retained lookups");
			expect(after.AcceptedObservationRows == before.AcceptedObservationRows && value.Observations.Requests.size() == requests,
				   "selection neither reloads nor re-observes the catalogue");
			expect(value.Host.selected() == value.Observations.Files[count - 16U], "indexed selection retains the exact requested identity");
			value.Host.invalidate(authority());
			expect(value.Host.actionable().empty() && !value.Host.selected(), "invalidation clears the retained lookup authority");
			value.Host.observe(authority(18U));
			value.settle();
			value.Host.apply(core::select_workbench_file_intent{ .File = value.Observations.Files.back() });
			expect(value.Host.state().Navigation->Ordinal == count, "reopen reconstructs exact navigation order");
		}
	}

	[[nodiscard]] auto counterpart_entry(const std::string& aPath) -> application::workbench_file_observation_entry
	{
		return { .File = core::file_identity_of(aPath), .PresentationPath = aPath, .CurrentPath = std::filesystem::path{ std::u8string{ aPath.begin(), aPath.end() } }, .Change = application::workbench_file_change_kind::modified, .ContentKind = application::workbench_file_content_kind::text };
	}

	void test_header_source_accepted_catalogue_resolution()
	{
		auto       value            = selected_harness{ };
		const auto source           = counterpart_entry("src/a/b/c/file.cpp");
		const auto header           = counterpart_entry("inc/project/a/b/c/file.H");
		const auto distant          = counterpart_entry("inc/a/b/d/file.hpp");
		const auto renamed          = counterpart_entry("inc/other.h");
		auto       renamedEntry     = renamed;
		renamedEntry.Change         = application::workbench_file_change_kind::renamed;
		renamedEntry.PreviousPath   = std::filesystem::path{ "src/a/b/c/file.h" };
		value.Observations.Entries  = std::vector{ source, distant, renamedEntry, counterpart_entry("src/a/b/c/File.h"), counterpart_entry("src/a/b/c/file.txt") };
		value.Observations.Withheld = { header, header };
		expect(value.Host.header_source_target().Status == core::header_source_status::unavailable_catalogue, "unobserved catalogue is unavailable");
		value.Host.observe(authority());
		value.settle();
		expect(value.Host.header_source_target().Status == core::header_source_status::no_selection, "accepted catalogue with no selection is distinct");
		value.Host.apply(core::select_workbench_file_intent{ source.File });
		value.settle();
		const auto order = std::vector<core::workbench_file_id>{ value.Host.actionable().begin(), value.Host.actionable().end() };
		auto       match = value.Host.header_source_target();
		expect(match.Status == core::header_source_status::matched && match.File == header.File, "owner example prefers longest trailing directory match including a withheld duplicate");
		expect(value.Host.contains_file(header.File) && value.Host.file_observation(header.File) == header, "withheld file retains accepted membership and exact path");
		value.Host.apply(core::select_workbench_file_intent{ header.File });
		value.settle();
		expect(value.SelectedDiffs.Requests.back().File == header && value.Host.header_source_target().File == source.File,
			   "ordinary exact selection loads a withheld counterpart and resolves back without altering the receipt");
		expect(std::vector<core::workbench_file_id>{ value.Host.actionable().begin(), value.Host.actionable().end() } == order, "hidden selection does not change actionable order");
		value.Host.apply(core::select_workbench_file_intent{ renamed.File });
		expect(value.Host.header_source_target().Status == core::header_source_status::no_match, "a renamed previous path is not a matching alias");
		value.Host.apply(core::select_workbench_file_intent{ core::file_identity_of("src/a/b/c/file.txt") });
		expect(value.Host.header_source_target().Status == core::header_source_status::unsupported_file, "unsupported selection is explicit");
		value.Host.invalidate(authority());
		expect(value.Host.header_source_target().Status == core::header_source_status::unavailable_catalogue && !value.Host.contains_file(header.File), "invalidation clears both visible and withheld counterpart membership");
		value.Host.observe(authority(18U));
		value.Observations.Status = application::workbench_observation_status::unavailable;
		value.settle();
		expect(value.Host.header_source_target().Status == core::header_source_status::unavailable_catalogue, "refused replacement never reports no match");
	}

	void test_header_source_preference_ambiguity_and_refresh()
	{
		auto       value           = harness{ };
		const auto source          = counterpart_entry("src/a/b/c/file.cpp");
		const auto one             = counterpart_entry("inc/a/b/c/file.h");
		const auto two             = counterpart_entry("inc/project/a/b/c/file.hpp");
		value.Observations.Entries = std::vector{ source, one, two };
		value.Host.observe(authority());
		value.settle();
		value.Host.apply(core::select_workbench_file_intent{ source.File });
		const auto before = value.Host.state();
		expect(value.Host.header_source_target().Status == core::header_source_status::ambiguous && value.Host.state() == before,
			   "equal best suffix groups are a read-only ambiguous result");
		const auto same = counterpart_entry("src/a/b/c/file.hxx");
		value.Observations.Entries->push_back(same);
		value.Host.refresh_actionability();
		value.settle();
		expect(value.Host.header_source_target().File == same.File, "exact same-directory target takes precedence after catalogue refresh");
		value.Observations.Entries->push_back(counterpart_entry("src/a/b/c/file.hh"));
		value.Host.refresh_actionability();
		value.settle();
		expect(value.Host.header_source_target().Status == core::header_source_status::ambiguous, "multiple same-directory targets do not choose by extension");
		value.Observations.Entries = std::vector{ source };
		value.Host.refresh_actionability();
		value.settle();
		expect(value.Host.header_source_target().Status == core::header_source_status::no_match, "replacement removes retired counterpart candidates");
	}

	void test_header_source_removed_unicode_and_zero_overlap_paths()
	{
		auto       value           = harness{ };
		const auto source          = counterpart_entry("src/a/Wídget.cpp");
		auto       removed         = counterpart_entry("old/b/Wídget.HXX");
		removed.Change             = application::workbench_file_change_kind::removed;
		value.Observations.Entries = std::vector{ source, removed };
		value.Host.observe(authority());
		value.settle();
		value.Host.apply(core::select_workbench_file_intent{ source.File });
		expect(value.Host.header_source_target().File == removed.File, "sole zero-overlap candidate uses its reported removed path and preserves UTF-8 basename bytes");
		value.Host.apply(core::select_workbench_file_intent{ removed.File });
		expect(value.Host.header_source_target().File == source.File, "removed headers resolve bidirectionally from their accepted path");
		value.Observations.Entries = std::vector{ source, counterpart_entry("inc/a/Wídget.h") };
		value.Host.refresh_actionability();
		value.settle();
		value.Host.apply(core::select_workbench_file_intent{ source.File });
		expect(value.Host.header_source_target().File == core::file_identity_of("inc/a/Wídget.h"), "refreshed Unicode counterpart follows current accepted catalogue");
	}

	void test_header_source_queries_are_indexed_even_with_one_shared_basename()
	{
		for (const auto count : { 32U, 4096U }) {
			auto value = selected_harness{ };
			value.Observations.Entries.emplace();
			const auto source   = counterpart_entry("src/a/b/c/file.cpp");
			const auto expected = counterpart_entry("inc/a/b/c/file.h");
			value.Observations.Entries->push_back(source);
			value.Observations.Entries->push_back(expected);
			for (auto index = 2U; index < count; ++index) {
				value.Observations.Entries->push_back(counterpart_entry("inc/other/" + std::to_string(index) + "/file.h"));
			}
			value.Host.observe(authority());
			value.settle();
			value.Host.apply(core::select_workbench_file_intent{ source.File });
			value.settle();
			const auto before      = value.Host.work_statistics();
			const auto reads       = value.SelectedDiffs.Requests.size();
			const auto projections = value.Projections.Published.size();
			for (auto gesture = 0U; gesture < 100U; ++gesture) {
				expect(value.Host.header_source_target().File == expected.File, "indexed query selects the unique closest counterpart");
			}
			const auto after = value.Host.work_statistics();
			expect(after.CounterpartDirectoryLookups - before.CounterpartDirectoryLookups == 400U,
				   "one hundred queries visit four directory components each regardless of catalogue size");
			expect(after.CounterpartIndexedRows == before.CounterpartIndexedRows && after.FileProjectionRows == before.FileProjectionRows
					   && value.SelectedDiffs.Requests.size() == reads && value.Projections.Published.size() == projections,
				   "queries neither reindex, read blobs nor publish file rows");
			std::cout << "header-source-index files=" << count << " queries=100 directory_lookups=" << after.CounterpartDirectoryLookups - before.CounterpartDirectoryLookups << '\n';
		}
	}

} // namespace

auto main() -> int
{
	test_header_source_accepted_catalogue_resolution();
	test_header_source_removed_unicode_and_zero_overlap_paths();
	test_header_source_preference_ambiguity_and_refresh();
	test_header_source_queries_are_indexed_even_with_one_shared_basename();
	test_routine_selection_visits_only_the_retained_index();
	test_retained_actionability_moves_files_in_place();
	test_orderly_shutdown_flushes_latest_layout_serially();
	test_orderly_shutdown_drains_layout_at_request_capacity();
	test_geometry_load_observation_and_orderly_drain();
	test_summary_fraction_load_hold_validation_and_orderly_drain();
	test_summary_reset_before_load_preserves_the_rest_of_the_record();
	test_line_details_height_load_hold_validation_and_orderly_drain();
	test_orderly_shutdown_failure_and_forced_escalation();
	test_the_signed_out_authority_is_valid_and_a_half_named_one_is_not();
	test_invalid_limits_are_rejected();
	test_first_observation_publishes_bounded_cursor_facts();
	test_change_navigation_observation_is_exact_and_bounded();
	test_navigation_resolves_through_the_host_order();
	test_preparation_window_follows_the_cursor();
	test_selected_diff_lifecycle_is_exactly_once_and_typed();
	test_selected_diff_replays_after_context_arrives_and_changes();
	test_file_navigation_keeps_the_accepted_comparison();
	test_same_selected_diff_names_only_the_diff_asked_again();
	test_actionability_refresh_preserves_selected_diff_identity();
	test_failed_authority_refresh_clears_rows_and_old_selection();
	test_exact_invalidation_clears_authority_bound_selection();
	test_drawer_gesture_publishes_and_persists();
	test_resize_clamps_and_persists_the_width();
	test_superseded_terminal_cannot_publish();
	test_duplicate_terminal_is_delivered_once();
	test_forged_terminal_context_is_refused();
	test_terminal_domain_mismatch_is_refused();
	test_cancellation_withholds_publication();
	test_failed_and_inexact_observations();
	test_restore_applies_when_the_workbench_activates();
	test_layout_survives_a_restart();
	test_review_behaviour_persists_publishes_and_refuses_outside_the_vocabulary();
	test_home_view_persists_publishes_and_refuses_outside_the_vocabulary();
	test_a_view_applied_before_the_load_merges_into_the_loaded_record();
	test_orderly_shutdown_flushes_the_latest_home_view();
	test_home_is_not_a_profile_default();
	test_a_home_view_whose_record_cannot_be_read_is_shown_and_reported();
	test_an_orderly_close_reports_a_home_view_whose_record_never_arrived();
	test_the_updates_line_folds_persists_and_restores_per_window();
	test_orderly_shutdown_drains_a_held_fold();
	test_settings_failure_is_reported();
	test_shutdown_withholds_publication_and_quiesces();
	std::cout << "workbench application host tests passed\n";
	return EXIT_SUCCESS;
}
