#include "tour_feedback.hpp"
#include "bpr/application/feedback_counters.hpp"
#include "bpr/core/feedback_conversation.hpp"
#include "bpr/core/feedback_label_columns.hpp"
#include <QByteArray>
#include <QJsonArray>
#include <QStringList>
#include <array>
namespace bpr::app {
	namespace {
		auto text(std::string_view aValue) -> QString { return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size())); }
		auto decimal(std::size_t aValue) -> QString { return QString::number(static_cast<qulonglong>(aValue)); }
		auto refuse(std::string aCode, std::string aMessage, int aExit) { return std::unexpected(tour_cli_error{ std::move(aCode), std::move(aMessage), aExit }); }
		auto digest_hex(const std::array<std::uint8_t, 32>& aDigest) -> QString
		{
			return QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(aDigest.data()), static_cast<qsizetype>(aDigest.size())).toHex());
		}
		auto form_name(core::feedback_header_form aForm) -> QString
		{
			switch (aForm) {
			case core::feedback_header_form::plain:         return "plain";
			case core::feedback_header_form::strong_header: return "strong_header";
			case core::feedback_header_form::strong_label:  return "strong_label";
			}
			return "plain";
		}
		auto decoration_kind_name(core::feedback_decoration_kind aKind) -> QString
		{
			switch (aKind) {
			case core::feedback_decoration_kind::custom:       return "custom";
			case core::feedback_decoration_kind::blocking:     return "blocking";
			case core::feedback_decoration_kind::non_blocking: return "non-blocking";
			case core::feedback_decoration_kind::if_minor:     return "if-minor";
			}
			return "custom";
		}
		auto span_json(std::string_view aBody, const std::optional<core::feedback_span>& aSpan, bool aWithText) -> QJsonValue
		{
			if (!aSpan) {
				return QJsonValue::Null;
			}
			QJsonObject value{ { "begin", decimal(aSpan->Begin) }, { "end", decimal(aSpan->End) } };
			if (aWithText) {
				value["text"] = text(core::feedback_text(aBody, *aSpan));
			}
			return value;
		}
		/// The label as the author wrote it, with the decorations as written: `issue (blocking, security)`.
		auto written_header(std::string_view aBody, const core::feedback_annotation& aAnnotation) -> QString
		{
			QString     result = aAnnotation.RawLabel ? text(core::feedback_text(aBody, *aAnnotation.RawLabel)) : QString{ };
			QStringList decorations;
			for (const auto& decoration : aAnnotation.Decorations) {
				decorations.append(text(core::feedback_text(aBody, decoration.Raw)));
			}
			if (!decorations.isEmpty()) {
				result += " (" + decorations.join(", ") + ")";
			}
			return result;
		}
		auto states_json(const core::feedback_state_counts& aStates) -> QJsonObject
		{
			QJsonObject value;
			for (std::size_t index = 0; index < core::feedback_resolution_count; ++index) {
				value[text(core::stable_name(static_cast<core::feedback_resolution>(index)))] = decimal(aStates[index]);
			}
			return value;
		}
		auto limits_json(const std::vector<core::feedback_count_limit>& aLimits) -> QJsonArray
		{
			QJsonArray value;
			for (const auto limit : aLimits) {
				value.append(text(core::stable_name(limit)));
			}
			return value;
		}
		auto mode_name(core::feedback_resolution_mode aMode) -> QString { return aMode == core::feedback_resolution_mode::all ? "all" : "open"; }
		auto row_kind_name(application::overview_feedback_row_kind aKind) -> QString
		{
			switch (aKind) {
			case application::overview_feedback_row_kind::file:         return "file";
			case application::overview_feedback_row_kind::folder:       return "folder";
			case application::overview_feedback_row_kind::remainder:    return "not_on_a_file";
			case application::overview_feedback_row_kind::pull_request: return "pull_request";
			}
			return "file";
		}
		auto row_json(const application::overview_feedback_row& aRow) -> QJsonObject
		{
			const auto& counts = aRow.Counts;
			QJsonArray  labels;
			for (const auto& dedicated : counts.Dedicated) {
				labels.append(QJsonObject{ { "id", text(core::stable_name(dedicated.Label)) }, { "count", decimal(dedicated.Count) }, { "states", states_json(dedicated.States) } });
			}
			const auto& parts = counts.CatchallParts;
			return { { "kind", row_kind_name(aRow.Kind) }, { "path", text(aRow.Path) }, { "included", decimal(counts.Included) }, { "scope_total", decimal(counts.ScopeTotal) }, { "uncertain", decimal(counts.Uncertain) }, { "unattributed", decimal(counts.Unattributed) }, { "unclassified", decimal(counts.ScopeUnclassified) }, { "exact", counts.Exact }, { "limits", limits_json(counts.Limits) }, { "states", states_json(counts.States) }, { "labels", labels }, { "other", QJsonObject{ { "count", decimal(counts.Catchall) }, { "states", states_json(counts.CatchallStates) }, { "unselected", decimal(parts.Unselected) }, { "plain", decimal(parts.Plain) }, { "custom", decimal(parts.Custom) }, { "unclassified", decimal(parts.Unclassified) } } } };
		}
		/// `src/a.cpp: 2 open — issue 1, Other 1 · ? 1 · not exact (resolution_unknown)`.
		auto row_human(const QString& aName, const core::feedback_counter_projection& aCounts) -> QString
		{
			const bool  all  = aCounts.Mode == core::feedback_resolution_mode::all;
			QString     line = aName + ": " + decimal(aCounts.Included) + (all ? " in all" : " open");
			QStringList parts;
			for (const auto& dedicated : aCounts.Dedicated) {
				parts.append(text(core::stable_name(dedicated.Label)) + " " + decimal(dedicated.Count));
			}
			parts.append("Other " + decimal(aCounts.Catchall));
			line += " — " + parts.join(", ");
			if (all) {
				const auto& s = aCounts.States;
				line += QStringLiteral(" (%1 open, %2 resolved, %3 unknown, %4 without resolution)").arg(decimal(s[0]), decimal(s[1]), decimal(s[2]), decimal(s[3]));
			}
			if (aCounts.Uncertain != 0U) {
				line += " · ? " + decimal(aCounts.Uncertain);
			}
			if (!aCounts.Exact) {
				QStringList limits;
				for (const auto limit : aCounts.Limits) {
					limits.append(text(core::stable_name(limit)));
				}
				line += " · not exact (" + limits.join(", ") + ")";
			}
			return line + "\n";
		}
	}

	auto tour_label_catalog() -> tour_feedback_output
	{
		QJsonArray labels;
		QString    human = QStringLiteral("Conventional Comments labels (catalog %1, grammar %2)\n").arg(QString::number(core::feedback_catalog_revision), QString::number(core::feedback_parser_revision));
		for (const auto& entry : core::feedback_label_catalog()) {
			labels.append(QJsonObject{ { "id", text(entry.Id) }, { "description", text(entry.Description) }, { "guidance", text(core::stable_name(entry.Guidance)) }, { "icon_role", text(entry.IconRole) } });
			human += text(entry.Id).leftJustified(11) + text(entry.Description) + "\n";
		}
		human += "Decorations: blocking, non-blocking, if-minor; any other word in the parentheses is kept as a custom decoration.\n";
		human += "plain: no label. A label is public intent only; it resolves nothing and chooses no review decision.\n";
		const QJsonArray decorations{ QJsonObject{ { "token", "blocking" }, { "expectation", "blocking" } }, QJsonObject{ { "token", "non-blocking" }, { "expectation", "non_blocking" } }, QJsonObject{ { "token", "if-minor" }, { "expectation", "if_minor" } } };
		return { { { "catalog", "bpr.feedback-label-catalog.v1" }, { "catalog_revision", QString::number(core::feedback_catalog_revision) }, { "parser_revision", QString::number(core::feedback_parser_revision) }, { "labels", labels }, { "decorations", decorations }, { "custom_decorations", "kept as written" }, { "plain", "plain" }, { "header_forms", QJsonArray{ "plain", "strong_header", "strong_label" } }, { "max_prefix_bytes", decimal(core::maximum_feedback_prefix_bytes) } }, human };
	}

	auto tour_label_annotation_json(std::string_view aBody, const core::feedback_annotation& aAnnotation) -> QJsonObject
	{
		QJsonArray decorations;
		for (const auto& decoration : aAnnotation.Decorations) {
			decorations.append(QJsonObject{ { "text", text(core::feedback_text(aBody, decoration.Raw)) }, { "kind", decoration_kind_name(decoration.Kind) }, { "begin", decimal(decoration.Raw.Begin) }, { "end", decimal(decoration.Raw.End) } });
		}
		QJsonArray diagnostics;
		for (const auto diagnostic : aAnnotation.Diagnostics) {
			diagnostics.append(text(core::stable_name(diagnostic)));
		}
		return { { "parser_revision", QString::number(aAnnotation.ParserRevision) }, { "body_sha256", digest_hex(aAnnotation.BodyDigest) }, { "body_bytes", decimal(aBody.size()) }, { "status", text(core::stable_name(aAnnotation.Status)) }, { "label", aAnnotation.Label ? QJsonValue{ text(core::stable_name(*aAnnotation.Label)) } : QJsonValue{ QJsonValue::Null } }, { "guidance", aAnnotation.Guidance ? QJsonValue{ text(core::stable_name(*aAnnotation.Guidance)) } : QJsonValue{ QJsonValue::Null } }, { "expectation", text(core::stable_name(aAnnotation.Expectation)) }, { "form", form_name(aAnnotation.Form) }, { "raw_label", span_json(aBody, aAnnotation.RawLabel, true) }, { "user_body", span_json(aBody, aAnnotation.UserBody, false) }, { "header", span_json(aBody, aAnnotation.Header, true) }, { "subject", span_json(aBody, aAnnotation.Subject, true) }, { "discussion", span_json(aBody, aAnnotation.Discussion, false) }, { "decorations", decorations }, { "diagnostics", diagnostics }, { "inspected_bytes", decimal(aAnnotation.InspectedBytes) } };
	}

	auto tour_label_annotation(std::string_view aBody, bool aPublished) -> tour_feedback_output
	{
		const auto segment    = aPublished ? core::published_feedback_segment(aBody) : core::feedback_body_segment{ };
		const auto annotation = core::recognize_feedback_body(aBody, segment);
		auto       data       = tour_label_annotation_json(aBody, annotation);
		data["segment"]       = aPublished ? (segment.Managed ? "published_managed" : "published_ordinary") : "ordinary";
		QString human;
		switch (annotation.Status) {
		case core::feedback_prefix_status::recognized:
			human = "recognized: " + written_header(aBody, annotation) + "\nExpectation: " + text(core::stable_name(annotation.Expectation)) + " · guidance: " + text(core::stable_name(*annotation.Guidance)) + "\n";
			if (annotation.Subject) {
				human += "Subject: " + text(core::feedback_text(aBody, *annotation.Subject)) + "\n";
			}
			break;
		case core::feedback_prefix_status::plain:
			human = annotation.Diagnostics.empty() ? "plain: no label; the body is ordinary text.\n" : "plain: no label could be read; the body is ordinary text.\n";
			break;
		case core::feedback_prefix_status::unknown_label:
			human = "unknown_label: \"" + written_header(aBody, annotation) + "\" is not a catalog label.\n";
			break;
		case core::feedback_prefix_status::malformed:
			human = "malformed: a header starts with \"" + written_header(aBody, annotation) + "\" but cannot be read; the text stays as written.\n";
			break;
		case core::feedback_prefix_status::limited:
			human = "limited: the header is longer than the recognizer reads; the body is not classified.\n";
			break;
		}
		QStringList diagnostics;
		for (const auto diagnostic : annotation.Diagnostics) {
			diagnostics.append(text(core::stable_name(diagnostic)));
		}
		human += "Diagnostics: " + (diagnostics.isEmpty() ? QStringLiteral("none") : diagnostics.join(", ")) + "\nBody SHA-256: " + digest_hex(annotation.BodyDigest) + "\n";
		return { data, human };
	}

	auto parse_tour_label_edit(const QMap<QString, QString>& aOptions) -> tour_cli_result<core::feedback_header_edit>
	{
		core::feedback_header_edit edit;
		if (aOptions.contains("--label")) {
			const auto value = aOptions["--label"].toStdString();
			const auto label = core::find_feedback_label(value);
			if (value == "plain") {
				edit.Change = core::feedback_label_change::remove;
			} else if (label && core::stable_name(*label) == value) {
				edit.Change = core::feedback_label_change::set;
				edit.Label  = *label;
			} else {
				return refuse("invalid_label", "Use a catalog label ID as bpr tour labels lists it, in lowercase, or plain.", 2);
			}
		}
		const auto weights = static_cast<int>(aOptions.contains("--expectation")) + static_cast<int>(aOptions.contains("--decoration")) + static_cast<int>(aOptions.contains("--remove-decoration"));
		if (weights > 1) {
			return refuse("invalid_input", "Give one of --expectation, --decoration or --remove-decoration per edit.", 2);
		}
		if (aOptions.contains("--expectation")) {
			const auto value = aOptions["--expectation"];
			if (value == "blocking") {
				edit.Expectation = core::feedback_expectation::blocking;
			} else if (value == "non-blocking") {
				edit.Expectation = core::feedback_expectation::non_blocking;
			} else if (value == "if-minor") {
				edit.Expectation = core::feedback_expectation::if_minor;
			} else if (value == "none") {
				edit.Expectation = core::feedback_expectation::unspecified;
			} else {
				return refuse("invalid_input", "Use --expectation blocking, non-blocking, if-minor or none.", 2);
			}
		}
		for (const auto* option : { "--decoration", "--remove-decoration" }) {
			if (aOptions.contains(option)) {
				const auto token = aOptions[option].toStdString();
				if (!core::valid_feedback_decoration(token)) {
					return refuse("invalid_decoration", "A decoration is 1 to 32 ASCII letters, digits, - or _.", 2);
				}
				core::feedback_decoration_change change;
				change.Token    = token;
				change.Written  = QString{ option } == "--decoration";
				edit.Decoration = std::move(change);
			}
		}
		if (edit.Change == core::feedback_label_change::keep && weights == 0) {
			return refuse("invalid_input", "Name the change: --label ID|plain, --expectation, --decoration or --remove-decoration.", 2);
		}
		if (edit.Change == core::feedback_label_change::remove && weights != 0) {
			return refuse("invalid_input", "--label plain removes the whole header; it takes no decoration.", 2);
		}
		return edit;
	}

	auto tour_label_edit_refusal(const core::feedback_tour_stop_edit_plan& aPlan) -> tour_cli_error
	{
		if (aPlan.Refusal == core::feedback_draft_edit_refusal::stale_draft) {
			return { "revision_conflict", "The draft changed since the revision you read. Read the stop again; nothing was changed.", 4 };
		}
		if (aPlan.Refusal == core::feedback_draft_edit_refusal::not_editable) {
			return { "read_only", "This tour is read-only; its stops keep their text. Nothing was changed.", 2 };
		}
		switch (aPlan.Decision.Refusal) {
		case core::feedback_edit_refusal::stale_revision: return { "revision_conflict", "The stop changed since the revision you read. Read it again; nothing was changed.", 4 };
		case core::feedback_edit_refusal::invalid_edit:   return { "invalid_edit", "That edit does not apply to this stop: a decoration needs a label, and conflicting is not a weight to write. Nothing was changed.", 2 };
		case core::feedback_edit_refusal::unsupported_header:
			return { "unsupported_header", "The stop's header is malformed or too long to edit safely, so it stays exactly as written. Fix it by hand with bpr tour edit stop:ID:body.", 5 };
		case core::feedback_edit_refusal::restructuring_required:
			return { "restructuring_required", "The stop begins with a quote, list, heading, fence, HTML or table, which cannot take a header without restructuring. Nothing was deleted to make it fit; edit it by hand.", 5 };
		case core::feedback_edit_refusal::body_limit: return { "input_too_large", "The labelled comment would exceed the stop's size bound. Nothing was changed.", 2 };
		case core::feedback_edit_refusal::none:       break;
		}
		return { "operation_failed", "The label edit was refused; nothing was changed.", 7 };
	}

	auto parse_tour_counts_options(const QMap<QString, QString>& aOptions) -> tour_cli_result<tour_counts_options>
	{
		tour_counts_options result;
		if (aOptions.contains("--columns")) {
			const auto value = aOptions["--columns"].toStdString();
			if (value == "none") {
				result.Columns = core::feedback_label_columns{ };
			} else {
				const auto reading = core::read_feedback_label_columns(value);
				if (value.empty() || reading.Unknown != 0U || reading.Repeated != 0U) {
					return refuse("invalid_columns", "Use --columns with comma-separated catalog label IDs, each once (bpr tour labels), or none.", 2);
				}
				result.Columns = reading.Columns;
			}
		}
		if (aOptions.contains("--mode")) {
			const auto value = aOptions["--mode"];
			if (value != "open" && value != "all") {
				return refuse("invalid_input", "Use --mode open (positively unresolved threads) or --mode all.", 2);
			}
			result.Mode = value == "all" ? core::feedback_resolution_mode::all : core::feedback_resolution_mode::unresolved;
		}
		return result;
	}

	auto count_tour_feedback(const application::tour_saved_target& aTarget, const application::github_pull_request_snapshot& aSnapshot, const application::overview_feedback_request& aRequest) -> tour_cli_result<tour_feedback_counts>
	{
		application::feedback_counter_model model;
		const auto                          update = model.observe(core::cached_review_key{ aTarget.Owner.HostAuthority.bytes(), aTarget.HostProfile, aTarget.Owner.Host, aTarget.Owner.Account, aTarget.Repository, aTarget.Number }, aSnapshot);
		if (!update.Admitted) {
			return refuse("counts_unavailable", "The pull request's conversations could not be counted (" + std::string{ core::stable_name(update.AnnotationRefusal) } + ", " + std::string{ core::stable_name(update.PopulationRefusal) } + "). No count is shown rather than a guessed one.", 5);
		}
		std::vector<std::string> files;
		files.reserve(aSnapshot.Files.size());
		for (const auto& file : aSnapshot.Files) {
			files.push_back(file.Path);
		}
		auto projection = application::project_overview_feedback(model, files, aRequest);
		if (!projection) {
			return refuse("counts_unavailable", "The counts could not be projected for these columns. No count is shown rather than a guessed one.", 5);
		}
		tour_feedback_counts result;
		result.Projection          = std::move(*projection);
		result.Collection          = update.Collection;
		result.TourIndexReferences = update.TourIndexReferences;
		return result;
	}

	auto tour_feedback_counts_output(const tour_feedback_counts& aCounts) -> tour_feedback_output
	{
		const auto& projection = aCounts.Projection;
		const auto& whole      = projection.PullRequest.Counts;
		QJsonArray  columns;
		for (const auto label : core::labels_of(projection.Request.Columns)) {
			columns.append(text(core::stable_name(label)));
		}
		const auto rows = [](const std::vector<application::overview_feedback_row>& aRows, std::size_t& aQuiet, bool& aQuietExact) {
			QJsonArray value;
			for (const auto& row : aRows) {
				if (row.Counts.ScopeTotal == 0U) {
					++aQuiet;
					aQuietExact = aQuietExact && row.Counts.Exact;
					continue;
				}
				value.append(row_json(row));
			}
			return value;
		};
		std::size_t quietFiles = 0U, quietFolders = 0U;
		bool        quietFilesExact = true, quietFoldersExact = true;
		const auto  files   = rows(projection.Files, quietFiles, quietFilesExact);
		const auto  folders = rows(projection.Folders, quietFolders, quietFoldersExact);
		QJsonObject data{ { "unit", "conversation" }, { "local_drafts_counted", false }, { "columns", columns }, { "columns_source", aCounts.ColumnsFromOption ? "option" : "settings" }, { "mode", mode_name(projection.Request.Mode) }, { "mode_source", aCounts.ModeFromOption ? "option" : "settings" }, { "generation", QString::number(projection.Generation) }, { "epoch", QString::number(whole.Epoch) }, { "collection", text(core::stable_name(aCounts.Collection)) }, { "exact", whole.Exact }, { "limits", limits_json(whole.Limits) }, { "tour_index_references", decimal(aCounts.TourIndexReferences) }, { "pull_request", row_json(projection.PullRequest) }, { "not_on_a_file", row_json(projection.Remainder) }, { "folders", folders }, { "files", files }, { "quiet_files", QJsonObject{ { "count", decimal(quietFiles) }, { "exact", quietFilesExact } } }, { "quiet_folders", QJsonObject{ { "count", decimal(quietFolders) }, { "exact", quietFoldersExact } } } };
		QString     human = row_human("Pull request", whole) + row_human("Not on a file", projection.Remainder.Counts);
		for (const auto& row : projection.Folders) {
			if (row.Counts.ScopeTotal != 0U) {
				human += row_human((row.Path.empty() ? QStringLiteral("(root)") : text(row.Path)) + "/", row.Counts);
			}
		}
		for (const auto& row : projection.Files) {
			if (row.Counts.ScopeTotal != 0U) {
				human += row_human(text(row.Path), row.Counts);
			}
		}
		if (quietFiles != 0U) {
			human += decimal(quietFiles) + (quietFilesExact ? " other listed files have no conversation.\n" : " other listed files show no conversation, but that zero is not exact.\n");
		}
		human += "Counts are conversations: a thread counts once, by its root's label. ? counts threads whose resolution is unknown or does not apply, or whose label cannot be read. Local tour stops are drafts and are not counted.\n";
		return { data, human };
	}
}
