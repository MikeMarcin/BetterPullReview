#include "tour_cli.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/review_tour_markdown.hpp"
#include "terminal_text.hpp"
#include "tour_cancellation.hpp"
#include "tour_editor.hpp"
#include "tour_text_input.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include <charconv>
#include <iostream>
#include <limits>
namespace bpr::app {
	namespace {
		auto text(std::string_view aValue) -> QString { return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size())); }
		struct command_options
		{
			QString                             Command;
			QString                             RequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
			QStringList                         Positional;
			QMap<QString, QString>              Values;
			bool                                Json = false, Interactive = false;
			std::optional<core::tour_attention> WalkAttention;
		};
		auto refuse(std::string aCode, std::string aMessage, int aExit = 2) { return std::unexpected(tour_cli_error{ std::move(aCode), std::move(aMessage), aExit }); }
		auto supported_commands() -> QStringList { return { "capabilities", "connect", "open", "status", "show", "edit", "move", "remove", "ready", "add", "undo", "redo", "walk", "preview", "publish", "recover", "hooks", "assist", "outline" }; }
		/// [[BPR-1068]] What this build offers on its own platform. Nothing here is a
		/// qualification: no platform has a release package or an observed live
		/// sign-in, read or publication, so every one says so.
		auto platform_status() -> QJsonObject
		{
#if defined(__APPLE__)
			return { { "os", "macos" }, { "native_vault", "Keychain" }, { "gh_transport", "explicit opt-in" }, { "package", "development install; no release package" }, { "qualification", "not yet qualified" } };
#elif defined(_WIN32)
			return { { "os", "windows" }, { "native_vault", "Credential Manager" }, { "gh_transport", "unavailable" }, { "package", "development install; no release package" }, { "qualification", "not yet qualified" } };
#else
			return { { "os", "linux" }, { "native_vault", "Secret Service default collection, unlocked; checked before a code is shown" }, { "gh_transport", "unavailable" }, { "package", "development install; no release package" }, { "qualification", "not yet qualified" } };
#endif
		}
		auto local_edit(const QString& aCommand) -> bool { return QStringList{ "edit", "move", "remove", "ready", "add", "undo", "redo" }.contains(aCommand); }
		auto service_operation(const QString& aCommand) -> application::tour_service_operation
		{
			if (aCommand == "undo") {
				return application::tour_service_operation::undo;
			}
			if (aCommand == "redo") {
				return application::tour_service_operation::redo;
			}
			return application::tour_service_operation::edit;
		}
		auto completion_origin_name(core::tour_completion_origin aOrigin) -> QString
		{
			if (aOrigin == core::tour_completion_origin::observed) {
				return "observed";
			}
			if (aOrigin == core::tour_completion_origin::relayed) {
				return "relayed";
			}
			return "unspecified";
		}
		auto stop_destination(const core::tour_stop& aStop) -> std::optional<core::tour_code_destination>
		{
			if (aStop.Mapped) {
				return core::destination_of(*aStop.Mapped);
			}
			if (aStop.Original) {
				return core::destination_of(*aStop.Original);
			}
			return aStop.Destination;
		}
		auto render_source(const tour_source_material& aSource) -> QString
		{
			QString    result = "Pinned " + text(aSource.Source.Path) + " @ " + text(core::canonical_hex(aSource.Source.Commit).view()) + (aSource.Source.First.Side == core::diff_side::left ? " LEFT\n" : " RIGHT\n");
			const auto lines  = text(aSource.Text).split('\n');
			for (qsizetype i = 0; i < lines.size(); ++i) {
				if (i + 1 == lines.size() && lines[i].isEmpty()) {
					break;
				}
				result += QString::number(aSource.Source.First.Line + 1 + static_cast<std::uint32_t>(i)) + " | " + lines[i] + "\n";
			}
			return result;
		}

		auto parse(const QStringList& aArguments) -> tour_cli_result<command_options>
		{
			command_options result;
			if (aArguments.size() > 80) {
				return refuse("input_too_large", "Too many command arguments.");
			}
			const QStringList flags{ "--json", "--non-interactive", "--no-color", "--offline", "--source", "--refresh", "--compatibility", "--description" };
			const QStringList options{ "--profile", "--draft", "--root", "--repo", "--host", "--transport", "--data-root", "--schema-version", "--cursor", "--file", "--expected-revision", "--before", "--after", "--side", "--commit", "--path", "--lines", "--resume", "--attempt", "--request-id", "--hook", "--field", "--range-bytes", "--quote", "--sources", "--session", "--ticket", "--result", "--indices" };
			QStringList       words;
			for (qsizetype i = 0; i < aArguments.size(); ++i) {
				const auto& value = aArguments[i];
				if (value.size() > 8192) {
					return refuse("input_too_large", "Command arguments exceed the bound.");
				}
				if (!value.startsWith("--")) {
					words.push_back(value);
					continue;
				}
				if (result.Values.contains(value)) {
					return refuse("invalid_input", "An option was repeated.");
				}
				if (flags.contains(value)) {
					result.Values.insert(value, "true");
					continue;
				}
				if (!options.contains(value) || i + 1 == aArguments.size() || aArguments[i + 1].startsWith("--")) {
					return refuse("unsupported_option", "Unknown option or missing option value.");
				}
				result.Values.insert(value, aArguments[++i]);
			}
			result.Json        = result.Values.contains("--json");
			result.Interactive = interactive_terminal() && !result.Json && !result.Values.contains("--non-interactive");
			if (result.Values.contains("--schema-version") && result.Values["--schema-version"] != "1") {
				return refuse("unsupported_schema", "Only CLI schema version 1 is supported.");
			}
			if (words.isEmpty() || words == QStringList{ "tour", "help" }) {
				result.Command = "help";
				return result;
			}
			if (words.size() < 2 || words[0] != "tour") {
				return refuse("unsupported_command", "Use bpr tour capabilities.");
			}
			result.Command      = words[1];
			result.Positional   = words.mid(2);
			const auto commands = supported_commands();
			if (!commands.contains(result.Command)) {
				return refuse("unsupported_command", "Use bpr tour capabilities for implemented commands.");
			}
			const bool takesArgument     = QStringList{ "open", "show", "edit", "move", "remove", "ready" }.contains(result.Command);
			const auto expectedArguments = (takesArgument || (result.Command == "add" && !result.Values.contains("--path"))) ? 1 : 0;
			if (result.Command == "hooks") {
				const auto sub = result.Positional.value(0);
				if (!(sub == "show" && result.Positional.size() <= 2) && !((sub == "edit" || sub == "reset") && result.Positional.size() == 2)) {
					return refuse("invalid_input", "Use hooks show [HOOK], hooks edit HOOK or hooks reset HOOK.");
				}
			} else if (result.Command == "assist") {
				if (result.Positional.size() != 1 || !QStringList{ "context", "complete", "cancel" }.contains(result.Positional[0])) {
					return refuse("invalid_input", "Use assist context, assist complete or assist cancel.");
				}
			} else if (result.Command == "outline") {
				if (result.Positional.size() != 2 || result.Positional[0] != "use") {
					return refuse("invalid_input", "Use outline use PROPOSAL --indices LIST.");
				}
			} else if (result.Positional.size() != expectedArguments) {
				return refuse("invalid_input", "Unexpected positional arguments.");
			}
			if (result.Values.contains("--request-id")) {
				result.RequestId = result.Values["--request-id"];
				if (result.RequestId.isEmpty() || result.RequestId.size() > 64 || !std::ranges::all_of(result.RequestId, [](QChar aChar) { return aChar.isLetterOrNumber() || aChar == '-'; })) {
					return refuse("invalid_request_id", "Use at most 64 alphanumeric or hyphen characters.");
				}
			}
			QStringList allowed{ "--json", "--non-interactive", "--no-color", "--schema-version", "--data-root" };
			if (result.Command != "capabilities") {
				allowed.append("--profile");
			}
			if (result.Command == "connect") {
				allowed.append({ "--host", "--transport" });
			}
			if (result.Command == "open") {
				allowed.append({ "--repo", "--transport", "--root", "--offline", "--cursor" });
			}
			if (result.Command == "status" || result.Command == "show" || local_edit(result.Command) || result.Command == "walk" || result.Command == "preview" || result.Command == "publish" || result.Command == "recover") {
				allowed.append("--draft");
			}
			if (result.Command == "preview" || result.Command == "publish" || result.Command == "recover") {
				allowed.append({ "--transport", "--compatibility", "--description" });
			}
			if (result.Command == "preview") {
				allowed.append("--refresh");
			}
			if (result.Command == "publish") {
				allowed.append("--resume");
			}
			if (result.Command == "recover") {
				allowed.append("--attempt");
			}
			if (result.Command == "status" || result.Command == "preview") {
				allowed.append("--cursor");
			}
			if (result.Command == "status") {
				allowed.append("--request-id");
			}
			if (result.Command == "show") {
				allowed.append("--source");
			}
			if (local_edit(result.Command)) {
				allowed.append("--request-id");
				allowed.append("--expected-revision");
			}
			if (result.Command == "edit") {
				allowed.append("--file");
			}
			if (result.Command == "add") {
				allowed.append({ "--file", "--path", "--lines", "--side", "--commit", "--transport" });
			}
			if ((result.Command == "show" && result.Values.contains("--source")) || result.Command == "walk") {
				allowed.append("--transport");
			}
			if (result.Command == "move") {
				allowed.append({ "--before", "--after" });
			}
			if (result.Command == "hooks" && result.Positional.value(0) == "edit") {
				allowed.append("--file");
			}
			if (result.Command == "assist") {
				allowed.append("--draft");
				if (result.Positional[0] == "context") {
					allowed.append({ "--hook", "--field", "--range-bytes", "--quote", "--sources", "--session", "--transport" });
				} else if (result.Positional[0] == "complete") {
					allowed.append({ "--ticket", "--result", "--request-id" });
				}
			}
			if (result.Command == "outline") {
				allowed.append({ "--draft", "--indices", "--after", "--request-id" });
			}
			for (const auto& option : result.Values.keys()) {
				if (!allowed.contains(option)) {
					return refuse("unsupported_option", "This option is not valid for this command.");
				}
			}
			return result;
		}
		auto number(const QString& aValue) -> std::optional<std::uint64_t>
		{
			const auto    bytes  = aValue.toUtf8();
			std::uint64_t value  = 0;
			const auto    parsed = std::from_chars(bytes.data(), bytes.data() + bytes.size(), value);
			if (parsed.ec != std::errc{ } || parsed.ptr != bytes.data() + bytes.size() || !value) {
				return std::nullopt;
			}
			return value;
		}
		struct pr_target
		{
			std::string               Host;
			core::repository_locator  Repository;
			core::pull_request_number Number;
		};
		auto resolve_pr(const command_options& aOptions) -> tour_cli_result<pr_target>
		{
			QString url = aOptions.Positional[0];
			if (number(url)) {
				const auto repo = aOptions.Values.value("--repo");
				if (repo.isEmpty()) {
					return refuse("target_ambiguous", "A PR number needs --repo HOST/OWNER/REPO or a full PR URL.");
				}
				url = "https://" + repo + "/pull/" + url;
			}
			const QUrl parsed{ url, QUrl::StrictMode };
			const auto parts = parsed.path().split('/');
			if (!parsed.isValid() || parsed.scheme() != "https" || !parsed.userInfo().isEmpty() || parsed.hasQuery() || parsed.hasFragment() || parsed.port() != -1 || parts.size() != 5 || parts[0] != "" || parts[3] != "pull" || !number(parts[4]) || *number(parts[4]) > std::numeric_limits<std::uint32_t>::max()) {
				return refuse("invalid_target", "Use a canonical https://HOST/OWNER/REPO/pull/NUMBER URL.");
			}
			core::repository_locator repo{ parts[1].toStdString(), parts[2].toStdString() };
			if (!core::valid(repo) || !application::valid_github_canonical_hostname(parsed.host().toStdString())) {
				return refuse("invalid_target", "The repository or host is invalid.");
			}
			return pr_target{ parsed.host().toStdString(), std::move(repo), core::pull_request_number{ static_cast<std::uint32_t>(*number(parts[4])) } };
		}
		auto remote_identity(const std::optional<core::tour_remote_binding>& aRemote) -> QJsonValue
		{
			if (!aRemote) {
				return QJsonValue::Null;
			}
			const auto& remote = *aRemote;
			const auto  kind   = remote.Kind == core::tour_remote_kind::review ? "review" : remote.Kind == core::tour_remote_kind::line_comment ? "line_comment"
				: remote.Kind == core::tour_remote_kind::file_comment                                                                           ? "file_comment"
																																				: "conversation_comment";
			return QJsonObject{ { "kind", kind }, { "database_id", QString::number(remote.DatabaseId) }, { "node_id", text(remote.Entity) }, { "actor", text(remote.Actor) }, { "url", text(remote.WebUrl) }, { "review", text(remote.Review) }, { "own", remote.Own }, { "resolved", remote.Resolved }, { "outdated", remote.Outdated } };
		}
		auto source_binding(const core::tour_stop& aStop) -> QJsonValue
		{
			const auto source = stop_destination(aStop);
			if (!source) {
				return QJsonValue::Null;
			}
			return QJsonObject{ { "path", text(source->Path) }, { "commit", text(core::canonical_hex(source->Commit).view()) }, { "side", source->Side == core::diff_side::left ? "LEFT" : "RIGHT" }, { "first_line", QString::number(source->FirstLine + 1) }, { "last_line", QString::number(source->LastLine + 1) }, { "blob_observed", source->Blob.has_value() } };
		}
		auto identities(const tour_local_snapshot& aSnapshot) -> QJsonObject
		{
			const auto& target = aSnapshot.Target;
			const auto& owner  = target.Owner;
			return { { "profile_id", QString::number(owner.Profile.value()) }, { "draft_id", text(tour_runtime::draft_token(target)) }, { "account_id", QString::number(owner.Account.value()) }, { "repository_id", QString::number(owner.Repository.value()) }, { "pull_request_id", QString::number(owner.PullRequest.value()) }, { "host", text(target.Hostname) }, { "repository", text(target.Repository.Owner + "/" + target.Repository.Name) }, { "number", QString::number(target.Number.value()) }, { "revision", QString::number(aSnapshot.Draft.Revision) }, { "head", text(core::canonical_hex(aSnapshot.Draft.Head).view()) }, { "local_state", "Saved locally" }, { "published_observation", text(aSnapshot.PublishedObservation) }, { "pending_server_candidates", QString::number(aSnapshot.PendingServerReviews) }, { "published_root", remote_identity(aSnapshot.Draft.Remote) }, { "prior_private_context", "unknown; GitHub does not contain unsent WIP, prior AI provenance or approval" }, { "published_change_history", aSnapshot.Draft.HasPriorObservation ? "observed_in_this_profile" : "unknown_before_this_profile" }, { "published", aSnapshot.Draft.Remote.has_value() }, { "read_only", aSnapshot.Draft.ReadOnly }, { "remote_stale", aSnapshot.Draft.RemoteStale }, { "conflict_count", QString::number(aSnapshot.Draft.Conflicts.size()) }, { "source_state", "not_loaded" }, { "undo_label", text(aSnapshot.UndoLabel) }, { "redo_label", text(aSnapshot.RedoLabel) }, { "operation_revision", aSnapshot.Receipt ? QString::number(aSnapshot.Receipt->Revision) : QString{ } }, { "operation_recorded", aSnapshot.Receipt.has_value() }, { "resume_stop", QString::number(aSnapshot.Resume.Stop.value()) } };
		}
		struct presentation
		{
			QJsonObject Data;
			QString     Human;
			QJsonArray  Warnings;
			QString     Status = "ok";
			int         Exit   = 0;
		};
		auto render_snapshot(const command_options& aOptions, const tour_local_snapshot& aSnapshot) -> tour_cli_result<presentation>
		{
			const auto&  draft = aSnapshot.Draft;
			presentation result{ identities(aSnapshot), { }, { } };
			result.Human = QStringLiteral("%1/%2/%3#%4\nDraft %5\nrevision %6\nSaved locally; local changes are not published.\n").arg(text(aSnapshot.Target.Hostname), text(aSnapshot.Target.Repository.Owner), text(aSnapshot.Target.Repository.Name), QString::number(aSnapshot.Target.Number.value()), text(tour_runtime::draft_token(aSnapshot.Target)), QString::number(draft.Revision));
			if (draft.Remote) {
				result.Human += "Published root: " + text(draft.Remote->WebUrl) + "\nEarlier private context and provenance are unknown after profile loss.\n";
			}
			if (aOptions.Values.contains("--offline")) {
				result.Warnings.append(QJsonObject{ { "code", "stale_cached" }, { "message", "Local state only; published material and source have not been refreshed." } });
			}
			if (!draft.ImportNotice.empty()) {
				result.Warnings.append(QJsonObject{ { "code", "import_notice" }, { "message", text(draft.ImportNotice).left(4096) + (text(draft.ImportNotice).size() > 4096 ? " [truncated]" : "") }, { "message_truncated", text(draft.ImportNotice).size() > 4096 } });
			}
			if (aOptions.Command == "show") {
				const auto field = aOptions.Positional[0];
				QString    value;
				if (field == "problem") {
					value = text(draft.Problem);
				} else if (field == "solution") {
					value = text(draft.Solution);
				} else if (field.startsWith("stop:") && field.split(':').size() >= 2 && field.split(':').size() <= 3 && number(field.split(':')[1]) && (field.split(':').size() == 2 || field.split(':')[2] == "title" || field.split(':')[2] == "body")) {
					const auto stop = std::ranges::find(draft.Stops, core::tour_stop_id{ *number(field.split(':')[1]) }, &core::tour_stop::Id);
					if (stop == draft.Stops.end()) {
						return refuse("unknown_stop", "No stop has that stable ID.");
					}
					value                          = field.endsWith(":title") ? text(stop->Title) : text(stop->Body);
					result.Data["title"]           = text(stop->Title).left(256);
					result.Data["title_truncated"] = text(stop->Title).size() > 256;
					result.Data["stop_id"]         = QString::number(stop->Id.value());
					result.Data["remote"]          = remote_identity(stop->Remote);
					result.Data["source_binding"]  = source_binding(*stop);
				} else {
					return refuse("invalid_field", "Use problem, solution or stop:ID.");
				}
				result.Data["field"] = field;
				result.Data["text"]  = value;
				result.Human += value + "\n";
				if (aOptions.Values.contains("--source")) {
					result.Data["source_availability"] = "unavailable";
					result.Warnings.append(QJsonObject{ { "code", "source_unavailable" }, { "message", "Pinned source was not loaded by this read; no worktree substitution occurred." } });
				}
			} else {
				std::size_t cursor = 0;
				if (aOptions.Values.contains("--cursor")) {
					auto parsed = number(aOptions.Values["--cursor"]);
					if (!parsed || *parsed > draft.Stops.size()) {
						return refuse("invalid_cursor", "The outline cursor is invalid.");
					}
					cursor = static_cast<std::size_t>(*parsed);
				}
				QJsonArray  stops;
				auto        end   = cursor;
				std::size_t bytes = 0;
				while (end < draft.Stops.size() && end < cursor + 20 && bytes + draft.Stops[end].Title.size() <= core::maximum_tour_field_bytes) {
					bytes += draft.Stops[end].Title.size();
					++end;
				}
				for (auto i = cursor; i < end; ++i) {
					const auto& stop = draft.Stops[i];
					stops.append(QJsonObject{ { "stop_id", QString::number(stop.Id.value()) }, { "title", text(stop.Title) }, { "ready", stop.Preparation == core::tour_preparation::ready }, { "can_ready", core::ready(stop) }, { "completion_origin", completion_origin_name(stop.CompletionOrigin) }, { "suggested", stop.Suggestion.has_value() }, { "visited", stop.Suggestion.has_value() && stop.Suggestion->Visited }, { "remote", remote_identity(stop.Remote) }, { "source_binding", source_binding(stop) } });
					result.Human += QStringLiteral("%1. %2 [%3]\n").arg(QString::number(stop.Id.value()), text(stop.Title), stop.Preparation == core::tour_preparation::ready ? "Ready" : "Editing");
				}
				result.Data["stops"]       = stops;
				result.Data["next_cursor"] = end < draft.Stops.size() ? QJsonValue{ QString::number(end) } : QJsonValue{ QJsonValue::Null };
				result.Data["complete"]    = end == draft.Stops.size();
				if (end < draft.Stops.size()) {
					result.Human += "More stops: --cursor " + QString::number(end) + "\n";
				}
			}
			return result;
		}
		auto source_destination(const command_options& aOptions, const core::review_tour& aDraft) -> tour_cli_result<core::tour_code_destination>
		{
			QString path = aOptions.Values.value("--path"), lines = aOptions.Values.value("--lines");
			if (path.isEmpty()) {
				const auto value = aOptions.Positional[0];
				const auto colon = value.lastIndexOf(':');
				if (colon < 1 || !lines.isEmpty()) {
					return refuse("invalid_source", "Use PATH:START-END or --path PATH --lines START-END.");
				}
				path  = value.first(colon);
				lines = value.mid(colon + 1);
			}
			const auto range = lines.split('-');
			const auto first = range.size() == 2 ? number(range[0]) : std::nullopt;
			const auto last  = range.size() == 2 ? number(range[1]) : std::nullopt;
			if (!first || !last || *first > *last || *last > std::numeric_limits<std::uint32_t>::max() || *last - *first >= 200) {
				return refuse("invalid_source", "Source lines are one-based inclusive; select at most 200 lines.");
			}
			const auto side = aOptions.Values.value("--side", "RIGHT");
			if (side != "LEFT" && side != "RIGHT") {
				return refuse("invalid_source", "Use --side LEFT or RIGHT.");
			}
			const auto commit = aOptions.Values.contains("--commit") ? core::parse_git_commit_oid(aOptions.Values["--commit"].toStdString()) : std::optional{ side == "LEFT" ? aDraft.Base : aDraft.Head };
			if (!commit) {
				return refuse("invalid_source", "Use a full pinned commit OID.");
			}
			core::tour_code_destination destination{ .Commit = *commit, .Path = path.toStdString(), .Side = side == "LEFT" ? core::diff_side::left : core::diff_side::right, .FirstLine = static_cast<std::uint32_t>(*first - 1), .LastLine = static_cast<std::uint32_t>(*last - 1) };
			if (!core::valid(destination)) {
				return refuse("invalid_source", "The relative repository source path or range is invalid.");
			}
			return destination;
		}

		auto edit_command(const command_options& aOptions, const core::review_tour& aDraft) -> tour_cli_result<core::tour_command>
		{
			const bool machine  = !aOptions.Interactive;
			const auto expected = aOptions.Values.contains("--expected-revision") ? number(aOptions.Values["--expected-revision"]) : (machine ? std::nullopt : std::optional{ aDraft.Revision });
			if (!expected) {
				return refuse("revision_required", "Local edits require --expected-revision from the current snapshot.");
			}
			// A caller that names its request ID may be retrying after a lost answer. The host returns the
			// stored outcome for that exact ID and payload and refuses a stale revision for anything else.
			if (*expected != aDraft.Revision && !aOptions.Values.contains("--request-id")) {
				return refuse("revision_conflict", "The draft changed. Read its current revision before editing.", 4);
			}
			core::tour_command command{ .Draft = aDraft.Id, .Owner = aDraft.Owner, .ExpectedRevision = *expected };
			if (aOptions.Command == "undo" || aOptions.Command == "redo") {
				return command;
			}
			if (aOptions.Command == "add") {
				command.Kind     = core::tour_command_kind::insert;
				command.Position = aDraft.Stops.size();
				return command;
			}
			const auto argument = aOptions.Positional[0];
			if (aOptions.Command == "edit" && (argument == "problem" || argument == "solution")) {
				command.Kind = argument == "problem" ? core::tour_command_kind::problem : core::tour_command_kind::solution;
				return command;
			}
			const auto parts   = argument.split(':');
			const bool editing = aOptions.Command == "edit";
			const auto id      = editing && (parts.size() == 2 || parts.size() == 3) && parts[0] == "stop" ? number(parts[1]) : (editing ? std::nullopt : number(argument));
			if (!id || (editing && parts.size() == 3 && parts[2] != "title" && parts[2] != "body")) {
				return refuse("invalid_field", "Use problem, solution, stop:ID[:title|body], or a stable numeric stop ID for move/remove/ready.");
			}
			const auto stop = std::ranges::find(aDraft.Stops, core::tour_stop_id{ *id }, &core::tour_stop::Id);
			if (stop == aDraft.Stops.end()) {
				return refuse("unknown_stop", "No stop has that stable ID.");
			}
			command.Stop = stop->Id;
			if (editing) {
				command.Kind  = core::tour_command_kind::edit_stop;
				command.Value = *stop;
			} else if (aOptions.Command == "remove") {
				command.Kind = core::tour_command_kind::remove;
			} else if (aOptions.Command == "ready") {
				command.Kind             = core::tour_command_kind::complete;
				command.CompletionOrigin = aOptions.WalkAttention ? core::tour_completion_origin::observed : core::tour_completion_origin::relayed;
				if (aOptions.WalkAttention) {
					command.Attention = *aOptions.WalkAttention;
				}
			} else {
				if (aOptions.Values.contains("--before") == aOptions.Values.contains("--after")) {
					return refuse("invalid_input", "Move requires exactly one --before or --after stable stop ID.");
				}
				const bool before = aOptions.Values.contains("--before");
				const auto anchor = number(aOptions.Values[before ? "--before" : "--after"]);
				const auto found  = anchor ? std::ranges::find(aDraft.Stops, core::tour_stop_id{ *anchor }, &core::tour_stop::Id) : aDraft.Stops.end();
				if (found == aDraft.Stops.end() || found == stop) {
					return refuse("invalid_input", "Move requires a different existing anchor stop.");
				}
				const auto old      = static_cast<std::size_t>(stop - aDraft.Stops.begin());
				auto       position = static_cast<std::size_t>(found - aDraft.Stops.begin());
				if (old < position) {
					--position;
				}
				if (!before) {
					++position;
				}
				command.Kind     = core::tour_command_kind::move;
				command.Position = position;
			}
			return command;
		}

		auto preview(const command_options& aOptions, const tour_local_snapshot& aSnapshot) -> tour_cli_result<presentation>
		{
			const auto& draft  = aSnapshot.Draft;
			std::size_t cursor = 0;
			if (aOptions.Values.contains("--cursor")) {
				const auto parsed = number(aOptions.Values["--cursor"]);
				if (!parsed || *parsed >= draft.Stops.size() + 2) {
					return refuse("invalid_cursor", "The preview cursor is invalid.");
				}
				cursor = static_cast<std::size_t>(*parsed);
			}
			auto                                     data = identities(aSnapshot);
			std::vector<core::tour_destination_plan> destinations;
			for (const auto& stop : draft.Stops) {
				const auto source = stop_destination(stop);
				destinations.push_back(source ? core::plan_tour_destination({ .Source = *source }) : core::tour_destination_plan{ });
			}
			const auto readiness           = draft.Remote ? core::assess_tour_update(draft, destinations, false, core::tour_operation_support::unknown) : core::assess_tour_publication(draft, destinations, false);
			data["publication_readiness"]  = readiness == core::tour_publication_readiness::incomplete ? "incomplete" : "needs_qualification";
			data["remote_writes"]          = false;
			data["publication_authorized"] = false;
			data["preview_scope"]          = "local_narrative";
			data["page"]                   = QString::number(cursor);
			data["next_cursor"]            = cursor + 1 < draft.Stops.size() + 2 ? QJsonValue{ QString::number(cursor + 1) } : QJsonValue{ QJsonValue::Null };
			data["complete"]               = cursor + 1 == draft.Stops.size() + 2;
			QString body, heading;
			if (cursor < 2) {
				heading = cursor ? "Solution" : "Problem";
				body    = text(cursor ? draft.Solution : draft.Problem);
			} else {
				const auto& stop = draft.Stops[cursor - 2];
				heading          = "Stop " + QString::number(stop.Id.value());
				body             = "### " + text(core::tour_markdown_label(stop.Title)) + "\n\n" + text(stop.Body);
				data["stop_id"]  = QString::number(stop.Id.value());
				data["ready"]    = stop.Preparation == core::tour_preparation::ready;
			}
			data["field"]    = heading;
			data["markdown"] = body;
			QString human    = "Local preview · revision " + QString::number(draft.Revision) + "\n" + heading + "\n" + body + "\n";
			human += "Publication operations require current qualification and a separate explicit approval.\n";
			if (cursor + 1 < draft.Stops.size() + 2) {
				human += "Next preview page: --cursor " + QString::number(cursor + 1) + "\n";
			}
			return presentation{ data, human, { } };
		}

		auto publication_state_name(application::staged_publication_state aState) -> QString
		{
			using state = application::staged_publication_state;
			switch (aState) {
			case state::ready:                 return "ready";
			case state::staged:                return "staged";
			case state::published:             return "published";
			case state::outcome_unknown:       return "outcome_unknown";
			case state::conflict:              return "conflict";
			case state::partial:               return "partial";
			case state::description_attention: return "description_attention";
			case state::unavailable:           return "unavailable";
			}
			return "unavailable";
		}
		auto render_publication(const tour_publication_preview& aPreview) -> tour_cli_result<presentation>
		{
			presentation result{ identities(aPreview.Snapshot), text(aPreview.Status) + "\n", { } };
			auto&        data                 = result.Data;
			data["publication_status"]        = text(aPreview.Status);
			data["publication_state"]         = publication_state_name(aPreview.Result.State);
			data["publication_authorized"]    = false;
			data["pending_server_candidates"] = QString::number(aPreview.PendingServerReviews);
			data["private_history"]           = "local only; unknown after profile loss";
			data["resume_required"]           = aPreview.Resume;
			data["plan_available"]            = aPreview.Plan.has_value();
			if (aPreview.Plan) {
				const auto markdown = tour_plan_markdown(*aPreview.Plan);
				if (markdown.size() > 400000) {
					return refuse("preview_too_large", "The exact plan exceeds the terminal preview bound. Reduce the publication before confirming.");
				}
				data["attempt_id"]       = text(aPreview.Plan->Submission.IdempotencyKey);
				data["markdown"]         = text(markdown);
				data["description_edit"] = aPreview.Plan->Description.has_value();
				data["compatibility"]    = aPreview.Plan->Compatibility;
				result.Human += "Attempt " + text(aPreview.Plan->Submission.IdempotencyKey) + "\n" + text(markdown) + "\n";
			}
			if (const auto& record = aPreview.Result.Record) {
				data["journal_revision"]     = QString::number(record->Revision);
				data["published"]            = record->Published;
				data["possibly_transmitted"] = record->PossiblyTransmitted;
				if (record->Review) {
					data["review_id"] = QString::number(record->Review->Id.value());
				}
				QJsonArray comments;
				for (const auto& comment : record->Comments) {
					comments.append(QJsonObject{ { "id", QString::number(comment.Id) }, { "url", text(comment.WebUrl) } });
				}
				data["confirmed_comments"] = comments;
				if (record->Index) {
					data["index_id"] = QString::number(record->Index->Id);
				}
			}
			using state = application::staged_publication_state;
			if (aPreview.Result.State == state::outcome_unknown) {
				result.Status = "uncertain";
				result.Exit   = 6;
			}
			if (aPreview.Result.State == state::partial || aPreview.Result.State == state::description_attention) {
				result.Status = "partial";
				result.Exit   = 6;
			}
			if (aPreview.Result.State == state::conflict) {
				result.Status = "conflict";
				result.Exit   = 4;
			}
			return result;
		}
		auto publication_command(const command_options& aOptions, const tour_local_snapshot& aSnapshot, std::unique_ptr<tour_runtime>& aRuntime, const QString& aBase, tour_reader_factory aReaders, bool aGh) -> tour_cli_result<presentation>
		{
			const auto token = tour_runtime::draft_token(aSnapshot.Target);
			if (aOptions.Command == "publish" && !aOptions.Interactive) {
				auto    quote   = [](QString aValue) { aValue.replace("'", "'\\''"); return "'" + aValue + "'"; };
				QString command = "bpr tour publish --data-root " + quote(aBase) + " --profile " + QString::number(aSnapshot.Target.Owner.Profile.value()) + " --draft " + text(token) + " --transport " + (aGh ? "gh" : "native");
				for (const auto& flag : QStringList{ "--description", "--compatibility" }) {
					if (aOptions.Values.contains(flag)) {
						command += " " + flag;
					}
				}
				if (aOptions.Values.contains("--resume")) {
					command += " --resume " + quote(aOptions.Values["--resume"]);
				}
				return refuse("interaction_required", "Run in a terminal: " + command.toStdString() + "\nJSON, readiness, stdin yes and plan files do not authorize writes.", 3);
			}
			const auto attempt  = aOptions.Values.value(aOptions.Command == "publish" ? "--resume" : "--attempt").toStdString();
			auto       prepared = aRuntime->prepare_publication(token, aGh, { aOptions.Values.contains("--compatibility"), aOptions.Values.contains("--description") }, attempt, aOptions.Command == "recover");
			if (!prepared) {
				return std::unexpected(prepared.error());
			}
			auto rendered = render_publication(*prepared);
			if (!rendered || aOptions.Command != "publish") {
				return rendered;
			}
			if (!prepared->Plan) {
				return refuse("publication_unavailable", prepared->Status, 5);
			}
			if (prepared->Resume && attempt.empty() && prepared->Result.State != application::staged_publication_state::published) {
				return refuse("resume_required", "Run publish --resume " + prepared->Plan->Submission.IdempotencyKey + " with this exact --profile and --draft to preview the captured attempt.", 3);
			}
			if (prepared->Result.State == application::staged_publication_state::outcome_unknown || prepared->Result.State == application::staged_publication_state::conflict || prepared->Result.State == application::staged_publication_state::published) {
				return rendered;
			}
			// A human prompt never retains the profile lease or a provider capability.
			aRuntime.reset();
			std::cout << wrapped_terminal_text(rendered->Human).toUtf8().constData();
			std::cout << "Type publish to authorize exactly this finite plan, or press Enter to cancel: " << std::flush;
			auto answer = read_tour_terminal_line();
			if (!answer || *answer != "publish") {
				return refuse("cancelled", "Publication cancelled; no writes were authorized.", 130);
			}
			aRuntime       = std::make_unique<tour_runtime>(native_filesystem_path(aBase), std::to_string(aSnapshot.Target.Owner.Profile.value()), std::move(aReaders));
			auto completed = aRuntime->publish(*prepared, aGh);
			if (!completed) {
				return std::unexpected(completed.error());
			}
			auto output = render_publication(*completed);
			if (output && completed->Result.State != application::staged_publication_state::published && output->Exit == 0) {
				output->Status = "partial";
				output->Exit   = 6;
			}
			return output;
		}
		auto walk(const command_options&, tour_reader_factory) -> tour_cli_result<presentation>;
		auto assist_command(const command_options&, tour_reader_factory) -> tour_cli_result<presentation>;

		auto perform(const command_options& aOptions, tour_reader_factory aReaders) -> tour_cli_result<presentation>
		{
			if (aOptions.Command == "walk") {
				return walk(aOptions, std::move(aReaders));
			}
			if (aOptions.Command == "hooks" || aOptions.Command == "assist" || aOptions.Command == "outline") {
				return assist_command(aOptions, std::move(aReaders));
			}
			if (aOptions.Command == "capabilities" || aOptions.Command == "help") {
				const char* const exportPolicy = tour_assist_policy() == tour_assist_export_policy::open
					? "open"
					: (tour_assist_policy() == tour_assist_export_policy::manual_only ? "manual_only" : "invalid");
				return presentation{ { { "cli_contract", "1" }, { "parent_tour_format", "1" }, { "local_draft_schema", "3" }, { "local_history", "atomic tour-state-v1; 16 entries and 8 MiB per direction" }, { "commands", QJsonArray::fromStringList(supported_commands()) }, { "max_field_bytes", QString::number(core::maximum_tour_field_bytes) }, { "max_frame_bytes", "1048576" }, { "gui_required", false }, { "ai_required", false }, { "remote_writes", false }, { "publication_gate", "production Tour host/transport qualification pending" }, { "terminal_color", "none" }, { "terminal_width", "COLUMNS 20..240; default 80" }, { "pager", "none; bounded pages and line-oriented walk" }, { "owner_protocol", "same-user local channel v6 (a named pipe on Windows) for local draft commands, assist and pinned source reads; owner_protocol_unsupported without one" }, { "assist", QJsonObject{ { "hooks", QJsonArray{ "problem", "solution", "outline", "comment" } }, { "hook_version", QString::fromUtf8(application::writing_hook_version.data(), static_cast<qsizetype>(application::writing_hook_version.size())) }, { "ticket_lifetime_seconds", "1800" }, { "max_result_bytes", "32768" }, { "max_sources", "8" }, { "model_launched_by_bpr", false }, { "verified_agent_identity", "unavailable" }, { "export_policy", exportPolicy } } }, { "approval_modes", QJsonArray{ "human-terminal-exact-plan" } }, { "native_device_signin", "github.com; admitted platform vault required" }, { "gh_transport", "explicit opt-in; qualified macOS trust boundary only" }, { "platform", platform_status() } },
									 "bpr tour — local review-tour terminal\nCLI contract: 1\nAvailable: capabilities, connect, open, status, show, edit, add, move, remove, ready, undo, redo, walk, preview, publish, recover, hooks, assist, outline\nNo GUI or AI provider required.\nPlatform: " + platform_status()["os"].toString() + ", " + platform_status()["qualification"].toString() + ". Native vault: " + platform_status()["native_vault"].toString() + ". gh transport: " + platform_status()["gh_transport"].toString() + ".\n",
									 { } };
			}
			const auto transport = aOptions.Values.value("--transport", "native");
			if (transport != "native" && transport != "gh") {
				return refuse("unsupported_transport", "Use native or gh.");
			}
			if (!aOptions.Interactive && aOptions.Command != "open" && aOptions.Command != "connect" && !aOptions.Values.contains("--profile")) {
				return refuse("target_ambiguous", "Machine draft commands require --profile and --draft.");
			}
			std::optional<pr_target> pr;
			if (aOptions.Command == "open") {
				auto resolved = resolve_pr(aOptions);
				if (!resolved) {
					return std::unexpected(resolved.error());
				}
				pr = std::move(*resolved);
			}
			if (aOptions.Command == "connect" && !aOptions.Values.contains("--host")) {
				return refuse("invalid_target", "Connect requires an explicit --host.");
			}
			const auto base = aOptions.Values.value("--data-root", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
			if (!QDir::isAbsolutePath(base)) {
				return refuse("invalid_profile_root", "--data-root must be an absolute private path.");
			}
			auto runtime = std::make_unique<tour_runtime>(native_filesystem_path(base), aOptions.Values.value("--profile").toStdString(), aReaders);
			if (const auto admission = runtime->admission(); !admission) {
				return std::unexpected(admission.error());
			}
			if (aOptions.Command == "connect") {
				auto result = runtime->connect(aOptions.Values["--host"].toStdString(), transport == "gh", aOptions.Interactive);
				if (!result) {
					return std::unexpected(result.error());
				}
				return presentation{ { { "profile_id", QString::number(runtime->admission()->value()) }, { "account_id", text(*result) }, { "host", aOptions.Values["--host"] }, { "transport", transport } }, "Connected account " + text(*result) + " on " + aOptions.Values["--host"] + ".\n", { } };
			}
			auto result = pr ? runtime->open(pr->Host, pr->Repository, pr->Number, transport == "gh", aOptions.Values.value("--root").toStdString(), aOptions.Values.contains("--offline")) : aOptions.Command == "status" && aOptions.Values.contains("--request-id") ? runtime->request_status(aOptions.Values.value("--draft").toStdString(), aOptions.RequestId.toStdString())
																																																																	   : runtime->load(aOptions.Values.value("--draft").toStdString(), !aOptions.Interactive);
			if (!result) {
				return std::unexpected(result.error());
			}
			if (aOptions.Command == "publish" || aOptions.Command == "recover" || (aOptions.Command == "preview" && aOptions.Values.contains("--refresh"))) {
				return publication_command(aOptions, *result, runtime, base, aReaders, transport == "gh");
			}
			if (aOptions.Command == "preview") {
				return preview(aOptions, *result);
			}
			const auto previousRevision = result->Draft.Revision;
			const auto transactionLabel = aOptions.Command == "undo" ? result->UndoLabel : aOptions.Command == "redo" ? result->RedoLabel
																													  : std::string{ };
			if ((aOptions.Command == "undo" || aOptions.Command == "redo") && transactionLabel.empty()) {
				return refuse("history_empty", "There is no retained local transaction in that direction.");
			}
			if (local_edit(aOptions.Command)) {
				auto command = edit_command(aOptions, result->Draft);
				if (!command) {
					return std::unexpected(command.error());
				}
				const auto                           token = tour_runtime::draft_token(result->Target);
				std::optional<tour_editor_candidate> editorCandidate;
				if (aOptions.Command == "add") {
					auto destination = source_destination(aOptions, result->Draft);
					if (!destination) {
						return std::unexpected(destination.error());
					}
					auto source = runtime->source(token, *destination, transport == "gh");
					if (!source) {
						return std::unexpected(source.error());
					}
					command->Value = { .Title = source->Source.Path, .Original = source->Source, .Mapping = core::tour_mapping::exact_original, .ReaffirmedHead = result->Draft.ObservedHead.value_or(result->Draft.Head) };
					if (aOptions.Values.contains("--file")) {
						runtime.reset();
						auto input = read_tour_text_input(aOptions.Values["--file"]);
						if (!input) {
							return std::unexpected(input.error());
						}
						command->Value.Body = std::move(*input);
						runtime             = std::make_unique<tour_runtime>(native_filesystem_path(base), aOptions.Values.value("--profile").toStdString(), aReaders);
					}
				}
				if (aOptions.Command == "edit") {
					const bool fromFile = aOptions.Values.contains("--file");
					if (!fromFile && !aOptions.Interactive) {
						return refuse("interaction_required", "Non-interactive edits require --file PATH or --file -.", 3);
					}
					std::string original;
					if (command->Kind == core::tour_command_kind::problem) {
						original = result->Draft.Problem;
					} else if (command->Kind == core::tour_command_kind::solution) {
						original = result->Draft.Solution;
					} else {
						original = aOptions.Positional[0].endsWith(":title") ? command->Value.Title : command->Value.Body;
					}
					runtime.reset();
					tour_cli_result<std::string> input;
					if (fromFile) {
						input = read_tour_text_input(aOptions.Values["--file"]);
					} else {
						auto edited = run_tour_editor(original);
						if (!edited) {
							return std::unexpected(edited.error());
						}
						editorCandidate = std::move(*edited);
						input           = editorCandidate->Text;
					}
					if (!input) {
						return std::unexpected(input.error());
					}
					if (command->Kind == core::tour_command_kind::edit_stop) {
						if (aOptions.Positional[0].endsWith(":title")) {
							command->Value.Title = std::move(*input);
						} else {
							command->Value.Body = std::move(*input);
						}
					} else {
						command->Text = std::move(*input);
					}
					runtime = std::make_unique<tour_runtime>(native_filesystem_path(base), aOptions.Values.value("--profile").toStdString(), aReaders);
				}
				result = runtime->mutate(token, { .Command = std::move(*command), .Operation = service_operation(aOptions.Command), .Origin = aOptions.Json ? application::tour_presentation_origin::command_client : application::tour_presentation_origin::terminal, .RequestId = aOptions.RequestId.toStdString() });
				if (!result) {
					auto error = result.error();
					if (editorCandidate) {
						error.Message += " Candidate retained at " + editorCandidate->Path.toStdString() + ". Reopen the current draft before importing that file.";
					}
					return std::unexpected(std::move(error));
				}
				if (editorCandidate) {
					cleanup_tour_editor(*editorCandidate);
				}
			}
			auto rendered = render_snapshot(aOptions, *result);
			if (rendered) {
				rendered->Data["changed"] = result->Draft.Revision != previousRevision;
				if (!transactionLabel.empty()) {
					rendered->Data["transaction_label"] = text(transactionLabel);
					rendered->Human += (aOptions.Command == "undo" ? "Undid " : "Redid ") + text(transactionLabel) + ".\n";
				}
			}
			if (rendered && aOptions.Command == "show" && aOptions.Values.contains("--source")) {
				const auto                                 parts = aOptions.Positional[0].split(':');
				const auto                                 id    = parts.size() >= 2 ? number(parts[1]) : std::nullopt;
				const auto                                 stop  = id ? std::ranges::find(result->Draft.Stops, core::tour_stop_id{ *id }, &core::tour_stop::Id) : result->Draft.Stops.end();
				std::optional<core::tour_code_destination> destination;
				if (stop != result->Draft.Stops.end()) {
					destination = stop_destination(*stop);
				}
				auto source = destination ? runtime->source(tour_runtime::draft_token(result->Target), *destination, transport == "gh") : tour_cli_result<tour_source_material>{ refuse("source_unavailable", "No pinned source destination belongs to this field.", 5) };
				if (!source && source.error().Exit == 130) {
					return std::unexpected(source.error());
				}
				rendered->Warnings.removeLast();
				if (source) {
					rendered->Data["source_availability"] = "available";
					rendered->Data["source_state"]        = "available";
					rendered->Data["source"]              = QJsonObject{ { "path", text(source->Source.Path) }, { "commit", text(core::canonical_hex(source->Source.Commit).view()) }, { "side", source->Source.First.Side == core::diff_side::left ? "LEFT" : "RIGHT" }, { "first_line", QString::number(source->Source.First.Line + 1) }, { "last_line", QString::number(source->Source.Last.Line + 1) }, { "text", text(source->Text) } };
					rendered->Human += render_source(*source);
				} else {
					rendered->Warnings.append(QJsonObject{ { "code", text(source.error().Code) }, { "message", text(source.error().Message) } });
				}
			}
			if (rendered && transport == "gh" && pr && !aOptions.Values.contains("--offline")) {
				rendered->Warnings.append(QJsonObject{ { "code", "gh_trust" }, { "message", "Explicit gh transport uses your configured gh executable and identity within the existing qualified process boundary." } });
			}
			return rendered;
		}
		auto walk(const command_options& aOptions, tour_reader_factory aReaders) -> tour_cli_result<presentation>
		{
			if (!aOptions.Interactive) {
				return refuse("interaction_required", "Run walk in a terminal; use status/show/ready with explicit revisions for command clients.", 3);
			}
			const auto base = aOptions.Values.value("--data-root", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
			if (!QDir::isAbsolutePath(base)) {
				return refuse("invalid_profile_root", "--data-root must be absolute.");
			}
			const auto transport = aOptions.Values.value("--transport", "native");
			if (transport != "native" && transport != "gh") {
				return refuse("unsupported_transport", "Use native or gh.");
			}
			std::size_t index   = 0;
			auto        options = aOptions;
			for (;;) {
				auto runtime  = std::make_unique<tour_runtime>(native_filesystem_path(base), options.Values.value("--profile").toStdString(), aReaders);
				auto snapshot = runtime->load(options.Values.value("--draft").toStdString(), false);
				if (!snapshot) {
					return std::unexpected(snapshot.error());
				}
				if (snapshot->Draft.Stops.empty()) {
					return presentation{ identities(*snapshot), "The tour has no stops. Use add with an exact source range.\n", { } };
				}
				index                   = std::min(index, snapshot->Draft.Stops.size() - 1);
				const auto& stop        = snapshot->Draft.Stops[index];
				const auto  range       = stop.Mapped ? stop.Mapped : stop.Original;
				const auto  destination = range ? std::optional{ core::destination_of(*range) } : stop.Destination;
				const auto  token       = tour_runtime::draft_token(snapshot->Target);
				auto        source      = destination ? runtime->source(token, *destination, transport == "gh") : tour_cli_result<tour_source_material>{ refuse("source_unavailable", "No pinned destination is available.", 5) };
				QString     page        = "Stop " + QString::number(index + 1) + "/" + QString::number(snapshot->Draft.Stops.size()) + " · ID " + QString::number(stop.Id.value()) + " · revision " + QString::number(snapshot->Draft.Revision) + "\n" + text(stop.Title) + "\nComment\n" + text(stop.Body) + "\n";
				if (source) {
					page += render_source(*source);
				} else {
					page += "Source unavailable: " + text(source.error().Message) + "\n";
				}
				if (stop.Preparation == core::tour_preparation::needs_attention) {
					page += "This stop needs attention before completion.\n";
				}
				options.Values["--profile"] = QString::number(runtime->admission()->value());
				options.Values["--draft"]   = text(token);
				runtime.reset();
				std::cout << wrapped_terminal_text(page).toUtf8().constData()
						  << wrapped_terminal_text("[e] Edit comment\n[t] Edit title\n[r] Ready and next\n[n] Next  [p] Previous\n[m before ID] Move\n[m after ID] Move\n[d] Remove  [q] Quit\n> ").toUtf8().constData() << std::flush;
				auto input = read_tour_terminal_line();
				if (!input) {
					return std::unexpected(input.error());
				}
				const auto action = text(*input).trimmed();
				if (action == "q") {
					return presentation{ identities(*snapshot), "Walk ended. Changes remain saved locally.\n", { } };
				}
				if (action == "p") {
					if (index) {
						--index;
					}
					continue;
				}
				if (action == "n") {
					index = std::min(index + 1, snapshot->Draft.Stops.size() - 1);
					continue;
				}
				auto command      = options;
				command.RequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
				command.Values.remove("--transport");
				command.Values["--expected-revision"] = QString::number(snapshot->Draft.Revision);
				command.Positional                    = { QString::number(stop.Id.value()) };
				if (action == "e" || action == "t") {
					command.Command    = "edit";
					command.Positional = { "stop:" + QString::number(stop.Id.value()) + (action == "t" ? ":title" : ":body") };
				} else if (action == "d") {
					command.Command = "remove";
				} else if (action == "r") {
					command.Command       = "ready";
					command.WalkAttention = core::tour_attention{ .EditorVisible = true, .Source = source ? std::optional{ source->Source } : std::nullopt, .SourceUnavailable = !source };
				} else {
					const auto parts = action.split(' ', Qt::SkipEmptyParts);
					if (parts.size() != 3 || parts[0] != "m" || (parts[1] != "before" && parts[1] != "after") || !number(parts[2])) {
						std::cerr << "Choose one of the displayed actions.\n";
						continue;
					}
					command.Command                                               = "move";
					command.Values[parts[1] == "before" ? "--before" : "--after"] = parts[2];
				}
				auto changed = perform(command, aReaders);
				if (!changed) {
					std::cerr << neutral_terminal_text(text(changed.error().Message)).toUtf8().constData() << '\n';
					if (changed.error().Exit == 130) {
						return changed;
					}
					continue;
				}
				std::cout << "Saved locally.\n";
				if (action == "r") {
					if (index + 1 == snapshot->Draft.Stops.size()) {
						return presentation{ changed->Data, "Walk complete. Preview the local tour before publication.\n", { } };
					}
					++index;
				}
			}
		}

		auto assist_json(const QString& aJson) -> QJsonObject { return QJsonDocument::fromJson(aJson.toUtf8()).object(); }
		auto parse_byte_range(const QString& aValue) -> std::optional<std::pair<std::uint64_t, std::uint64_t>>
		{
			const auto parts = aValue.split(':');
			if (parts.size() != 2) {
				return std::nullopt;
			}
			const auto zeroOk = [](const QString& aText) -> std::optional<std::uint64_t> {
				bool       ok    = false;
				const auto value = aText.toULongLong(&ok);
				return ok && aText == QString::number(value) ? std::optional{ value } : std::nullopt;
			};
			const auto begin = zeroOk(parts[0]), end = zeroOk(parts[1]);
			return begin && end ? std::optional{ std::pair{ *begin, *end } } : std::nullopt;
		}
		auto hooks_command(const command_options& aOptions, std::unique_ptr<tour_runtime>& aRuntime, const QString& aBase, tour_reader_factory aReaders) -> tour_cli_result<presentation>
		{
			const auto                        sub = aOptions.Positional[0];
			std::optional<core::writing_hook> hook;
			if (aOptions.Positional.size() == 2) {
				hook = parse_tour_hook(aOptions.Positional[1].toStdString());
				if (!hook) {
					return refuse("invalid_hook", "Use problem, solution, outline or comment.");
				}
			}
			auto shown = aRuntime->hooks_show();
			if (!shown) {
				return std::unexpected(shown.error());
			}
			std::string effective;
			if (hook) {
				for (const auto& entry : assist_json(text(*shown))["hooks"].toArray()) {
					if (entry.toObject()["name"].toString().toStdString() == tour_hook_name(*hook)) {
						effective = entry.toObject()["instructions"].toString().toStdString();
					}
				}
			}
			if (sub == "edit" || sub == "reset") {
				std::string instructions;
				if (sub == "edit") {
					std::optional<tour_editor_candidate> candidate;
					tour_cli_result<std::string>         input;
					aRuntime.reset();
					if (aOptions.Values.contains("--file")) {
						input = read_tour_text_input(aOptions.Values["--file"]);
					} else if (!aOptions.Interactive) {
						return refuse("interaction_required", "Non-interactive hook edits require --file PATH or --file -.", 3);
					} else {
						auto edited = run_tour_editor(effective);
						if (!edited) {
							return std::unexpected(edited.error());
						}
						candidate = std::move(*edited);
						input     = candidate->Text;
					}
					if (!input) {
						return std::unexpected(input.error());
					}
					instructions = std::move(*input);
					aRuntime     = std::make_unique<tour_runtime>(native_filesystem_path(aBase), aOptions.Values.value("--profile").toStdString(), aReaders);
					if (candidate) {
						cleanup_tour_editor(*candidate);
					}
				}
				shown = aRuntime->hooks_store(*hook, std::move(instructions));
				if (!shown) {
					return std::unexpected(shown.error());
				}
			}
			const auto   document = assist_json(text(*shown));
			presentation result{ QJsonObject{ { "hooks", document["hooks"] }, { "hooks_revision", document["revision"] }, { "hook_version", document["version"] } }, { }, { } };
			for (const auto& entry : document["hooks"].toArray()) {
				const auto object = entry.toObject();
				if (!hook || object["name"].toString().toStdString() == tour_hook_name(*hook)) {
					result.Human += object["name"].toString() + (object["customized"].toBool() ? " (customized)\n" : " (shipped)\n");
					if (hook) {
						result.Human += object["instructions"].toString() + "\n";
					}
				}
			}
			return result;
		}
		auto assist_command(const command_options& aOptions, tour_reader_factory aReaders) -> tour_cli_result<presentation>
		{
			if (!aOptions.Interactive && !aOptions.Values.contains("--profile")) {
				return refuse("target_ambiguous", "Machine commands require --profile and --draft.");
			}
			const auto transport = aOptions.Values.value("--transport", "native");
			if (transport != "native" && transport != "gh") {
				return refuse("unsupported_transport", "Use native or gh.");
			}
			const auto base = aOptions.Values.value("--data-root", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
			if (!QDir::isAbsolutePath(base)) {
				return refuse("invalid_profile_root", "--data-root must be an absolute private path.");
			}
			auto runtime = std::make_unique<tour_runtime>(native_filesystem_path(base), aOptions.Values.value("--profile").toStdString(), aReaders);
			if (const auto admission = runtime->admission(); !admission) {
				return std::unexpected(admission.error());
			}
			if (aOptions.Command == "hooks") {
				return hooks_command(aOptions, runtime, base, aReaders);
			}
			const auto sub = aOptions.Command == "assist" ? aOptions.Positional[0] : QString{ "use" };
			if (sub != "cancel" && tour_assist_policy() != tour_assist_export_policy::open) {
				return refuse("context_export_refused", "This host does not allow draft text to be exported to an agent it cannot verify. Manual authoring is unaffected: edit, add, move, remove, ready and preview still work.", 5);
			}
			const auto draftId = aOptions.Values.value("--draft").toStdString();
			auto       loaded  = runtime->load(draftId, !aOptions.Interactive);
			if (!loaded) {
				return std::unexpected(loaded.error());
			}
			const auto token  = tour_runtime::draft_token(loaded->Target);
			const auto origin = aOptions.Json ? application::tour_presentation_origin::command_client : application::tour_presentation_origin::terminal;
			const auto finish = [&](const tour_local_snapshot& aSnapshot, QString aHuman) -> tour_cli_result<presentation> {
				auto rendered = render_snapshot(aOptions, aSnapshot);
				if (!rendered) {
					return rendered;
				}
				rendered->Data["changed"] = aSnapshot.Draft.Revision != loaded->Draft.Revision;
				if (!aSnapshot.AssistJson.empty()) {
					rendered->Data["assist"] = assist_json(text(aSnapshot.AssistJson));
				}
				rendered->Human += aHuman;
				return rendered;
			};
			if (sub == "cancel") {
				auto cancelled = runtime->assist_cancel(token);
				if (!cancelled) {
					return std::unexpected(cancelled.error());
				}
				return finish(*cancelled, "Pending requests and proposals were closed.\n");
			}
			if (sub == "context") {
				const auto hook = parse_tour_hook(aOptions.Values.value("--hook").toStdString());
				if (!hook) {
					return refuse("invalid_hook", "Use --hook problem, solution, outline or comment.");
				}
				tour_assist_query query{ .Hook = *hook, .Session = aOptions.Values.value("--session", "cli").toStdString() };
				const auto        field = aOptions.Values.value("--field");
				if (*hook == core::writing_hook::outline) {
					if (!field.isEmpty() || aOptions.Values.contains("--range-bytes") || aOptions.Values.contains("--quote")) {
						return refuse("invalid_input", "An outline is for the whole draft: omit --field, --range-bytes and --quote.");
					}
				} else if (*hook == core::writing_hook::comment) {
					const auto parts = field.split(':');
					const auto id    = parts.size() == 2 && parts[0] == "stop" ? number(parts[1]) : std::nullopt;
					if (!id) {
						return refuse("invalid_field", "The comment hook writes one stop's body: use --field stop:ID with a stable stop ID.");
					}
					query.Stop = core::tour_stop_id{ *id };
				} else if (field != QString::fromStdString(std::string{ tour_hook_name(*hook) })) {
					return refuse("invalid_field", "Use --field problem with the problem hook and --field solution with the solution hook.");
				}
				if (aOptions.Values.contains("--range-bytes")) {
					query.Range = parse_byte_range(aOptions.Values["--range-bytes"]);
					if (!query.Range) {
						return refuse("invalid_range", "Use --range-bytes BEGIN:END, half-open UTF-8 byte offsets.");
					}
				}
				if (aOptions.Values.contains("--quote")) {
					query.Quote = aOptions.Values["--quote"].toStdString();
				}
				std::vector<application::writing_source> extra;
				const bool                               gh = transport == "gh";
				if (aOptions.Values.contains("--sources")) {
					if (*hook != core::writing_hook::outline) {
						return refuse("invalid_input", "--sources gives an outline its code destinations.");
					}
					if (runtime->forwarding()) {
						// The owner's assist request carries no source evidence, so none is read for it.
						return refuse("interaction_required", "A running desktop owner does not take source evidence for assist. Ask without --sources, or close the desktop.", 3);
					}
					const auto specs = aOptions.Values["--sources"].split(',', Qt::SkipEmptyParts);
					if (specs.isEmpty() || specs.size() > 8) {
						return refuse("invalid_input", "Give one to eight PATH:START-END sources.");
					}
					int index = 0;
					for (const auto& spec : specs) {
						command_options probe = aOptions;
						probe.Positional      = { spec };
						probe.Values.remove("--path");
						probe.Values.remove("--lines");
						const auto destination = source_destination(probe, loaded->Draft);
						if (!destination) {
							return std::unexpected(destination.error());
						}
						runtime.reset();
						runtime     = std::make_unique<tour_runtime>(native_filesystem_path(base), aOptions.Values.value("--profile").toStdString(), aReaders);
						auto source = runtime->source(token, *destination, gh);
						if (!source) {
							return std::unexpected(source.error());
						}
						extra.push_back({ "source-" + std::to_string(++index), source->Text, source->Source });
					}
				}
				if (*hook == core::writing_hook::comment && !runtime->forwarding()) {
					const auto stop = std::ranges::find(loaded->Draft.Stops, query.Stop, &core::tour_stop::Id);
					if (stop != loaded->Draft.Stops.end()) {
						if (const auto destination = stop_destination(*stop)) {
							if (auto source = runtime->source(token, *destination, gh)) {
								extra.push_back({ "stop-source", source->Text, source->Source });
							}
						}
					}
				}
				auto issued = runtime->assist_issue(token, query, std::move(extra));
				if (!issued) {
					return std::unexpected(issued.error());
				}
				const auto ticket = assist_json(text(issued->AssistJson))["ticket"].toString();
				return finish(*issued, "Ticket " + ticket + ". Answer it with assist complete; nothing changes until you do.\n");
			}
			if (sub == "complete") {
				const auto ticket = number(aOptions.Values.value("--ticket"));
				if (!ticket || !aOptions.Values.contains("--result")) {
					return refuse("invalid_input", "Use assist complete --ticket ID --result PATH|-.");
				}
				runtime.reset();
				auto input = read_tour_text_input(aOptions.Values["--result"]);
				if (!input) {
					return std::unexpected(input.error());
				}
				if (input->size() > 32768) {
					return refuse("input_too_large", "The result is larger than 32768 bytes.");
				}
				runtime = std::make_unique<tour_runtime>(native_filesystem_path(base), aOptions.Values.value("--profile").toStdString(), aReaders);
				application::tour_service_request request{ .Operation = application::tour_service_operation::assist_complete, .Origin = origin, .RequestId = aOptions.RequestId.toStdString(), .Assist = { .Id = *ticket, .ResultJson = std::move(*input) } };
				auto                              done = runtime->mutate(token, std::move(request));
				if (!done) {
					return std::unexpected(done.error());
				}
				const auto object = assist_json(text(done->AssistJson));
				QString    human;
				if (object["applied"].toBool()) {
					human = "Updated the captured text. Undo restores it. Nothing published.\n";
				} else if (object.contains("proposal")) {
					human = "Outline proposal " + object["proposal"].toObject()["id"].toString() + ". Choose suggestions with outline use.\n";
				} else {
					human = object["reason"].toString() + ": " + object["detail"].toString() + "\n";
				}
				return finish(*done, human);
			}
			// outline use PROPOSAL --indices LIST [--after STOP]
			const auto proposal = number(aOptions.Positional[1]);
			if (!proposal || !aOptions.Values.contains("--indices")) {
				return refuse("invalid_input", "Use outline use PROPOSAL --indices 0,2 [--after STOP].");
			}
			std::vector<std::size_t> indices;
			for (const auto& part : aOptions.Values["--indices"].split(',')) {
				bool       ok    = false;
				const auto value = part.toULongLong(&ok);
				if (!ok || part != QString::number(value) || value > 23) {
					return refuse("invalid_indices", "Indices are zero-based suggestion numbers, e.g. 0,2.");
				}
				indices.push_back(static_cast<std::size_t>(value));
			}
			auto position = loaded->Draft.Stops.size();
			if (aOptions.Values.contains("--after")) {
				const auto anchor = number(aOptions.Values["--after"]);
				const auto found  = anchor ? std::ranges::find(loaded->Draft.Stops, core::tour_stop_id{ *anchor }, &core::tour_stop::Id) : loaded->Draft.Stops.end();
				if (found == loaded->Draft.Stops.end()) {
					return refuse("unknown_stop", "--after names no stop with that stable ID.");
				}
				position = static_cast<std::size_t>(found - loaded->Draft.Stops.begin()) + 1U;
			}
			application::tour_service_request request{ .Operation = application::tour_service_operation::outline_use, .Origin = origin, .RequestId = aOptions.RequestId.toStdString(), .Assist = { .Id = *proposal, .Indices = std::move(indices), .Position = position } };
			auto                              inserted = runtime->mutate(token, std::move(request));
			if (!inserted) {
				return std::unexpected(inserted.error());
			}
			return finish(*inserted, "Inserted the chosen stops as Suggested. Each still needs your own comment and Ready.\n");
		}

	}
	auto run_tour_cli(const QStringList& aArguments, tour_reader_factory aReaders) -> int
	{
		tour_signal_scope signalScope;
		auto              options = parse(aArguments);
		const bool        json    = aArguments.contains("--json");
		const auto        command = options ? options->Command : QStringLiteral("invalid");
		auto              result  = [&]() -> tour_cli_result<presentation> {
			try {
				return options ? perform(*options, std::move(aReaders)) : tour_cli_result<presentation>{ std::unexpected(options.error()) };
			} catch (...) {
				return command == "publish" ? refuse("outcome_unknown", "Publication stopped without a completion claim. Recover the exact draft before another write.", 6) : refuse("operation_failed", "The operation failed; no completion is claimed. Reopen the exact draft to inspect retained state.", 7);
			}
		}();
		if (json) {
			QJsonValue error{ QJsonValue::Null };
			if (!result) {
				error = QJsonObject{ { "code", text(result.error().Code) }, { "message", text(result.error().Message) }, { "recoverable", true } };
			}
			const QJsonObject envelope{ { "schema", "bpr.tour.cli-result.v1" }, { "command", command }, { "request_id", options ? options->RequestId : QString{ } }, { "status", result ? result->Status : (result.error().Exit == 4 ? "conflict" : "error") }, { "data", result ? result->Data : QJsonObject{ } }, { "warnings", result ? result->Warnings : QJsonArray{ } }, { "error", error } };
			std::cout << QJsonDocument{ envelope }.toJson(QJsonDocument::Compact).constData() << '\n';
		} else if (result) {
			std::cout << wrapped_terminal_text(result->Human).toUtf8().constData();
			for (const auto& warning : result->Warnings) {
				std::cerr << wrapped_terminal_text(warning.toObject()["message"].toString()).toUtf8().constData() << '\n';
			}
		} else {
			std::cerr << neutral_terminal_text(text(result.error().Message)).toUtf8().constData() << '\n';
		}
		return result ? result->Exit : result.error().Exit;
	}
}
