#include "bpr/application/workbench_snapshot_observation.hpp"

#include "bpr/core/diff_model.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
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

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] auto authority(const std::uint32_t aNumber = 17U)
		-> application::workbench_pull_request_authority
	{
		return application::workbench_pull_request_authority{
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

	[[nodiscard]] auto request(
		const application::workbench_pull_request_authority& aAuthority,
		const std::size_t                                    aMaximum = 64U)
		-> application::workbench_observation_request
	{
		return application::workbench_observation_request{
			.Context = application::workbench_request_context{
				.Request    = core::request_id{ 7U },
				.Session    = core::application_session_id{ 3U },
				.Generation = core::request_generation{ 2U },
			},
			.Authority    = aAuthority,
			.MaximumFiles = aMaximum,
		};
	}

	[[nodiscard]] auto file(const std::string_view aPath)
		-> application::github_file_record
	{
		auto record        = application::github_file_record{ };
		record.Path        = std::string{ aPath };
		record.Change      = application::github_file_change::modified;
		record.ContentKind = application::github_file_content_kind::text;
		return record;
	}

	[[nodiscard]] auto supplying(std::vector<application::github_file_record> aFiles)
		-> application::snapshot_workbench_observation::file_supplier
	{
		return [files = std::move(aFiles)](const application::workbench_pull_request_authority&) {
			return std::optional{ files };
		};
	}

	auto test_the_answer_repeats_the_request_exactly() -> void
	{
		auto observation = application::snapshot_workbench_observation{
			supplying({ file("a.cpp"), file("b.cpp") })
		};
		const auto owner    = authority();
		const auto asked    = request(owner);
		const auto answered = observation.assemble(asked, { });

		// The host refuses a terminal whose context or authority does not match
		// the request it issued. An answer that forgot to copy them is a
		// misrouted answer, not a reported failure.
		expect(answered.Context == asked.Context,
			   "the answer repeats the request context exactly");
		expect(answered.Authority == asked.Authority,
			   "the answer repeats the request authority exactly");
		expect(answered.Status == application::workbench_observation_status::available,
			   "a supplied review is available");
		expect(answered.Actionable.size() == 2U,
			   "every file in the review is reported");
	}

	auto test_the_order_is_the_snapshot_order_and_names_are_derived() -> void
	{
		const auto paths = std::vector<std::string>{
			"src/zeta.cpp", "src/alpha.cpp", "docs/readme.md"
		};
		auto files = std::vector<application::github_file_record>{ };
		for (const auto& path : paths) {
			files.push_back(file(path));
		}

		auto observation = application::snapshot_workbench_observation{
			supplying(files)
		};
		const auto answered = observation.assemble(request(authority()), { });

		expect(answered.Actionable.size() == paths.size(),
			   "one entry per file");
		for (std::size_t index = 0U; index < paths.size(); ++index) {
			// Compared against the core's own derivation rather than a value
			// restated here, so the adapter cannot drift from the single
			// function RFC-BPR-0007's receipts are keyed by.
			expect(answered.Actionable[index].File
					   == core::file_identity_of(paths[index]),
				   "each entry is the core's identity for its path");
		}
		// Presentation order, not sorted. Sorting here would disagree with
		// what the reviewer sees listed and make "next file" jump.
		expect(answered.Actionable[0] != answered.Actionable[1],
			   "the snapshot's own order is preserved rather than sorted");
	}

	auto test_a_repeated_path_appears_once() -> void
	{
		auto observation = application::snapshot_workbench_observation{
			supplying({ file("a.cpp"), file("b.cpp"), file("a.cpp") })
		};
		const auto answered = observation.assemble(request(authority()), { });

		// The port owns uniqueness; the host resolves the first matching entry.
		// A repeated path would otherwise put one file in the order twice and
		// make navigation revisit it.
		expect(answered.Actionable.size() == 2U,
			   "a snapshot repeating a path yields one entry for it");
		expect(answered.Actionable[0].File == core::file_identity_of("a.cpp")
				   && answered.Actionable[1].File == core::file_identity_of("b.cpp"),
			   "and the first occurrence keeps its position");
	}

	auto test_too_many_files_is_reported_and_never_truncated() -> void
	{
		auto files = std::vector<application::github_file_record>{ };
		for (auto index = 0U; index < 5U; ++index) {
			files.push_back(file("file-" + std::to_string(index) + ".cpp"));
		}

		auto observation = application::snapshot_workbench_observation{
			supplying(files)
		};
		const auto answered = observation.assemble(
			request(authority(), 4U), { });

		expect(answered.Status
				   == application::workbench_observation_status::limit_exceeded,
			   "a review beyond the requested bound reports limit_exceeded");
		// Asserting the status alone is not enough: a truncating implementation
		// could report it and still hand back four rows, and the core would
		// take the fourth for the last file there is.
		expect(answered.Actionable.empty(),
			   "and hands back no partial order at all");
		expect(answered.Context.Request == core::request_id{ 7U },
			   "the refusal still repeats the request");

		const auto exact = observation.assemble(request(authority(), 5U), { });
		expect(exact.Status == application::workbench_observation_status::available
				   && exact.Actionable.size() == 5U,
			   "the bound itself is acceptable, so it is inclusive");
	}

	auto test_an_absent_snapshot_differs_from_an_empty_review() -> void
	{
		auto absent = application::snapshot_workbench_observation{
			[](const application::workbench_pull_request_authority&) {
				return std::optional<
					std::vector<application::github_file_record>>{ };
			}
		};
		expect(absent.assemble(request(authority()), { }).Status
				   == application::workbench_observation_status::unavailable,
			   "a snapshot that is not held is unavailable");

		auto       empty    = application::snapshot_workbench_observation{ supplying({ }) };
		const auto answered = empty.assemble(request(authority()), { });
		// Both cases are constructed. Collapsing them would make "not loaded
		// yet" indistinguishable from "nothing to review", and the reviewer
		// would be told a busy pull request was empty.
		expect(answered.Status
					   == application::workbench_observation_status::available
				   && answered.Actionable.empty(),
			   "a review with no files is available and empty, not unavailable");
	}

	auto test_an_unnameable_path_fails_the_whole_answer() -> void
	{
		auto observation = application::snapshot_workbench_observation{
			supplying({ file("good.cpp"),
						file(std::string(
							core::maximum_identified_path_bytes + 1U, 'p')) })
		};
		const auto answered = observation.assemble(request(authority()), { });

		// Not a shorter list. Omitting the record would hide a file from the
		// reviewer entirely, and the read model already bounds path length, so
		// reaching this means the snapshot is malformed rather than large.
		expect(answered.Status
				   == application::workbench_observation_status::unavailable,
			   "a path this build cannot name makes the whole answer "
			   "unavailable rather than silently dropping the file");
		expect(answered.Actionable.empty(),
			   "and no partial order survives the refusal");
	}

	auto test_cancellation_is_reported() -> void
	{
		auto observation = application::snapshot_workbench_observation{
			supplying({ file("a.cpp") })
		};
		auto stop = std::stop_source{ };
		stop.request_stop();

		const auto answered
			= observation.assemble(request(authority()), stop.get_token());
		expect(answered.Status
				   == application::workbench_observation_status::cancelled,
			   "a cancelled observation reports cancellation");
		expect(answered.Actionable.empty(),
			   "and reports no order");
		expect(answered.Authority == authority(),
			   "a cancelled answer still repeats its authority");
	}

	auto test_only_exact_accepted_ledger_facts_filter_actionability() -> void
	{
		const auto selected    = core::file_identity_of("read.cpp");
		const auto material    = opaque<core::file_material_id>(91U);
		auto       observation = application::snapshot_workbench_observation{
			supplying({ file("read.cpp"), file("unknown.cpp") }),
			[selected, material](
				const application::workbench_pull_request_authority&,
				const core::file_identity aFile)
				-> std::optional<core::actionability_input> {
				if (aFile != selected) {
					return std::nullopt;
				}
				return core::actionability_input{
					.Receipt = core::file_receipt{
						.File     = selected,
						.Material = material,
						.Read     = core::file_read_state::read,
					},
				};
			}
		};

		const auto answered = observation.assemble(request(authority()), { });
		expect(answered.Actionable.size() == 1U
				   && answered.Actionable.front().File
					   == core::file_identity_of("unknown.cpp"),
			   "read exact material leaves the queue while unknown material stays conservatively actionable");

		auto mismatched = application::snapshot_workbench_observation{
			supplying({ file("read.cpp") }),
			[material](
				const application::workbench_pull_request_authority&,
				const core::file_identity)
				-> std::optional<core::actionability_input> {
				return core::actionability_input{
					.Receipt = core::file_receipt{
						.File     = core::file_identity_of("different.cpp"),
						.Material = material,
						.Read     = core::file_read_state::read,
					},
				};
			}
		};
		expect(mismatched.assemble(request(authority()), { }).Actionable.size()
				   == 1U,
			   "a receipt for another file cannot hide the current row");
	}
} // namespace

auto main() -> int
{
	test_the_answer_repeats_the_request_exactly();
	test_the_order_is_the_snapshot_order_and_names_are_derived();
	test_a_repeated_path_appears_once();
	test_too_many_files_is_reported_and_never_truncated();
	test_an_absent_snapshot_differs_from_an_empty_review();
	test_an_unnameable_path_fails_the_whole_answer();
	test_cancellation_is_reported();
	test_only_exact_accepted_ledger_facts_filter_actionability();
	std::cout << "workbench-snapshot-observation-tests:pass\n";
	return EXIT_SUCCESS;
}
