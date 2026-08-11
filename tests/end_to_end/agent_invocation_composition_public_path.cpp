#include "../../src/app/src/github_application_fixture_access.hpp"

#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
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
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	template <typename Predicate>
	[[nodiscard]] auto wait_until(Predicate aPredicate, const int aMilliseconds = 30'000)
		-> bool
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (timer.elapsed() < aMilliseconds) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
			if (aPredicate()) {
				return true;
			}
			QThread::msleep(5);
		}
		return false;
	}

	[[nodiscard]] auto shutdown(app::github_application& aApplication) -> bool
	{
		aApplication.begin_shutdown();
		return wait_until([&aApplication] {
			return aApplication.status().ShutdownComplete;
		});
	}

	auto fail(app::github_application& aApplication, const std::string_view aReason)
		-> int
	{
		std::cerr << "agent-invocation-composition-failed:" << aReason << '\n';
		static_cast<void>(shutdown(aApplication));
		return EXIT_FAILURE;
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
			decoded.push_back(static_cast<char>((*high << 4U) | *low));
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
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	if (argc != 3 && argc != 4) {
		return EXIT_FAILURE;
	}
	const auto course = std::string_view{ argv[1] };
	if (course != "positive" && course != "positive-seed"
		&& course != "positive-reopen" && course != "construction-cut"
		&& course != "gesture-cut" && course != "cancellation"
		&& course != "staleness" && course != "cleanup-retry") {
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
	auto data = QTemporaryDir{ };
	if (!data.isValid()) {
		return EXIT_FAILURE;
	}
	const auto      dataRoot = argc == 4
		? std::filesystem::path{ argv[3] }
		: std::filesystem::path{ data.path().toStdString() };
	std::error_code dataError;
	std::filesystem::create_directories(dataRoot, dataError);
	if (dataError) {
		return EXIT_FAILURE;
	}
	auto externalExecutable = std::filesystem::path{ argv[2] };
	if (
		course == "cancellation" || course == "staleness"
		|| course == "cleanup-retry") {
		const auto mode    = course == "cancellation"
			? "descendant-wait"
			: course == "staleness" ? "stale-wait"
									: "cleanup-retry-wait";
		externalExecutable = dataRoot
			/ ("better_pull_review_agent_canary-" + std::string{ mode });
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
	auto application = app::detail::github_application_fixture_access::make(
		app::detail::github_public_path_fixture::github_com,
		{
			.ApplicationDataRoot         = dataRoot,
			.EpochPhase                  = app::detail::github_epoch_fixture_phase::single,
			.DisableAgentHostComposition = course == "construction-cut",
			.DisableAgentGestureWire     = course == "gesture-cut",
			.ExternalAgentExecutable     = externalExecutable,
		});
	application->show();
	auto& window = application->window();
	window.github_connect_action()->trigger();
	if (!wait_until([&window] {
			return window.github_status_text() == "GitHub account connected."
				&& window.github_open_action()->isEnabled();
		})) {
		return fail(*application, "connect");
	}
	window.session_panel()->owner_editor()->setText(QStringLiteral("octo-org"));
	window.session_panel()->repository_editor()->setText(
		QStringLiteral("review-repository"));
	window.session_panel()->pull_request_editor()->setValue(17);
	window.github_open_action()->trigger();

	auto* const panel = window.agent_panel();
	if (course == "construction-cut") {
		if (!wait_until([&window] {
				return window.files_drawer()->contents_view()->model()->rowCount() != 0;
			})) {
			return fail(*application, "construction-cut-open");
		}
		panel->offer_profiles(core::built_in_agent_profiles());
		panel->disclose_scope(QStringLiteral("construction cut test"));
		panel->profile_selector()->setCurrentIndex(3);
		panel->start_button()->click();
		QApplication::processEvents();
		const auto evidence = app::detail::github_application_fixture_access::
			agent_invocation_evidence(*application);
		const auto passed = !evidence.HostComposed
			&& panel->statistics().StartIntents == 1U
			&& evidence.Statistics.CoreDecisions == 0U
			&& evidence.Statistics.WorkspaceStarts == 0U
			&& evidence.Statistics.ProcessStarts == 0U;
		if (!shutdown(*application) || !passed) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-invocation-construction-cut:pass\n";
		return EXIT_SUCCESS;
	}

	if (!wait_until([&window, panel] {
			return window.files_drawer()->contents_view()->model()->rowCount() != 0
				&& panel->profile_selector()->count() == 4
				&& !panel->disclosure_text().isEmpty();
		})) {
		return fail(*application, "agent-authority-and-offerings");
	}
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
			&& panel->session_catalog_text().contains("Latest session #1")
			&& panel->session_catalog_text().contains("current")
			&& panel->session_catalog_text().contains("next #2");
		if (!shutdown(*application) || !exact) {
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
		if (!wait_until([&window, panel] {
				return window.comparison_surface()->current_result() != nullptr
					&& panel->start_action()->isEnabled();
			},
						45'000)) {
			return fail(*application, "real-selected-diff");
		}
		auto* const surface = window.comparison_surface();
		if (!surface->next_change_button()->isEnabled()) {
			return fail(*application, "semantic-selection-action");
		}
		surface->next_change_button()->click();
		if (!wait_until([surface] {
				return surface->selection().has_value();
			})) {
			return fail(*application, "semantic-selection");
		}
		if (!window.read_action()->isEnabled()) {
			return fail(*application, "receipt-action");
		}
		window.read_action()->trigger();
		if (!wait_until([&window] {
				return window.read_action()->isChecked();
			})) {
			return fail(*application, "receipt-projection");
		}
		if (course == "positive" || course == "positive-seed") {
			const auto  defectThread      = std::string{ "review-thread:PRRT_fixture_1" };
			auto* const conversationView  = window.conversation_view();
			auto* const conversationModel = window.conversation_model();
			if (
				conversationView == nullptr || conversationModel == nullptr
				|| !wait_until([conversationModel, &defectThread] {
					   return conversationModel->index_for_id(defectThread).isValid();
				   })) {
				return fail(*application, "defect-thread");
			}
			conversationView->setCurrentIndex(
				conversationModel->index_for_id(defectThread));
			if (!window.classify_defect_action()->isEnabled()) {
				return fail(*application, "defect-action");
			}
			window.classify_defect_action()->trigger();
			if (!wait_until([&window] {
					return window.defect_count() == 1U;
				})) {
				return fail(*application, "defect-persistence");
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
			if (!wait_until([panel] {
					return panel->note_selector()->count() == 2
						&& panel->draft_selector()->isEnabled();
				})) {
				return fail(*application, "private-selection-offerings");
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

	panel->start_button()->click();
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
	if (course == "cleanup-retry") {
		if (!wait_until([panel] {
				return panel->status_text().startsWith(QStringLiteral("Running in"))
					&& panel->output_text().contains(QStringLiteral("canary-complete"));
			},
						45'000)) {
			return fail(*application, "cleanup-retry-running");
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
		std::filesystem::rename(workspacePath, originalPath, filesystemError);
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
		if (!wait_until([&application, panel] {
				const auto current = app::detail::github_application_fixture_access::
					agent_invocation_evidence(*application);
				return panel->status_text() == QStringLiteral("Finished: success")
					&& current.Statistics.WorkspaceCleanupFailures == 1U
					&& current.Statistics.WorkspaceCleanups == 0U
					&& !current.Quiescent;
			},
						45'000)) {
			return restoreThenFail("cleanup-retry-first-refusal");
		}
		if (!restore()) {
			return fail(*application, "cleanup-retry-restore");
		}
		application->begin_shutdown();
		if (!wait_until([&application] {
				return application->status().ShutdownComplete;
			},
						45'000)) {
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
		if (!wait_until([panel] {
				return panel->status_text().startsWith(QStringLiteral("Running in"))
					&& panel->output_text().contains(QStringLiteral("canary-complete"));
			},
						45'000)) {
			return fail(*application, "running-course");
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
		if (!wait_until([panel, &expectedTerminal] {
				return panel->status_text() == expectedTerminal;
			},
						45'000)
			|| !panel->output_text().isEmpty() || !wait_until([&application] {
				   const auto evidence = app::detail::github_application_fixture_access::
					   agent_invocation_evidence(*application);
				   return evidence.Statistics.WorkspaceCleanups == 1U
					   && evidence.Quiescent;
			   })) {
			const auto failureEvidence = app::detail::github_application_fixture_access::
				agent_invocation_evidence(*application);
			std::cerr << "agent-terminal-status:"
					  << panel->status_text().toStdString() << '\n'
					  << "agent-terminal-output-bytes:"
					  << panel->output_text().toUtf8().size() << '\n'
					  << "agent-terminal-cleanups:"
					  << failureEvidence.Statistics.WorkspaceCleanups << '\n'
					  << "agent-terminal-quiescent:"
					  << failureEvidence.Quiescent << '\n';
			return fail(*application, "typed-terminal-cleanup");
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

	if (!wait_until([panel] {
			return panel->status_text().startsWith(QStringLiteral("Finished:"));
		},
					45'000)
		|| panel->status_text() != "Finished: success") {
		const auto failureEvidence = app::detail::github_application_fixture_access::
			agent_invocation_evidence(*application);
		std::cerr << "agent-status:" << panel->status_text().toStdString() << '\n'
				  << "core:" << failureEvidence.Statistics.CoreDecisions << '\n'
				  << "workspace:" << failureEvidence.Statistics.WorkspaceStarts << '\n'
				  << "process:" << failureEvidence.Statistics.ProcessStarts << '\n'
				  << "cleanup:" << failureEvidence.Statistics.WorkspaceCleanups << '\n'
				  << panel->output_text().toStdString();
		return fail(*application, "typed-success");
	}
	if (!wait_until([&application] {
			return app::detail::github_application_fixture_access::
					   agent_invocation_evidence(*application)
						   .Statistics.WorkspaceCleanups
				== 1U;
		})) {
		return fail(*application, "workspace-cleanup");
	}
	const auto evidence = app::detail::github_application_fixture_access::
		agent_invocation_evidence(*application);
	const auto output  = panel->output_text().toStdString();
	const auto cwd     = line_value(output, "cwd\t");
	const auto fixture = app::detail::github_application_fixture_access::evidence(
		*application);
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
	const auto expectedFields      = expectedContext.has_value()
		? std::optional{ expected_context_fields(*expectedContext) }
		: std::optional<context_fields>{ };
	const auto acceptedAuthorities = expectedContext.has_value()
		&& expectedContext->ApplicationSession == core::application_session_id{ 1U }
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
	const auto exact = evidence.HostComposed
		&& evidence.Quiescent
		&& evidence.Statistics.CoreDecisions == 1U
		&& evidence.Statistics.WorkspaceStarts == 1U
		&& evidence.Statistics.ProcessStarts == 1U
		&& evidence.Statistics.WorkspaceCleanups == 1U
		&& cwd.has_value() && !std::filesystem::exists(*cwd)
		&& !evidence.WorkspaceRoot.empty()
		&& std::filesystem::exists(evidence.WorkspaceRoot)
		&& !std::filesystem::path{ *cwd }.string().starts_with(dataRoot.string())
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
				  << evidence.Quiescent << ':'
				  << evidence.Statistics.CoreDecisions << ':'
				  << evidence.Statistics.WorkspaceStarts << ':'
				  << evidence.Statistics.ProcessStarts << ':'
				  << evidence.Statistics.WorkspaceCleanups << ':'
				  << (actualFields.has_value() ? actualFields->size() : 0U) << ':'
				  << (expectedFields.has_value() ? expectedFields->size() : 0U)
				  << '\n';
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
