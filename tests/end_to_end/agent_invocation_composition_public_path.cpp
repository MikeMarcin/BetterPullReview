#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"

#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>
#include <QTreeView>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	using e2e::default_agent_budget;
	using e2e::extended_agent_budget;
	using e2e::wait_terms;
	inline constexpr std::string_view workspace_failure_marker
		= ".bpr-fail-workspace-pack";
	inline constexpr std::string_view redacted_git_diagnostic
		= "fatal: could not materialize /tmp/bpr-agent-workspace from "
		  "<redacted-url>\n";

	// The course's own budgets, unchanged, including the six sites that have
	// always asked for the longer one. This used to be a second copy of the queue
	// course's loop, differing only in whether the budget was a parameter
	// (BPR-0381).
	template <typename Predicate>
	[[nodiscard]] auto wait_until_reporting(
		wait_terms&                     aTerms,
		Predicate                       aPredicate,
		const std::chrono::milliseconds aBudget = default_agent_budget) -> bool
	{
		return e2e::wait_until(aTerms, aPredicate, aBudget);
	}

	template <typename Predicate>
	[[nodiscard]] auto wait_until(
		Predicate                       aPredicate,
		const std::chrono::milliseconds aBudget = default_agent_budget) -> bool
	{
		return e2e::wait_until_plain(aPredicate, aBudget);
	}

	[[nodiscard]] auto shutdown(app::github_application& aApplication) -> bool
	{
		aApplication.begin_shutdown();
		return wait_until([&aApplication] {
			return aApplication.status().ShutdownComplete;
		});
	}

	/// \brief Reports a failed condition without shutting anything down.
	///
	/// \details Separate from `fail` because the cleanup-retry shutdown wait is
	/// reached after `begin_shutdown` has already been called, where `fail`'s own
	/// shutdown wait would be a second one. That site used to return
	/// `EXIT_FAILURE` having printed nothing at all (BPR-0381).
	void report(const std::string_view aReason, const std::string_view aState = { })
	{
		std::cerr << "agent-invocation-composition-failed:" << aReason;
		if (!aState.empty()) {
			std::cerr << ' ' << aState;
		}
		std::cerr << '\n';
	}

	auto fail(
		app::github_application& aApplication,
		const std::string_view   aReason,
		const std::string_view   aState = { }) -> int
	{
		report(aReason, aState);
		static_cast<void>(shutdown(aApplication));
		return EXIT_FAILURE;
	}

	/// \brief Names a typed startup refusal for the failure report.
	///
	/// \details [[BPR-0808]]. The startup wait settles on any terminal, so a
	/// refusal left its report empty and `application-startup-failed:` named
	/// nothing. The switch has no default, so a new failure fails the build here
	/// instead of printing an empty reason again.
	[[nodiscard]] auto startup_failure_name(const app::github_application_startup_failure aFailure) noexcept -> std::string_view
	{
		using enum app::github_application_startup_failure;
		switch (aFailure) {
		case owner_busy:
			return "owner_busy";
		case activation_queued:
			return "activation_queued";
		case activation_refused:
			return "activation_refused";
		case activation_indeterminate:
			return "activation_indeterminate";
		case invalid_profile:
			return "invalid_profile";
		case registry:
			return "registry";
		case root:
			return "root";
		case unavailable:
			return "unavailable";
		case cancelled:
			return "cancelled";
		}
		return "unnamed";
	}

	[[nodiscard]] auto line_value(
		const std::string_view aText,
		const std::string_view aPrefix) -> std::optional<std::string>
	{
		const auto begin = aText.find(aPrefix);
		if (begin == std::string_view::npos) {
			return std::nullopt;
		}
		const auto valueBegin = begin + aPrefix.size();
		const auto end        = aText.find('\n', valueBegin);
		return std::string{ aText.substr(
			valueBegin,
			end == std::string_view::npos ? aText.size() - valueBegin
										  : end - valueBegin) };
	}

	using context_fields = std::map<std::string, std::string>;

	[[nodiscard]] auto decode_hex(const std::string_view aValue)
		-> std::optional<std::string>
	{
		if (aValue.size() % 2U != 0U) {
			return std::nullopt;
		}
		const auto nibble = [](const char aDigit) -> std::optional<std::uint8_t> {
			if (aDigit >= '0' && aDigit <= '9') {
				return static_cast<std::uint8_t>(aDigit - '0');
			}
			if (aDigit >= 'a' && aDigit <= 'f') {
				return static_cast<std::uint8_t>(aDigit - 'a' + 10);
			}
			return std::nullopt;
		};
		auto decoded = std::string{ };
		decoded.reserve(aValue.size() / 2U);
		for (auto index = std::size_t{ 0U }; index < aValue.size(); index += 2U) {
			const auto high = nibble(aValue[index]);
			const auto low  = nibble(aValue[index + 1U]);
			if (!high.has_value() || !low.has_value()) {
				return std::nullopt;
			}
			decoded.push_back(static_cast<char>((static_cast<unsigned>(*high) << 4U) | *low));
		}
		return decoded;
	}

	[[nodiscard]] auto unescape_field(const std::string_view aValue)
		-> std::optional<std::string>
	{
		auto decoded = std::string{ };
		decoded.reserve(aValue.size());
		for (auto index = std::size_t{ 0U }; index < aValue.size(); ++index) {
			if (aValue[index] != '\\') {
				decoded.push_back(aValue[index]);
				continue;
			}
			if (++index == aValue.size()) {
				return std::nullopt;
			}
			switch (aValue[index]) {
			case 'n':
				decoded.push_back('\n');
				break;
			case 'r':
				decoded.push_back('\r');
				break;
			case 't':
				decoded.push_back('\t');
				break;
			case '\\':
				decoded.push_back('\\');
				break;
			default:
				return std::nullopt;
			}
		}
		return decoded;
	}

	[[nodiscard]] auto parse_context_fields(const std::string_view aPayload)
		-> std::optional<context_fields>
	{
		if (aPayload.empty() || aPayload.back() != '\n') {
			return std::nullopt;
		}
		auto fields = context_fields{ };
		auto begin  = std::size_t{ 0U };
		while (begin < aPayload.size()) {
			const auto end = aPayload.find('\n', begin);
			if (end == std::string_view::npos || end == begin) {
				return std::nullopt;
			}
			const auto line = aPayload.substr(begin, end - begin);
			const auto tab  = line.find('\t');
			if (tab == std::string_view::npos || tab == 0U) {
				return std::nullopt;
			}
			auto value = unescape_field(line.substr(tab + 1U));
			if (
				!value.has_value()
				|| !fields.emplace(std::string{ line.substr(0U, tab) }, std::move(*value))
						.second) {
				return std::nullopt;
			}
			begin = end + 1U;
		}
		return fields;
	}

	template <typename Identity>
	[[nodiscard]] auto identity_hex(const Identity& aIdentity) -> std::string
	{
		constexpr std::string_view digits = "0123456789abcdef";
		auto                       result = std::string{ };
		result.reserve(aIdentity.bytes().size() * 2U);
		for (const auto byte : aIdentity.bytes()) {
			result.push_back(digits[byte >> 4U]);
			result.push_back(digits[byte & 0x0FU]);
		}
		return result;
	}

	template <typename Value>
	void put_number(
		context_fields& aFields, const std::string& aName, const Value aValue)
	{
		aFields.emplace(aName, std::to_string(aValue));
	}

	template <typename Identity>
	void put_optional_identity(
		context_fields&                aFields,
		const std::string&             aPrefix,
		const std::optional<Identity>& aIdentity)
	{
		put_number(aFields, aPrefix + ".present", aIdentity.has_value() ? 1 : 0);
		if (aIdentity.has_value()) {
			aFields.emplace(aPrefix + ".value", identity_hex(*aIdentity));
		}
	}

	void put_source_line(
		context_fields&              aFields,
		const std::string&           aPrefix,
		const core::source_line_ref& aLine)
	{
		aFields.emplace(aPrefix + ".comparison", identity_hex(aLine.Comparison));
		put_number(
			aFields,
			aPrefix + ".side",
			static_cast<std::uint8_t>(aLine.Side));
		put_number(aFields, aPrefix + ".line", aLine.Line);
	}

	[[nodiscard]] auto expected_context_fields(
		const app::detail::agent_invocation_context_evidence& aEvidence)
		-> context_fields
	{
		auto fields = context_fields{ };
		put_number(fields, "schema", core::agent_supplement_schema_version);
		put_number(
			fields, "application_session", aEvidence.ApplicationSession.value());
		put_number(fields, "session", aEvidence.NextSession.value());
		put_number(
			fields, "profile", core::external_terminal_profile_id.value());
		put_number(fields, "host", aEvidence.Binding.Host.value());
		put_number(fields, "repository", aEvidence.Binding.Repository.value());
		put_number(fields, "pull_request", aEvidence.Binding.PullRequest.value());
		fields.emplace("pull_request_url", aEvidence.PullRequestUrl);
		// BPR-0517: the supplement carries the side-chat message in one position
		// after the pull request URL; an invocation from the launch panel sends
		// none, so the field is present and empty.
		fields.emplace("chat_message", std::string{ });
		// BPR-0533: and the workshop operation in the one position after it.
		// An invocation from the launch panel is not a workshop request, so
		// the field is present and empty.
		fields.emplace("workshop_operation", std::string{ });
		fields.emplace("base", std::string{ core::canonical_hex(aEvidence.Binding.BaseOid).view() });
		fields.emplace("head", std::string{ core::canonical_hex(aEvidence.Binding.HeadOid).view() });
		put_number(fields, "left_epoch", aEvidence.Binding.LeftEpoch.value());
		put_number(fields, "right_epoch", aEvidence.Binding.RightEpoch.value());
		put_number(
			fields,
			"previous_upload.present",
			aEvidence.PreviousUpload.has_value() ? 1 : 0);
		if (aEvidence.PreviousUpload.has_value()) {
			fields.emplace(
				"previous_upload.value",
				std::string{ core::canonical_hex(*aEvidence.PreviousUpload).view() });
		}
		put_number(
			fields,
			"last_reviewed_head.present",
			aEvidence.Private.LastReviewedHead.has_value() ? 1 : 0);
		if (aEvidence.Private.LastReviewedHead.has_value()) {
			fields.emplace(
				"last_reviewed_head.value",
				std::string{ core::canonical_hex(
								 *aEvidence.Private.LastReviewedHead)
								 .view() });
		}

		put_number(fields, "file.count", aEvidence.Files.size());
		for (auto index = std::size_t{ 0U }; index < aEvidence.Files.size(); ++index) {
			const auto& file   = aEvidence.Files[index];
			const auto  prefix = "file." + std::to_string(index);
			fields.emplace(prefix + ".identity", identity_hex(file.Receipt.File));
			fields.emplace(prefix + ".material", identity_hex(file.Receipt.Material));
			fields.emplace(prefix + ".path", file.Path);
			put_number(
				fields,
				prefix + ".read",
				static_cast<std::uint8_t>(file.Receipt.Read));
			put_number(
				fields,
				prefix + ".changed_since_review",
				file.ChangedSinceReview ? 1 : 0);
			put_number(fields, prefix + ".accepted", file.Receipt.Accepted ? 1 : 0);
			put_number(fields, prefix + ".excluded", file.Receipt.Excluded ? 1 : 0);
		}

		put_number(fields, "defect.count", aEvidence.Private.Defects.size());
		for (
			auto index = std::size_t{ 0U };
			index < aEvidence.Private.Defects.size();
			++index) {
			const auto& defect = aEvidence.Private.Defects[index];
			const auto  prefix = "defect." + std::to_string(index);
			fields.emplace(prefix + ".file", identity_hex(defect.File));
			put_number(fields, prefix + ".conversation", defect.Conversation.value());
			put_number(fields, prefix + ".id", defect.History.Defect.Id.value());
			put_source_line(fields, prefix + ".anchor", defect.History.Defect.Anchor);
			put_source_line(
				fields, prefix + ".original_anchor", defect.History.OriginalAnchor);
			put_number(
				fields,
				prefix + ".severity",
				static_cast<std::uint8_t>(defect.History.Defect.Severity));
			put_number(
				fields,
				prefix + ".category",
				static_cast<std::uint8_t>(defect.History.Defect.Category));
			put_number(
				fields,
				prefix + ".lifecycle",
				static_cast<std::uint8_t>(defect.History.Defect.State));
			put_number(
				fields,
				prefix + ".availability",
				static_cast<std::uint8_t>(defect.History.Availability));
		}

		put_number(
			fields, "selection.present", aEvidence.Selection.has_value() ? 1 : 0);
		if (aEvidence.Selection.has_value()) {
			const auto& selection = *aEvidence.Selection;
			fields.emplace("selection.file", identity_hex(selection.File));
			fields.emplace(
				"selection.comparison",
				identity_hex(selection.Selection.Comparison));
			put_number(
				fields,
				"selection.kind",
				static_cast<std::uint8_t>(selection.Selection.Kind));
			put_source_line(
				fields, "selection.preferred", selection.Selection.PreferredLine);
			put_optional_identity(
				fields, "selection.segment", selection.Selection.Segment);
			put_optional_identity(
				fields, "selection.group", selection.Selection.Group);
			put_number(
				fields,
				"selection.mapping_confidence",
				static_cast<std::uint8_t>(selection.Confidence));
			put_number(
				fields,
				"selection.source_text.present",
				selection.SelectedSourceText.has_value() ? 1 : 0);
			if (selection.SelectedSourceText.has_value()) {
				fields.emplace(
					"selection.source_text.value", *selection.SelectedSourceText);
			}
			put_number(fields, "selection.segment.count", selection.Segments.size());
			for (
				auto index = std::size_t{ 0U };
				index < selection.Segments.size();
				++index) {
				const auto& segment = selection.Segments[index];
				const auto  prefix  = "selection.segment." + std::to_string(index);
				fields.emplace(prefix + ".id", identity_hex(segment.Id));
				put_number(
					fields,
					prefix + ".kind",
					static_cast<std::uint8_t>(segment.Kind));
				put_number(fields, prefix + ".left.begin", segment.Left.Begin);
				put_number(fields, prefix + ".left.end", segment.Left.End);
				put_number(fields, prefix + ".right.begin", segment.Right.Begin);
				put_number(fields, prefix + ".right.end", segment.Right.End);
				put_optional_identity(fields, prefix + ".group", segment.Group);
			}
			put_number(fields, "selection.group.count", selection.Groups.size());
			for (
				auto index = std::size_t{ 0U };
				index < selection.Groups.size();
				++index) {
				const auto& group  = selection.Groups[index];
				const auto  prefix = "selection.group." + std::to_string(index);
				fields.emplace(prefix + ".id", identity_hex(group.Id));
				put_number(fields, prefix + ".members.begin", group.Members.Begin);
				put_number(fields, prefix + ".members.end", group.Members.End);
				put_number(fields, prefix + ".left.begin", group.Left.Begin);
				put_number(fields, prefix + ".left.end", group.Left.End);
				put_number(fields, prefix + ".right.begin", group.Right.Begin);
				put_number(fields, prefix + ".right.end", group.Right.End);
				put_number(fields, prefix + ".first.left", group.First.Left);
				put_number(fields, prefix + ".first.right", group.First.Right);
				put_number(fields, prefix + ".last.left", group.Last.Left);
				put_number(fields, prefix + ".last.right", group.Last.Right);
				put_number(
					fields,
					prefix + ".kind",
					static_cast<std::uint8_t>(group.Kind));
				put_number(fields, prefix + ".added", group.AddedLines);
				put_number(fields, prefix + ".deleted", group.DeletedLines);
				put_number(
					fields, prefix + ".replaced_left", group.ReplacedLeftLines);
				put_number(
					fields, prefix + ".replaced_right", group.ReplacedRightLines);
			}
		}
		const auto selectedNote = std::ranges::find(
			aEvidence.Private.Notes,
			application::local_note_id{ 246U },
			&application::local_note_record::Id);
		if (selectedNote == aEvidence.Private.Notes.end()
			|| !aEvidence.CurrentDraft.has_value()) {
			return { };
		}
		put_number(fields, "note.count", 1);
		put_optional_identity(fields, "note.0.file", std::optional<core::file_identity>{ });
		put_number(fields, "note.0.comparison.present", 0);
		fields.emplace("note.0.text", selectedNote->Note.Text);
		put_number(fields, "draft.count", 1);
		put_number(fields, "draft.0.anchor.present", 1);
		put_source_line(fields, "draft.0.anchor", aEvidence.CurrentDraft->Draft.Anchor);
		fields.emplace("draft.0.text", aEvidence.CurrentDraft->Draft.Body);
		return fields;
	}

	[[nodiscard]] auto contains_forbidden_context(
		const context_fields& aFields) -> bool
	{
		constexpr auto forbidden = std::array{
			std::string_view{ "BPR_UNSELECTED_NOTE_0250" },
			std::string_view{ "BPR_UNSELECTED_DRAFT_0247" },
			std::string_view{ "BPR_CREDENTIAL_CANARY_0247" },
			std::string_view{ "viewport" },
			std::string_view{ "coordinate" },
			std::string_view{ "qt_index" },
			std::string_view{ "gutter_pixel" },
			std::string_view{ "scroll_coordinate" },
		};
		return std::ranges::any_of(aFields, [&forbidden](const auto& aField) {
			return std::ranges::any_of(forbidden, [&aField](const auto aCanary) {
				return aField.first.contains(aCanary)
					|| aField.second.contains(aCanary);
			});
		});
	}

	[[nodiscard]] auto exact_agent_process_launcher_path(
		const app::detail::agent_invocation_composition_evidence& aEvidence,
		const bool                                                aStartGestureObserved) noexcept -> bool
	{
		return aEvidence.AgentProcessLauncherComposed
			&& aEvidence.HostComposed && aStartGestureObserved
			&& aEvidence.AgentProcessLauncherInvocations == 1U
			&& aEvidence.Statistics.ProcessStarts == 1U;
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	if (argc < 2) {
		return EXIT_FAILURE;
	}
	const auto course              = std::string_view{ argv[1] };
	const auto workspaceDiagnostic = course == "workspace-diagnostic";
	const auto crashMaterializing  = course == "crash-materializing-init" || course == "crash-materializing-checkout" || course == "cancel-materializing-checkout";
	if (
		(workspaceDiagnostic && argc != 4)
		|| (crashMaterializing && argc != 5)
		|| (!workspaceDiagnostic && !crashMaterializing && argc != 3 && argc != 4)) {
		return EXIT_FAILURE;
	}
	if (course != "positive" && course != "positive-seed"
		&& course != "crash-running" && course != "crash-reinvoke"
		&& course != "startup-recover" && course != "startup-repair"
		&& course != "startup-owner-busy" && course != "positive-reopen" && course != "construction-cut"
		&& course != "launcher-construction-cut"
		&& course != "gesture-cut" && course != "cancellation"
		&& course != "staleness" && course != "cleanup-retry"
		&& !workspaceDiagnostic && !crashMaterializing) {
		return EXIT_FAILURE;
	}
	constexpr std::string_view selectedNote
		= "BPR_SELECTED_NOTE_0250 verify exact retry exhaustion";
	constexpr std::string_view unselectedNoteCanary
		= "BPR_UNSELECTED_NOTE_0250 viewport coordinate qt_index "
		  "BPR_CREDENTIAL_CANARY_0250";
	constexpr std::string_view selectedDraft
		= "BPR_SELECTED_DRAFT_0250 would a bounded retry be safer?";
	auto expectedContext
		= std::optional<app::detail::agent_invocation_context_evidence>{ };
	const auto externalRoot = crashMaterializing || (argc == 4 && !workspaceDiagnostic);
	auto       data         = externalRoot ? nullptr : std::make_unique<QTemporaryDir>();
	if (data && !data->isValid()) {
		return EXIT_FAILURE;
	}
	// A root the driver passes in is the driver's to arrange; this course only
	// makes it private. Its own root is one owned level below its temporary
	// directory ([[BPR-1050]]), because the agent workspace neighbourhood beside
	// the root refuses a parent another principal can write.
	const auto privateRoot = [&]() -> std::optional<std::filesystem::path> {
		if (data) {
			return e2e::private_data_root(*data);
		}
		auto            root = std::filesystem::path{ argv[crashMaterializing ? 4 : 3] };
		std::error_code rootError;
		std::filesystem::create_directories(root, rootError);
		if (rootError || !adapters::sqlite::make_storage_root_owner_private(root)) {
			return std::nullopt;
		}
		return root;
	}();
	if (!privateRoot) {
		std::cerr << "agent-invocation-composition-failure:private-test-root\n";
		return EXIT_FAILURE;
	}
	const auto& dataRoot = *privateRoot;

	auto diagnosticGitRoot = workspaceDiagnostic ? std::make_unique<QTemporaryDir>() : nullptr;
	if (diagnosticGitRoot && !diagnosticGitRoot->isValid()) {
		return EXIT_FAILURE;
	}
	auto diagnosticGit = std::optional<std::filesystem::path>{ };
	if (workspaceDiagnostic || crashMaterializing) {
		const auto source = std::filesystem::path{ argv[3] };
		// The staging directory carries a non-ASCII component so the ordinary
		// run of this course exercises the exec-path reply encoding: the
		// native-code-page and UTF-8 encodings of this name differ on Windows,
		// so the course fails there if either handshake side regresses to the
		// code page (BPR-0476). The root arrives from Qt as UTF-16; the narrow
		// toStdString() round-trip would mis-decode the component on Windows
		// before the wrapper ever ran.
		const auto      stagedRoot = crashMaterializing ? dataRoot / "git-wrapper" : std::filesystem::path{ diagnosticGitRoot->path().toStdU16String() } / std::filesystem::path{ u8"wräpper" };
		std::error_code stageError;
		std::filesystem::create_directories(stagedRoot, stageError);
		if (stageError) {
			return EXIT_FAILURE;
		}
		auto            target = stagedRoot / source.filename();
		std::error_code copyError;
		std::filesystem::copy_file(
			source,
			target,
			std::filesystem::copy_options::overwrite_existing,
			copyError);
		if (copyError) {
			return EXIT_FAILURE;
		}
		std::filesystem::permissions(
			target,
			std::filesystem::status(source).permissions(),
			std::filesystem::perm_options::replace,
			copyError);
		if (copyError) {
			return EXIT_FAILURE;
		}
		diagnosticGit = std::filesystem::canonical(target, copyError);
		if (copyError) {
			return EXIT_FAILURE;
		}
	}
	auto externalExecutable = std::filesystem::path{ argv[2] };
	if (
		course == "cancellation" || course == "staleness"
		|| course == "cleanup-retry" || course == "crash-running") {
		const auto mode = course == "crash-running" ? "crash-descendant-wait"
			: course == "cancellation"
			? "descendant-wait"
			: course == "staleness" ? "stale-wait"
									: "cleanup-retry-wait";
		// The copy keeps the source's extension: the canary re-spawns itself
		// through the CRT on Windows, which appends ".exe" to an
		// extensionless name and then cannot find the file this copy
		// actually created (BPR-0240).
		externalExecutable = dataRoot
			/ ("better_pull_review_agent_canary-" + std::string{ mode }
			   + std::filesystem::path{ argv[2] }.extension().string());
		std::error_code copyError;
		std::filesystem::copy_file(
			argv[2],
			externalExecutable,
			std::filesystem::copy_options::overwrite_existing,
			copyError);
		if (copyError) {
			return EXIT_FAILURE;
		}
		std::filesystem::permissions(
			externalExecutable,
			std::filesystem::status(argv[2]).permissions(),
			std::filesystem::perm_options::replace,
			copyError);
		if (copyError) {
			return EXIT_FAILURE;
		}
	}
	std::unique_ptr<app::github_application> application;
	auto                                     startupFailure  = std::optional<app::github_application_startup_failure>{ };
	auto                                     startupTerminal = false;
	auto                                     startup         = app::detail::github_application_fixture_access::start(
		app::detail::github_public_path_fixture::github_com,
		{
			.ApplicationDataRoot = dataRoot,
			.EpochPhase          = app::detail::github_epoch_fixture_phase::single,
			.DisableAgentProcessLauncherComposition
			= course == "launcher-construction-cut",
			.DisableAgentHostComposition = course == "construction-cut",
			.DisableAgentGestureWire     = course == "gesture-cut",
			.ExternalAgentExecutable     = externalExecutable,
			.GitExecutable               = diagnosticGit,
		},
		[&](auto aResult) {
			startupTerminal = true;
			if (aResult) {
				application = std::move(*aResult);
			} else {
				startupFailure = aResult.error();
			}
		});
	auto       startupTerms   = wait_terms{ };
	const auto startupSettled = wait_until_reporting(startupTerms, [&](wait_terms& aTerms) {
		return aTerms.require("startup-terminal", startupTerminal && startup && startup->is_quiescent());
	});
	// [[BPR-0808]]. A second ordinary application on a live owner's root is
	// refused at profile admission, before any service or window exists, so it
	// can neither launch nor reclaim what the owner holds.
	if (course == "startup-owner-busy") {
		if (!startupSettled || application || startupFailure != app::github_application_startup_failure::owner_busy) {
			startupTerms.note_name("failure", startupFailure ? startup_failure_name(*startupFailure) : "none");
			startupTerms.note("application", application != nullptr);
			std::cerr << "application-startup-not-refused:" << startupTerms.render() << '\n';
			if (application) {
				static_cast<void>(shutdown(*application));
			}
			return EXIT_FAILURE;
		}
		std::cout << "agent-invocation-" << course << ":pass\n";
		return EXIT_SUCCESS;
	}
	if (!startupSettled || !application) {
		startupTerms.note_name("failure", startupFailure ? startup_failure_name(*startupFailure) : "none");
		std::cerr << "application-startup-failed:" << startupTerms.render() << '\n';
		return EXIT_FAILURE;
	}
	application->show();
	auto&           window              = application->window();
	qt::dif_widget* workspaceFailureDif = nullptr;
	if (course == "startup-recover" || course == "startup-repair") {
		if (course == "startup-repair") {
			auto repair = wait_terms{ };
			if (!wait_until_reporting(repair, [&window](wait_terms& aTerms) {
					// [[BPR-0817]]. Also said in the window's status bar, whatever the
					// window shows. application_profile_management_public_path proves it
					// at Home, where the agent panel is not showing.
					return aTerms.require("repair-visible", window.agent_panel()->status_text().contains("Disposable workspace recovery needs repair"))
						&& aTerms.require("status-says-it", window.status_text().contains("Disposable agent workspaces are unavailable"));
				})) {
				return fail(*application, "startup-repair-visible", repair.render());
			}
		}
		const auto stopped  = shutdown(*application);
		const auto evidence = app::detail::github_application_fixture_access::agent_invocation_evidence(*application);
		const auto closed   = application->status();
		// [[BPR-0817]]. The refusal is reported as a refusal, and it is not a cleanup
		// failure of the quit: RFC-BPR-0033's owner-delegated amendment keeps an
		// otherwise orderly quit at exit 0.
		if (!stopped || evidence.AgentProcessLauncherInvocations != 0U
			|| closed.AgentWorkspaceRecoveryRefused != (course == "startup-repair")
			|| closed.AgentWorkspaceCleanupFailed || closed.ShutdownCleanupFailed) {
			report("startup-recovery-terminal",
				   std::string{ "stopped=" } + (stopped ? "1" : "0")
					   + " launches=" + std::to_string(evidence.AgentProcessLauncherInvocations)
					   + " refused=" + (closed.AgentWorkspaceRecoveryRefused ? "1" : "0")
					   + " workspace-cleanup-failed=" + (closed.AgentWorkspaceCleanupFailed ? "1" : "0")
					   + " shutdown-cleanup-failed=" + (closed.ShutdownCleanupFailed ? "1" : "0"));
			return EXIT_FAILURE;
		}
		std::cout << "agent-invocation-" << course << ":pass\n";
		return EXIT_SUCCESS;
	}

	window.github_connect_action()->trigger();
	auto connected = wait_terms{ };
	if (!wait_until_reporting(connected, [&window](wait_terms& aTerms) {
			aTerms.note_identifier("status", window.github_status_text());
			return aTerms.require(
					   "status-is-connected",
					   window.github_status_text() == "GitHub account connected.")
				&& aTerms.require(
					"open-enabled", window.github_open_action()->isEnabled());
		})) {
		return fail(*application, "connect", connected.render());
	}
	// [[BPR-0771]]. A pull request is named in the Home bar; the locator is retired.
	window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
	window.home_open_action()->trigger();

	auto* const panel = window.agent_panel();
	if (course == "construction-cut" || course == "launcher-construction-cut") {
		auto opened = wait_terms{ };
		if (!wait_until_reporting(opened, [&window](wait_terms& aTerms) {
				return aTerms.require(
					"files-listed",
					window.files_drawer()->contents_view()->model()->rowCount() != 0);
			})) {
			return fail(*application, "construction-cut-open", opened.render());
		}
		panel->offer_profiles(core::built_in_agent_profiles());
		panel->disclose_scope(QStringLiteral("construction cut test"));
		panel->profile_selector()->setCurrentIndex(3);
		panel->start_button()->click();
		QApplication::processEvents();
		const auto evidence = app::detail::github_application_fixture_access::
			agent_invocation_evidence(*application);
		const auto startGestureObserved
			= panel->statistics().StartIntents == 1U;
		const auto launcherPathExact = exact_agent_process_launcher_path(
			evidence, startGestureObserved);
		const auto sharedCutEvidence = !launcherPathExact
			&& startGestureObserved
			&& evidence.AgentProcessLauncherInvocations == 0U
			&& !evidence.HostComposed
			&& evidence.Statistics
				== application::agent_invocation_host_statistics{ };
		const auto passed = course == "launcher-construction-cut"
			? sharedCutEvidence && !evidence.AgentProcessLauncherComposed
			: sharedCutEvidence && evidence.AgentProcessLauncherComposed;
		if (!shutdown(*application) || !passed) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-invocation-" << course << ":pass\n";
		return EXIT_SUCCESS;
	}

	auto offerings = wait_terms{ };
	if (!wait_until_reporting(offerings, [&window, panel](wait_terms& aTerms) {
			aTerms.note(
				"profiles",
				static_cast<std::uint64_t>(panel->profile_selector()->count()));
			return aTerms.require(
					   "files-listed",
					   window.files_drawer()->contents_view()->model()->rowCount()
						   != 0)
				&& aTerms.require(
					"profiles-is-four", panel->profile_selector()->count() == 4)
				&& aTerms.require(
					"disclosure-present", !panel->disclosure_text().isEmpty());
		})) {
		return fail(
			*application, "agent-authority-and-offerings", offerings.render());
	}
	// [[BPR-0771]]. Opening from the Home bar lands on the reviewer's opening
	// modality, Overview by default; the retired locator this course opened
	// through left the window on Review. The course is about the review
	// surface, so it chooses Review from the strip, as a reviewer would.
	window.choose_modality(qt::review_modality::review);
	QApplication::processEvents();
	if (course == "positive-reopen") {
		const auto catalog = app::detail::github_application_fixture_access::
			agent_session_catalog(*application);
		const auto fixture = app::detail::github_application_fixture_access::evidence(
			*application);
		const auto exact = catalog.has_value()
			&& catalog->Catalog.Latest.has_value()
			&& catalog->Catalog.Latest->Id == core::agent_session_id{ 1U }
			&& catalog->Catalog.Latest->Profile
				== core::external_terminal_profile_id
			&& catalog->Catalog.Next == core::agent_session_id{ 2U }
			&& core::canonical_hex(catalog->Catalog.Latest->Binding.BaseOid).view()
				== fixture.BaseOid
			&& core::canonical_hex(catalog->Catalog.Latest->Binding.HeadOid).view()
				== fixture.HeadOneOid
			&& catalog->Catalog.Latest->Binding.LeftEpoch
				== core::epoch_ordinal{ 1U }
			&& catalog->Catalog.Latest->Binding.RightEpoch
				== core::epoch_ordinal{ 1U }
			&& catalog->Statistics.SessionAdmissions == 0U
			&& app::detail::github_application_fixture_access::
					agent_invocation_evidence(*application)
						.AgentProcessLauncherInvocations
				== 0U
			&& app::detail::github_application_fixture_access::
					review_mutation_evidence(*application)
						.Submissions
				== 0U
			&& panel->session_catalog_text().contains("Latest session #1")
			&& panel->session_catalog_text().contains("current")
			&& panel->session_catalog_text().contains("next #2");
		if (!shutdown(*application) || !exact
			|| app::detail::github_application_fixture_access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations != 0U) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-invocation-composition-reopen:pass\n";
		return EXIT_SUCCESS;
	}
	const auto externalIndex = panel->profile_selector()->findData(
		QVariant::fromValue<qulonglong>(core::external_terminal_profile_id.value()));
	if (externalIndex < 0) {
		return fail(*application, "external-profile");
	}
	panel->profile_selector()->setCurrentIndex(externalIndex);

	if (course != "gesture-cut") {
		auto* const files = window.files_drawer()->contents_view();
		const auto  first = files->model()->index(0, 0);
		files->selectionModel()->setCurrentIndex(
			first,
			QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		auto selectedDiff = wait_terms{ };
		if (!wait_until_reporting(
				selectedDiff,
				[&window, panel](wait_terms& aTerms) {
					return aTerms.require(
							   "comparison-shown",
							   window.comparison_surface()->current_result()
								   != nullptr)
						&& aTerms.require(
							"start-enabled", panel->start_action()->isEnabled());
				},
				extended_agent_budget)) {
			return fail(*application, "real-selected-diff", selectedDiff.render());
		}
		auto* const surface = window.comparison_surface();
		if (!surface->navigate_change(core::change_navigation_direction::next,
									  core::change_navigation_policy::stop)) {
			return fail(*application, "semantic-selection-action");
		}
		auto semantic = wait_terms{ };
		if (!wait_until_reporting(semantic, [surface](wait_terms& aTerms) {
				return aTerms.require(
					"selection-present", surface->selection().has_value());
			})) {
			return fail(*application, "semantic-selection", semantic.render());
		}
		if (!window.read_action()->isEnabled()) {
			return fail(*application, "receipt-action");
		}
		// Reinvocation must retain the receipt persisted by the killed run.
		// Triggering this toggle again would undo that receipt.
		if (course != "crash-reinvoke") {
			window.read_action()->trigger();
		}
		auto receipt = wait_terms{ };
		if (!wait_until_reporting(receipt, [&window](wait_terms& aTerms) {
				return aTerms.require(
					"read-checked", window.read_action()->isChecked());
			})) {
			return fail(*application, "receipt-projection", receipt.render());
		}
		if (course == "positive" || course == "positive-seed") {
			const auto  defectThread      = std::string{ "review-thread:PRRT_fixture_1" };
			auto* const conversationView  = window.conversation_view();
			auto* const conversationModel = window.conversation_model();
			auto        defectThreadTerms = wait_terms{ };
			if (
				conversationView == nullptr || conversationModel == nullptr
				|| !wait_until_reporting(
					defectThreadTerms,
					[conversationModel, &defectThread](wait_terms& aTerms) {
						return aTerms.require(
							"thread-index-valid",
							conversationModel->index_for_id(defectThread)
								.isValid());
					})) {
				return fail(
					*application, "defect-thread", defectThreadTerms.render());
			}
			conversationView->setCurrentIndex(
				conversationModel->index_for_id(defectThread));
			if (!window.classify_defect_action()->isEnabled()) {
				return fail(*application, "defect-action");
			}
			window.classify_defect_action()->trigger();
			auto defectPersisted = wait_terms{ };
			if (!wait_until_reporting(defectPersisted, [&window](wait_terms& aTerms) {
					aTerms.note(
						"defects", static_cast<std::uint64_t>(window.defect_count()));
					return aTerms.require(
						"defect-count-is-one", window.defect_count() == 1U);
				})) {
				return fail(
					*application, "defect-persistence", defectPersisted.render());
			}
			if (!app::detail::github_application_fixture_access::
					seed_agent_private_context_canary(
						*application,
						selectedNote,
						unselectedNoteCanary)) {
				return fail(*application, "private-canary-seed");
			}
			auto* const editor = window.review_editor();
			if (editor == nullptr || !editor->shown_draft().has_value()) {
				return fail(*application, "draft-canary-ready");
			}
			editor->body_editor()->setPlainText(
				QString::fromUtf8(
					selectedDraft.data(),
					static_cast<qsizetype>(selectedDraft.size())));
			// [[BPR-1211]]. Choices are offered again when the draft host
			// accepts the typed draft, which clears a selection made before it;
			// the course selects once that acceptance has landed.
			auto draftAccepted = wait_terms{ };
			if (!wait_until_reporting(draftAccepted, [&](wait_terms& aTerms) {
					const auto context = app::detail::github_application_fixture_access::agent_invocation_context(*application);
					return aTerms.require("draft-accepted", context.has_value() && context->CurrentDraft.has_value() && context->CurrentDraft->Draft.Body == selectedDraft);
				})) {
				return fail(*application, "draft-accepted", draftAccepted.render());
			}
			auto privateOfferings = wait_terms{ };
			if (!wait_until_reporting(privateOfferings, [panel](wait_terms& aTerms) {
					aTerms.note(
						"notes",
						static_cast<std::uint64_t>(panel->note_selector()->count()));
					return aTerms.require(
							   "notes-is-two", panel->note_selector()->count() == 2)
						&& aTerms.require(
							"draft-enabled", panel->draft_selector()->isEnabled());
				})) {
				return fail(
					*application,
					"private-selection-offerings",
					privateOfferings.render());
			}
			auto selectedNoteFound = false;
			for (auto row = 0; row < panel->note_selector()->count(); ++row) {
				auto* const item = panel->note_selector()->item(row);
				if (item->data(Qt::UserRole).toULongLong() == 246U) {
					item->setCheckState(Qt::Checked);
					selectedNoteFound = true;
				}
			}
			panel->draft_selector()->setChecked(true);
			expectedContext
				= app::detail::github_application_fixture_access::
					agent_invocation_context(*application);
			if (
				!expectedContext.has_value()
				|| expectedContext->Files.size() != 1U
				|| expectedContext->Private.Notes.size() != 2U
				|| !selectedNoteFound
				|| !expectedContext->CurrentDraft.has_value()
				|| expectedContext->CurrentDraft->Draft.Body != selectedDraft
				|| !expectedContext->Private.LastReviewedHead.has_value()
				|| expectedContext->Private.Defects.size() != 1U
				|| !expectedContext->Selection.has_value()
				|| expectedContext->Selection->Segments.empty()
				|| expectedContext->Selection->Groups.empty()) {
				return fail(*application, "independent-context-evidence");
			}
		}
	}

	if (workspaceDiagnostic) {
		if (!diagnosticGit.has_value()) {
			return fail(*application, "workspace-diagnostic-git");
		}
		window.adopt_dif(core::resolve_dif(
			{ .Visibility = core::dif_visibility::shown,
			  .Motion     = core::motion_mode::full },
			core::motion_observation::full));
		if (window.conversations_drawer()->state()
			!= core::workbench_drawer_state::expanded) {
			window.conversations_drawer_toggle_action()->trigger();
		}
		if (!window.ai_tools_action()->isChecked()) {
			window.ai_tools_action()->trigger();
		}
		QApplication::processEvents();
		workspaceFailureDif = window.dif();
		if (workspaceFailureDif == nullptr || !window.dif_corner_slot()->isVisible()
			|| workspaceFailureDif->parentWidget() != &window || workspaceFailureDif->size() != QSize{ 40, 40 }) {
			return fail(*application, "workspace-diagnostic-dif");
		}
		auto marker = std::ofstream{
			diagnosticGit->parent_path() / workspace_failure_marker,
			std::ios::binary | std::ios::trunc,
		};
		marker << "fail the agent workspace pack only\n";
		if (!marker) {
			return fail(*application, "workspace-diagnostic-marker");
		}
	}
	if (crashMaterializing) {
		auto marker = std::ofstream{ diagnosticGit->parent_path() / ".bpr-pause-workspace" };
		marker << (course == "crash-materializing-init" ? "init" : "checkout-index");
		marker.close();
		if (!marker) {
			return fail(*application, "materialization-marker");
		}
	}
	panel->start_button()->click();
	if (crashMaterializing) {
		auto checkpoint = wait_terms{ };
		if (!wait_until_reporting(checkpoint, [&dataRoot](wait_terms& aTerms) {
				return aTerms.require("materializing-git-paused",
									  std::filesystem::exists(dataRoot / "workspace-parent"));
			})) {
			return fail(*application, "materializing-git", checkpoint.render());
		}
		const auto fixture    = app::detail::github_application_fixture_access::evidence(*application);
		const auto invocation = app::detail::github_application_fixture_access::agent_invocation_evidence(*application);
		if (invocation.Statistics.SessionAdmissions != 1U || invocation.Statistics.ProcessStarts != 0U) {
			return fail(*application, "materializing-session");
		}
		for (const auto& [name, value] : std::array{
				 std::pair{ "base-oid", fixture.BaseOid }, std::pair{ "head-oid", fixture.HeadOneOid },
				 std::pair{ "kill-ready", std::string{ course } } }) {
			auto file = std::ofstream{ dataRoot / name };
			file << value;
			file.close();
			if (!file) {
				return fail(*application, "materializing-checkpoint");
			}
		}
		if (course == "cancel-materializing-checkout") {
			panel->cancel_action()->trigger();
			auto terminal = wait_terms{ };
			if (!wait_until_reporting(terminal, [&application, panel](wait_terms& aTerms) {
					const auto current = app::detail::github_application_fixture_access::agent_invocation_evidence(*application);
					return aTerms.require("cancelled", panel->status_text() == "Finished: cancelled")
						&& aTerms.require("quiescent", current.Quiescent)
						&& aTerms.require("no-agent-start", current.Statistics.ProcessStarts == 0U);
				})) {
				return fail(*application, "materialization-cancellation", terminal.render());
			}
			if (!shutdown(*application) || application->status().AgentWorkspaceCleanupFailed) {
				return EXIT_FAILURE;
			}
			std::cout << "agent-materialization-cancel:pass\n";
			return EXIT_SUCCESS;
		}
		for (;;) {
			QThread::msleep(100);
		}
	}
	if (course == "crash-running") {
		auto running = wait_terms{ };
		if (!wait_until_reporting(running, [panel](wait_terms& aTerms) {
				const auto output = panel->output_text().toStdString();
				return aTerms.require("agent-and-descendant-running",
									  output.find("process-id\t") != std::string::npos
										  && output.find("descendant-id\t") != std::string::npos);
			})) {
			return fail(*application, "crash-running", running.render());
		}
		const auto output   = panel->output_text().toStdString();
		const auto evidence = app::detail::github_application_fixture_access::
			agent_invocation_evidence(*application);
		const auto fixture    = app::detail::github_application_fixture_access::evidence(*application);
		const auto cwd        = line_value(output, "cwd\t");
		const auto agent      = line_value(output, "process-id\t");
		const auto descendant = line_value(output, "descendant-id\t");
		const auto supervisor = line_value(output, "supervisor-id\t");
		const auto marker     = line_value(output, "descendant-marker\t");
		if (!cwd || !agent || !descendant || !supervisor || !marker
			|| !exact_agent_process_launcher_path(evidence, panel->statistics().StartIntents == 1U)
			|| evidence.Statistics.SessionAdmissions != 1U
			|| evidence.Statistics.ProcessStarts != 1U
			|| output.find("head\t" + fixture.HeadOneOid) == std::string::npos
			|| !std::filesystem::exists(*cwd)) {
			return fail(*application, "crash-identity");
		}
		for (const auto& [name, value] : std::array{
				 std::pair{ "agent.pid", *agent }, std::pair{ "descendant.pid", *descendant },
				 std::pair{ "supervisor.pid", *supervisor },
				 std::pair{ "heartbeat-path", *marker }, std::pair{ "workspace-path", *cwd },
				 std::pair{ "workspace-parent", evidence.WorkspaceRoot.string() },
				 std::pair{ "base-oid", fixture.BaseOid }, std::pair{ "head-oid", fixture.HeadOneOid } }) {
			auto file = std::ofstream{ dataRoot / name };
			file << value;
			file.close();
			if (!file) {
				return fail(*application, "crash-checkpoint-write");
			}
		}
		{
			auto file = std::ofstream{ dataRoot / "kill-ready" };
			file << "crash-running";
			file.close();
			if (!file) {
				return fail(*application, "crash-ready-write");
			}
		}
		// The runner force-kills this process; no application destructor or
		// shutdown timer can contribute recovery evidence.
		for (;;) {
			QThread::msleep(100);
		}
	}
	if (course == "gesture-cut") {
		QThread::msleep(100);
		QApplication::processEvents();
		const auto evidence = app::detail::github_application_fixture_access::
			agent_invocation_evidence(*application);
		const auto passed = evidence.HostComposed
			&& panel->statistics().StartIntents == 1U
			&& evidence.Statistics.CoreDecisions == 0U
			&& evidence.Statistics.WorkspaceStarts == 0U
			&& evidence.Statistics.ProcessStarts == 0U;
		if (!shutdown(*application) || !passed) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-invocation-gesture-cut:pass\n";
		return EXIT_SUCCESS;
	}
	if (workspaceDiagnostic) {
		auto terminal = wait_terms{ };
		if (!wait_until_reporting(
				terminal,
				[&application, panel](wait_terms& aTerms) {
					const auto evidence = app::detail::github_application_fixture_access::
						agent_invocation_evidence(*application);
					aTerms.note_identifier(
						"status", panel->status_text().toStdString());
					aTerms.note(
						"output-bytes",
						static_cast<std::uint64_t>(panel->output_text().size()));
					aTerms.note(
						"process-starts",
						static_cast<std::uint64_t>(
							evidence.Statistics.ProcessStarts));
					aTerms.note("quiescent", evidence.Quiescent);
					return aTerms.require(
							   "status-is-workspace-unavailable",
							   panel->status_text()
								   == QStringLiteral(
									   "Finished: workspace_unavailable"))
						&& aTerms.require(
							"diagnostic-is-exact",
							panel->output_text().toStdString()
								== redacted_git_diagnostic)
						&& aTerms.require(
							"workspace-starts-is-one",
							evidence.Statistics.WorkspaceStarts == 1U)
						&& aTerms.require(
							"process-starts-is-zero",
							evidence.Statistics.ProcessStarts == 0U)
						&& aTerms.require("host-is-quiescent", evidence.Quiescent);
				},
				extended_agent_budget)) {
			return fail(
				*application, "workspace-diagnostic-terminal", terminal.render());
		}
		const auto evidence = app::detail::github_application_fixture_access::
			agent_invocation_evidence(*application);
		const auto exact = evidence.Statistics.CoreDecisions == 1U
			&& evidence.Statistics.WorkspaceStarts == 1U
			&& evidence.Statistics.ProcessStarts == 0U
			&& evidence.Statistics.WorkspaceCleanups == 0U
			&& evidence.Quiescent && workspaceFailureDif != nullptr
			&& workspaceFailureDif->current_frame().Clip
				== qt::dif_index(qt::dif_clip_id::blocked);
		if (!shutdown(*application) || !exact) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-invocation-workspace-diagnostic:pass\n";
		return EXIT_SUCCESS;
	}
	if (course == "cleanup-retry") {
		auto retryRunning = wait_terms{ };
		if (!wait_until_reporting(
				retryRunning,
				[panel](wait_terms& aTerms) {
					aTerms.note_identifier("status", panel->status_text().toStdString());
					aTerms.note(
						"output-bytes",
						static_cast<std::uint64_t>(panel->output_text().size()));
					return aTerms.require(
							   "status-is-running",
							   panel->status_text().startsWith(
								   QStringLiteral("Running in")))
						&& aTerms.require(
							"canary-complete-seen",
							panel->output_text().contains(
								QStringLiteral("canary-complete")));
				},
				extended_agent_budget)) {
			return fail(
				*application, "cleanup-retry-running", retryRunning.render());
		}
		const auto output    = panel->output_text().toStdString();
		const auto workspace = line_value(output, "cwd\t");
		const auto evidence  = app::detail::github_application_fixture_access::
			agent_invocation_evidence(*application);
		if (!workspace.has_value() || evidence.WorkspaceRoot.empty()) {
			return fail(*application, "cleanup-retry-workspace");
		}
		const auto workspacePath = std::filesystem::path{ *workspace };
		const auto originalPath  = evidence.WorkspaceRoot.parent_path()
			/ (evidence.WorkspaceRoot.filename().string() + "-retry-original");
		const auto substitutePath = evidence.WorkspaceRoot.parent_path()
			/ (evidence.WorkspaceRoot.filename().string() + "-retry-substitute");
		const auto releasePath = workspacePath / ".bpr-cleanup-retry-release";
		auto       restore     = [&] {
			auto restoreError = std::error_code{ };
			if (std::filesystem::exists(originalPath)) {
				if (std::filesystem::exists(workspacePath)
					&& !std::filesystem::exists(substitutePath)) {
					std::filesystem::rename(
						workspacePath, substitutePath, restoreError);
				}
				if (!restoreError && !std::filesystem::exists(workspacePath)) {
					std::filesystem::rename(
						originalPath, workspacePath, restoreError);
				}
			}
			if (!restoreError && std::filesystem::exists(workspacePath)) {
				auto release = std::ofstream{ releasePath, std::ios::binary };
				release << "release\n";
			}
			return !restoreError;
		};
		const auto restoreThenFail = [&](const std::string_view aReason) {
			static_cast<void>(restore());
			return fail(*application, aReason);
		};

		auto filesystemError = std::error_code{ };
		// On Windows the live canary holds its current directory's handle
		// without delete sharing, so this swap-aside rename is refused until
		// the canary vacates its cwd on entering the release poll; the retry
		// waits for exactly that release while the canary stays alive and
		// paused. POSIX permits the first attempt (BPR-0465).
		for (auto attempt = 0;; ++attempt) {
			std::filesystem::rename(workspacePath, originalPath, filesystemError);
			if (!filesystemError || attempt >= 3000) {
				break;
			}
			QThread::msleep(10);
		}
		if (filesystemError
			|| !std::filesystem::create_directory(workspacePath, filesystemError)) {
			return restoreThenFail("cleanup-retry-substitute");
		}
		std::filesystem::permissions(
			workspacePath,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace,
			filesystemError);
		if (filesystemError) {
			return restoreThenFail("cleanup-retry-substitute-permissions");
		}
		{
			auto release = std::ofstream{ releasePath, std::ios::binary };
			release << "release\n";
			if (!release) {
				return restoreThenFail("cleanup-retry-release");
			}
		}
		auto firstRefusal = wait_terms{ };
		if (!wait_until_reporting(
				firstRefusal,
				[&application, panel](wait_terms& aTerms) {
					const auto current = app::detail::github_application_fixture_access::
						agent_invocation_evidence(*application);
					aTerms.note_identifier("status", panel->status_text().toStdString());
					aTerms.note(
						"cleanup-failures",
						static_cast<std::uint64_t>(
							current.Statistics.WorkspaceCleanupFailures));
					aTerms.note(
						"cleanups",
						static_cast<std::uint64_t>(
							current.Statistics.WorkspaceCleanups));
					return aTerms.require(
							   "status-is-finished-success",
							   panel->status_text()
								   == QStringLiteral("Finished: success"))
						&& aTerms.require(
							"cleanup-failures-is-one",
							current.Statistics.WorkspaceCleanupFailures == 1U)
						&& aTerms.require(
							"cleanups-is-zero",
							current.Statistics.WorkspaceCleanups == 0U)
						&& aTerms.require("not-quiescent", !current.Quiescent);
				},
				extended_agent_budget)) {
			report("cleanup-retry-first-refusal", firstRefusal.render());
			return restoreThenFail("cleanup-retry-first-refusal");
		}
		if (!restore()) {
			return fail(*application, "cleanup-retry-restore");
		}
		application->begin_shutdown();
		// `report` rather than `fail`: shutdown has already begun, so `fail`'s own
		// shutdown wait would be a second one. This used to return EXIT_FAILURE
		// having printed nothing whatsoever (BPR-0381).
		auto retryShutdown = wait_terms{ };
		if (!wait_until_reporting(
				retryShutdown,
				[&application](wait_terms& aTerms) {
					const auto status = application->status();
					aTerms.note("cleanup-failed", status.ShutdownCleanupFailed);
					return aTerms.require(
						"shutdown-complete", status.ShutdownComplete);
				},
				extended_agent_budget)) {
			report("cleanup-retry-shutdown", retryShutdown.render());
			return EXIT_FAILURE;
		}
		const auto finalEvidence = app::detail::github_application_fixture_access::
			agent_invocation_evidence(*application);
		const auto substitutePreserved = std::filesystem::exists(substitutePath)
			&& std::filesystem::exists(
											 substitutePath / ".bpr-cleanup-retry-release");
		const auto passed = finalEvidence.Quiescent
			&& finalEvidence.Statistics.WorkspaceCleanupFailures == 1U
			&& finalEvidence.Statistics.WorkspaceCleanups == 1U
			&& !std::filesystem::exists(workspacePath)
			&& !std::filesystem::exists(evidence.WorkspaceRoot)
			&& substitutePreserved
			&& !application->status().AgentWorkspaceCleanupFailed;
		std::filesystem::remove_all(substitutePath, filesystemError);
		if (!passed) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-invocation-cleanup-retry:pass\n";
		return EXIT_SUCCESS;
	}
	auto descendantMarker = std::optional<std::string>{ };
	if (course == "cancellation" || course == "staleness") {
		auto running = wait_terms{ };
		if (!wait_until_reporting(
				running,
				[panel](wait_terms& aTerms) {
					aTerms.note_identifier("status", panel->status_text().toStdString());
					aTerms.note(
						"output-bytes",
						static_cast<std::uint64_t>(panel->output_text().size()));
					return aTerms.require(
							   "status-is-running",
							   panel->status_text().startsWith(
								   QStringLiteral("Running in")))
						&& aTerms.require(
							"canary-complete-seen",
							panel->output_text().contains(
								QStringLiteral("canary-complete")));
				},
				extended_agent_budget)) {
			return fail(*application, "running-course", running.render());
		}
		if (course == "cancellation") {
			descendantMarker = line_value(
				panel->output_text().toStdString(), "descendant-marker\t");
			if (!descendantMarker.has_value()
				|| !std::filesystem::exists(*descendantMarker)) {
				return fail(*application, "descendant-start");
			}
			panel->cancel_action()->trigger();
		} else if (!app::detail::github_application_fixture_access::
					   reaccept_fixture_revision(*application, true)) {
			return fail(*application, "stale-revision");
		}
		const auto expectedTerminal = course == "cancellation"
			? QStringLiteral("Finished: cancelled")
			: QStringLiteral("Finished: stale_result");
		auto       terminalReached  = wait_terms{ };
		auto       terminalCleaned  = wait_terms{ };
		if (!wait_until_reporting(
				terminalReached,
				[panel, &expectedTerminal](wait_terms& aTerms) {
					aTerms.note_identifier("status", panel->status_text().toStdString());
					return aTerms.require(
						"status-is-expected-terminal",
						panel->status_text() == expectedTerminal);
				},
				extended_agent_budget)
			|| !panel->output_text().isEmpty()
			|| !wait_until_reporting(
				terminalCleaned,
				[&application](wait_terms& aTerms) {
					const auto evidence = app::detail::github_application_fixture_access::
						agent_invocation_evidence(*application);
					aTerms.note(
						"cleanups",
						static_cast<std::uint64_t>(
							evidence.Statistics.WorkspaceCleanups));
					return aTerms.require(
							   "cleanups-is-one",
							   evidence.Statistics.WorkspaceCleanups == 1U)
						&& aTerms.require("quiescent", evidence.Quiescent);
				})) {
			const auto failureEvidence = app::detail::github_application_fixture_access::
				agent_invocation_evidence(*application);
			// The third independently shaped dump this file carried, now the same
			// recorder as the others (BPR-0381).
			auto terminal = wait_terms{ };
			terminal.note_identifier(
				"agent-terminal-status", panel->status_text().toStdString());
			terminal.note_identifier(
				"agent-terminal-expected", expectedTerminal.toStdString());
			terminal.note(
				"agent-terminal-output-bytes",
				static_cast<std::uint64_t>(panel->output_text().toUtf8().size()));
			terminal.note(
				"agent-terminal-cleanups",
				static_cast<std::uint64_t>(
					failureEvidence.Statistics.WorkspaceCleanups));
			terminal.note("agent-terminal-quiescent", failureEvidence.Quiescent);
			terminal.note_identifier("reached", terminalReached.failing_term());
			terminal.note_identifier("cleaned", terminalCleaned.failing_term());
			return fail(
				*application, "typed-terminal-cleanup", terminal.render());
		}
		if (course == "cancellation") {
			if (!descendantMarker.has_value()) {
				return fail(*application, "descendant-marker");
			}
			const auto stoppedSize = std::filesystem::file_size(*descendantMarker);
			QThread::msleep(250);
			if (std::filesystem::file_size(*descendantMarker) != stoppedSize) {
				return fail(*application, "descendant-stop");
			}
			std::error_code removeError;
			static_cast<void>(
				std::filesystem::remove(*descendantMarker, removeError));
		}
		if (!shutdown(*application)) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-invocation-" << course << ":pass\n";
		return EXIT_SUCCESS;
	}

	auto finished = wait_terms{ };
	if (!wait_until_reporting(
			finished,
			[panel](wait_terms& aTerms) {
				aTerms.note_identifier("status", panel->status_text().toStdString());
				return aTerms.require(
					"status-is-finished",
					panel->status_text().startsWith(QStringLiteral("Finished:")));
			},
			extended_agent_budget)
		|| panel->status_text() != "Finished: success") {
		const auto failureEvidence = app::detail::github_application_fixture_access::
			agent_invocation_evidence(*application);
		// Was a hand-rolled dump that ended with the panel's whole output text.
		// That text is agent output and `evidence.WorkspaceRoot` is a filesystem
		// path carrying a local username -- BPR-0344's own evidence contains one --
		// so both are replaced by bounded facts. The status text is an identifier,
		// so it goes through the channel that bounds and sanitizes it (BPR-0381).
		auto success = wait_terms{ };
		success.note_identifier("agent-status", panel->status_text().toStdString());
		success.note(
			"core",
			static_cast<std::uint64_t>(failureEvidence.Statistics.CoreDecisions));
		success.note(
			"workspace",
			static_cast<std::uint64_t>(failureEvidence.Statistics.WorkspaceStarts));
		success.note(
			"process",
			static_cast<std::uint64_t>(failureEvidence.Statistics.ProcessStarts));
		success.note(
			"cleanup",
			static_cast<std::uint64_t>(
				failureEvidence.Statistics.WorkspaceCleanups));
		success.note(
			"output-bytes",
			static_cast<std::uint64_t>(panel->output_text().size()));
		success.note_identifier("reached", finished.failing_term());
		return fail(*application, "typed-success", success.render());
	}
	auto cleaned = wait_terms{ };
	if (!wait_until_reporting(cleaned, [&application](wait_terms& aTerms) {
			const auto current = app::detail::github_application_fixture_access::
				agent_invocation_evidence(*application);
			aTerms.note(
				"cleanups",
				static_cast<std::uint64_t>(current.Statistics.WorkspaceCleanups));
			return aTerms.require(
				"cleanups-is-one", current.Statistics.WorkspaceCleanups == 1U);
		})) {
		return fail(*application, "workspace-cleanup", cleaned.render());
	}
	const auto evidence = app::detail::github_application_fixture_access::
		agent_invocation_evidence(*application);
	const auto output  = panel->output_text().toStdString();
	const auto cwd     = line_value(output, "cwd\t");
	const auto fixture = app::detail::github_application_fixture_access::evidence(
		*application);
	if (course == "crash-reinvoke") {
		const auto catalog = app::detail::github_application_fixture_access::agent_session_catalog(*application);
		const auto passed  = exact_agent_process_launcher_path(evidence, panel->statistics().StartIntents == 1U)
			&& evidence.Quiescent && evidence.Statistics.SessionAdmissions == 1U
			&& evidence.Statistics.ProcessStarts == 1U && evidence.Statistics.WorkspaceCleanups == 1U
			&& cwd.has_value() && !std::filesystem::exists(*cwd)
			&& catalog.has_value() && catalog->Catalog.Latest.has_value()
			&& catalog->Catalog.Latest->Id == core::agent_session_id{ 2U }
			&& catalog->Catalog.Next == core::agent_session_id{ 3U }
			&& core::canonical_hex(catalog->Catalog.Latest->Binding.HeadOid).view() == fixture.HeadOneOid
			&& output.find("head\t" + fixture.HeadOneOid) != std::string::npos
			&& app::detail::github_application_fixture_access::review_mutation_evidence(*application).Submissions == 0U;
		const auto stopped = shutdown(*application);
		if (!passed || !stopped || std::filesystem::exists(evidence.WorkspaceRoot)
			|| application->status().AgentWorkspaceCleanupFailed) {
			auto terms = wait_terms{ };
			terms.note("path-exact", exact_agent_process_launcher_path(evidence, panel->statistics().StartIntents == 1U));
			terms.note("quiescent", evidence.Quiescent);
			terms.note("admissions", evidence.Statistics.SessionAdmissions);
			terms.note("processes", evidence.Statistics.ProcessStarts);
			terms.note("cleanups", evidence.Statistics.WorkspaceCleanups);
			terms.note("cwd-present", cwd.has_value());
			terms.note("cwd-removed", cwd.has_value() && !std::filesystem::exists(*cwd));
			terms.note("latest", catalog && catalog->Catalog.Latest ? catalog->Catalog.Latest->Id.value() : 0U);
			terms.note("next", catalog && catalog->Catalog.Next ? catalog->Catalog.Next->value() : 0U);
			terms.note("head-output", output.find("head\t" + fixture.HeadOneOid) != std::string::npos);
			terms.note("shutdown", stopped);
			terms.note("root-removed", !std::filesystem::exists(evidence.WorkspaceRoot));
			report("crash-reinvoke", terms.render());
			return EXIT_FAILURE;
		}
		std::cout << "agent-invocation-crash-reinvoke:pass\n";
		return EXIT_SUCCESS;
	}
	const auto                 stdinHex     = line_value(output, "stdin-hex\t");
	const auto                 decodedInput = stdinHex.has_value()
		? decode_hex(*stdinHex)
		: std::optional<std::string>{ };
	constexpr std::string_view contextHeader
		= "BPR-CONTEXT/2 EXTERNAL-TERMINAL\n";
	auto actualFields = std::optional<context_fields>{ };
	if (decodedInput.has_value() && decodedInput->starts_with(contextHeader)) {
		actualFields = parse_context_fields(
			std::string_view{ *decodedInput }.substr(contextHeader.size()));
	}
	const auto expectedFields = expectedContext.has_value()
		? std::optional{ expected_context_fields(*expectedContext) }
		: std::optional<context_fields>{ };
	// [[BPR-0826]]. Since BPR-0705 each window session owns an application
	// session distinct from the profile runtime's ("Profile and window
	// ownership", docs/architecture/system-architecture.md). The expected session
	// is the one the composition gave the window this course invoked from, which
	// has to be the only live window, not the one-window constant assumed here
	// before.
	const auto liveWindows    = application->windows();
	const auto invokingWindow = liveWindows.size() == 1U
		&& application->window(liveWindows.front()) == &window;
	const auto composedSession     = invokingWindow
		? app::detail::github_application_fixture_access::window_application_session(
			  *application, liveWindows.front())
		: std::optional<core::application_session_id>{ };
	const auto acceptedAuthorities = expectedContext.has_value()
		&& composedSession.has_value()
		&& expectedContext->ApplicationSession == *composedSession
		&& expectedContext->NextSession == core::agent_session_id{ 1U }
		&& expectedContext->Authority.Host == core::git_host_id{ 1U }
		&& expectedContext->Authority.Repository == core::repository_id{ 2001U }
		&& expectedContext->Authority.PullRequest == core::pull_request_id{ 3001U }
		&& expectedContext->Binding.Host == expectedContext->Authority.Host
		&& expectedContext->Binding.Repository
			== expectedContext->Authority.Repository
		&& expectedContext->Binding.PullRequest
			== expectedContext->Authority.PullRequest
		&& core::canonical_hex(expectedContext->Binding.BaseOid).view()
			== fixture.BaseOid
		&& core::canonical_hex(expectedContext->Binding.HeadOid).view()
			== fixture.HeadOneOid
		&& expectedContext->Binding.LeftEpoch == core::epoch_ordinal{ 1U }
		&& expectedContext->Binding.RightEpoch == core::epoch_ordinal{ 1U }
		&& expectedContext->PullRequestUrl
			== "https://github.com/octo-org/review-repository/pull/17";
	const auto containsForbiddenContext = actualFields.has_value()
		&& contains_forbidden_context(*actualFields);
	const auto relativeWorkspace = cwd ? std::filesystem::path{ *cwd }.lexically_relative(dataRoot) : std::filesystem::path{ };
	const auto outsideCache      = !relativeWorkspace.empty() && *relativeWorkspace.begin() == "..";
	const auto exact             = exact_agent_process_launcher_path(
									   evidence, panel->statistics().StartIntents == 1U)
		&& evidence.Quiescent
		&& evidence.Statistics.CoreDecisions == 1U
		&& evidence.Statistics.WorkspaceStarts == 1U
		&& evidence.Statistics.ProcessStarts == 1U
		&& evidence.Statistics.WorkspaceCleanups == 1U
		&& cwd.has_value() && !std::filesystem::exists(*cwd)
		&& !evidence.WorkspaceRoot.empty()
#if defined(Q_OS_UNIX) || defined(Q_OS_WIN)
		&& !std::filesystem::exists(evidence.WorkspaceRoot)
#else
		&& std::filesystem::exists(evidence.WorkspaceRoot)
#endif
		&& outsideCache
		&& output.find("head\t" + fixture.HeadOneOid) != std::string::npos
		&& output.find("origin\tabsent") != std::string::npos
		&& output.find("alternates\tabsent") != std::string::npos
		&& output.find("http-alternates\tabsent") != std::string::npos
		&& output.find("hooks\tabsent") != std::string::npos
		&& output.find("worktrees\tabsent") != std::string::npos
		&& acceptedAuthorities && actualFields.has_value()
		&& expectedFields.has_value() && *actualFields == *expectedFields
		&& actualFields->at("schema") == "2"
		&& actualFields->at("file.count") == "1"
		&& actualFields->at("file.0.read") == "2"
		&& actualFields->at("file.0.changed_since_review") == "0"
		&& actualFields->at("last_reviewed_head.present") == "1"
		&& actualFields->at("defect.count") == "1"
		&& actualFields->at("selection.present") == "1"
		&& actualFields->at("selection.mapping_confidence") == "0"
		&& actualFields->at("selection.source_text.present") == "0"
		&& actualFields->at("note.count") == "1"
		&& actualFields->at("note.0.text") == selectedNote
		&& actualFields->at("draft.count") == "1"
		&& actualFields->at("draft.0.text") == selectedDraft
		&& !containsForbiddenContext
		&& decodedInput->find(unselectedNoteCanary) == std::string::npos
		&& output.find("GITHUB_TOKEN") == std::string::npos
		&& output.find("BPR_CREDENTIAL_CANARY_0250") == std::string::npos;
	if (!exact) {
		std::cerr << "agent-exact-context-output:\n"
				  << output << "\nagent-exact-context-evidence:"
				  << evidence.AgentProcessLauncherComposed << ':'
				  << evidence.AgentProcessLauncherInvocations << ':'
				  << panel->statistics().StartIntents << ':'
				  << evidence.Quiescent << ':'
				  << evidence.Statistics.CoreDecisions << ':'
				  << evidence.Statistics.WorkspaceStarts << ':'
				  << evidence.Statistics.ProcessStarts << ':'
				  << evidence.Statistics.WorkspaceCleanups << ':'
				  << (actualFields.has_value() ? actualFields->size() : 0U) << ':'
				  << (expectedFields.has_value() ? expectedFields->size() : 0U)
				  << "\nagent-exact-context-session:"
				  << (expectedContext.has_value()
						  ? expectedContext->ApplicationSession.value()
						  : 0U)
				  << ':'
				  << (composedSession.has_value() ? composedSession->value() : 0U)
				  << ':' << liveWindows.size() << ':' << invokingWindow << '\n';
	}
	const auto shutdownComplete = shutdown(*application);
	const auto rootRemoved      = shutdownComplete
		&& !std::filesystem::exists(evidence.WorkspaceRoot)
		&& !application->status().AgentWorkspaceCleanupFailed;
	if (!shutdownComplete || !exact || !rootRemoved) {
		return EXIT_FAILURE;
	}
	std::cout
		<< (course == "positive-seed"
				? "agent-invocation-composition-seed:pass\n"
				: "agent-invocation-composition:pass\n");
	return EXIT_SUCCESS;
}
