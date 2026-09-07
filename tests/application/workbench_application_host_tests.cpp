#include "bpr/application/manual_executor.hpp"
#include "bpr/application/workbench_application_host.hpp"

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
					.ContentKind      = application::workbench_file_content_kind::text,
				});
			}
			auto observation = application::workbench_file_observation{
				.Context    = aRequest.Context,
				.Authority  = aRequest.Authority,
				.Status     = Status,
				.Actionable = std::move(files),
			};
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

		std::vector<core::workbench_file_id>                    Files;
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

		application::workbench_layout_settings Stored{ };
		bool                                   Available = true;
		std::size_t                            Loads     = 0U;
		std::size_t                            Stores    = 0U;
		std::vector<bool>                      LoadStopObserved;
		std::vector<bool>                      StoreStopObserved;
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
			expect(previousCommand.Enabled == aPrevious
					   && nextCommand.Enabled == aNext,
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
		expect(selection.Selection.has_value() && !selection.Commands.has_value()
				   && !selection.Drawers.has_value() && !selection.Layout.has_value(),
			   "a bounded selection change publishes the selection domain only");
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
		expect(!next.Enabled
				   && next.DisabledReason == core::workbench_reason_code::at_last_file,
			   "the core disabled the exhausted command");

		harnessed.Host.apply(core::navigate_workbench_file_intent{
			.Direction = core::change_navigation_direction::next });
		expect(harnessed.Projections.Published.size() == 3U,
			   "a rejected gesture publishes nothing");
		expect(harnessed.Host.last_reason()
				   == core::workbench_reason_code::at_last_file,
			   "the host reports the core rejection without re-deciding it");
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
					   == application::workbench_host_failure::selected_file_stale,
			   "the accepted selection is published before its typed stale failure");

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
			.OpenModality     = application::review_open_modality::read,
			.ContinueModality = application::review_continue_modality::review,
			.MarkRead         = application::review_mark_read_behaviour::when_fully_seen,
			.ReturnComparison = application::review_return_comparison::since_last_review,
			.CommentsFilter   = application::review_comments_filter::unread,
		};
		{
			auto harnessed = harness{ { }, &durable };
			harnessed.Host.apply_behaviour(chosen);
			expect(harnessed.Projections.Published.size() == 1U
					   && harnessed.Projections.Published.back().Behaviour == std::optional{ chosen },
				   "a behaviour change publishes the chosen record");
			harnessed.settle();
			expect(durable.Stores == 1U && durable.Stored.Behaviour == chosen,
				   "the change is persisted beside the layout");
			auto narrowed           = chosen;
			narrowed.CommentsFilter = application::review_comments_filter::unresolved;
			harnessed.Host.apply_behaviour(narrowed);
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
			first.settle();
		}
		expect(durable.Stored.Conversations == core::workbench_drawer_state::collapsed,
			   "the durable record kept the collapsed drawer");
		expect(durable.Stored.ConversationsWidth == 500U,
			   "the durable record kept the width");
		expect(durable.Stored.LineNumbers == core::workbench_line_numbers::hidden,
			   "the durable record kept the source-pane numbering");

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
} // namespace

auto main() -> int
{
	test_invalid_limits_are_rejected();
	test_first_observation_publishes_bounded_cursor_facts();
	test_change_navigation_observation_is_exact_and_bounded();
	test_navigation_resolves_through_the_host_order();
	test_selected_diff_lifecycle_is_exactly_once_and_typed();
	test_selected_diff_replays_after_context_arrives_and_changes();
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
	test_settings_failure_is_reported();
	test_shutdown_withholds_publication_and_quiesces();
	std::cout << "workbench application host tests passed\n";
	return EXIT_SUCCESS;
}
