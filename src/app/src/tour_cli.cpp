#include "tour_cli.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/github_host_authority.hpp"
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
#if defined(_WIN32)
	#include <io.h>
#else
	#include <unistd.h>
#endif
namespace bpr::app {
	namespace {
		auto text(std::string_view aValue) -> QString { return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size())); }
		auto safe_terminal(const QString& aText) -> QString
		{
			QString result;
			for (const auto value : aText.toUcs4()) {
				if ((value < 32 && value != 10 && value != 9) || (value >= 127 && value <= 159) || (value >= 0x202a && value <= 0x202e) || (value >= 0x2066 && value <= 0x2069)) {
					result += QStringLiteral("\\u%1").arg(value, 4, 16, QChar{ '0' });
				} else {
					const char32_t scalar = value;
					result += QString::fromUcs4(&scalar, 1);
				}
			}
			return result;
		}
		auto terminal() -> bool
		{
#if defined(_WIN32)
			return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
#else
			return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
		}
		struct command_options
		{
			QString                Command;
			QStringList            Positional;
			QMap<QString, QString> Values;
			bool                   Json = false, Interactive = false;
		};
		auto refuse(std::string aCode, std::string aMessage, int aExit = 2) { return std::unexpected(tour_cli_error{ std::move(aCode), std::move(aMessage), aExit }); }
		auto parse(const QStringList& aArguments) -> tour_cli_result<command_options>
		{
			command_options result;
			if (aArguments.size() > 80) {
				return refuse("input_too_large", "Too many command arguments.");
			}
			const QStringList flags{ "--json", "--non-interactive", "--no-color", "--offline", "--source" };
			const QStringList options{ "--profile", "--draft", "--root", "--repo", "--host", "--transport", "--data-root", "--schema-version", "--cursor" };
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
			result.Interactive = terminal() && !result.Json && !result.Values.contains("--non-interactive");
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
			result.Command    = words[1];
			result.Positional = words.mid(2);
			const QStringList commands{ "capabilities", "connect", "open", "status", "show" };
			if (!commands.contains(result.Command)) {
				return refuse("unsupported_command", "Use bpr tour capabilities for implemented commands.");
			}
			const auto count = result.Positional.size();
			if (((result.Command == "open" || result.Command == "show") && count != 1) || ((result.Command == "connect" || result.Command == "status" || result.Command == "capabilities") && count != 0)) {
				return refuse("invalid_input", "Unexpected positional arguments.");
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
			if (result.Command == "status" || result.Command == "show") {
				allowed.append("--draft");
			}
			if (result.Command == "status") {
				allowed.append("--cursor");
			}
			if (result.Command == "show") {
				allowed.append("--source");
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
		auto identities(const tour_local_snapshot& aSnapshot) -> QJsonObject
		{
			const auto& target = aSnapshot.Target;
			const auto& owner  = target.Owner;
			return { { "profile_id", QString::number(owner.Profile.value()) }, { "draft_id", text(tour_runtime::draft_token(target)) }, { "account_id", QString::number(owner.Account.value()) }, { "repository_id", QString::number(owner.Repository.value()) }, { "pull_request_id", QString::number(owner.PullRequest.value()) }, { "host", text(target.Hostname) }, { "repository", text(target.Repository.Owner + "/" + target.Repository.Name) }, { "number", QString::number(target.Number.value()) }, { "revision", QString::number(aSnapshot.Draft.Revision) }, { "head", text(core::canonical_hex(aSnapshot.Draft.Head).view()) }, { "local_state", "Saved locally" }, { "published", aSnapshot.Draft.Remote.has_value() }, { "read_only", aSnapshot.Draft.ReadOnly }, { "remote_stale", aSnapshot.Draft.RemoteStale }, { "conflict_count", QString::number(aSnapshot.Draft.Conflicts.size()) }, { "source_state", "not_loaded" } };
		}
		struct presentation
		{
			QJsonObject Data;
			QString     Human;
			QJsonArray  Warnings;
		};
		auto render_snapshot(const command_options& aOptions, const tour_local_snapshot& aSnapshot) -> tour_cli_result<presentation>
		{
			const auto&  draft = aSnapshot.Draft;
			presentation result{ identities(aSnapshot), { }, { } };
			result.Human = QStringLiteral("%1/%2/%3#%4\nDraft %5 · revision %6\nSaved locally; local changes are not published.\n").arg(text(aSnapshot.Target.Hostname), text(aSnapshot.Target.Repository.Owner), text(aSnapshot.Target.Repository.Name), QString::number(aSnapshot.Target.Number.value()), text(tour_runtime::draft_token(aSnapshot.Target)), QString::number(draft.Revision));
			if (aOptions.Values.contains("--offline")) {
				result.Warnings.append(QJsonObject{ { "code", "stale_cached" }, { "message", "Local state only; published material and source have not been refreshed." } });
			}
			if (!draft.ImportNotice.empty()) {
				result.Warnings.append(QJsonObject{ { "code", "import_notice" }, { "message", text(draft.ImportNotice) } });
			}
			if (aOptions.Command == "show") {
				const auto field = aOptions.Positional[0];
				QString    value;
				if (field == "problem") {
					value = text(draft.Problem);
				} else if (field == "solution") {
					value = text(draft.Solution);
				} else if (field.startsWith("stop:") && number(field.mid(5))) {
					const auto stop = std::ranges::find(draft.Stops, core::tour_stop_id{ *number(field.mid(5)) }, &core::tour_stop::Id);
					if (stop == draft.Stops.end()) {
						return refuse("unknown_stop", "No stop has that stable ID.");
					}
					value                  = text(stop->Body);
					result.Data["title"]   = text(stop->Title);
					result.Data["stop_id"] = QString::number(stop->Id.value());
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
					stops.append(QJsonObject{ { "stop_id", QString::number(stop.Id.value()) }, { "title", text(stop.Title) }, { "ready", core::ready(stop) } });
					result.Human += QStringLiteral("%1. %2 [%3]\n").arg(QString::number(stop.Id.value()), text(stop.Title), core::ready(stop) ? "Ready" : "Editing");
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
		auto perform(const command_options& aOptions, tour_reader_factory aReaders) -> tour_cli_result<presentation>
		{
			if (aOptions.Command == "capabilities" || aOptions.Command == "help") {
				return presentation{ { { "cli_contract", "1" }, { "parent_tour_format", "1" }, { "local_draft_schema", "2" }, { "commands", QJsonArray{ "capabilities", "connect", "open", "status", "show" } }, { "max_field_bytes", QString::number(core::maximum_tour_field_bytes) }, { "max_frame_bytes", "1048576" }, { "gui_required", false }, { "ai_required", false }, { "remote_writes", false }, { "owner_protocol", "standalone; busy refusal for another owner" }, { "approval_modes", QJsonArray{ } }, { "native_device_signin", "github.com; admitted platform vault required" }, { "gh_transport", "explicit opt-in; qualified macOS trust boundary only" } }, "bpr tour — local review-tour terminal\nCLI contract: 1\nAvailable: capabilities, connect, open, status, show\nNo GUI or AI provider required.\n", { } };
			}
			const auto transport = aOptions.Values.value("--transport", "native");
			if (transport != "native" && transport != "gh") {
				return refuse("unsupported_transport", "Use native or gh.");
			}
			if (!aOptions.Interactive && (aOptions.Command == "status" || aOptions.Command == "show") && !aOptions.Values.contains("--profile")) {
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
			tour_runtime runtime{ native_filesystem_path(base), aOptions.Values.value("--profile").toStdString(), std::move(aReaders) };
			if (const auto admission = runtime.admission(); !admission) {
				return std::unexpected(admission.error());
			}
			if (aOptions.Command == "connect") {
				auto result = runtime.connect(aOptions.Values["--host"].toStdString(), transport == "gh", aOptions.Interactive);
				if (!result) {
					return std::unexpected(result.error());
				}
				return presentation{ { { "profile_id", QString::number(runtime.admission()->value()) }, { "account_id", text(*result) }, { "host", aOptions.Values["--host"] }, { "transport", transport } }, "Connected account " + text(*result) + " on " + aOptions.Values["--host"] + ".\n", { } };
			}
			auto result = pr ? runtime.open(pr->Host, pr->Repository, pr->Number, transport == "gh", aOptions.Values.value("--root").toStdString(), aOptions.Values.contains("--offline")) : runtime.load(aOptions.Values.value("--draft").toStdString(), aOptions.Json || aOptions.Values.contains("--non-interactive"));
			if (!result) {
				return std::unexpected(result.error());
			}
			auto rendered = render_snapshot(aOptions, *result);
			if (rendered && transport == "gh" && pr && !aOptions.Values.contains("--offline")) {
				rendered->Warnings.append(QJsonObject{ { "code", "gh_trust" }, { "message", "Explicit gh transport uses your configured gh executable and identity within the existing qualified process boundary." } });
			}
			return rendered;
		}
	}
	auto run_tour_cli(const QStringList& aArguments, tour_reader_factory aReaders) -> int
	{
		auto       options = parse(aArguments);
		const bool json    = aArguments.contains("--json");
		const auto command = options ? options->Command : QStringLiteral("invalid");
		auto       result  = [&]() -> tour_cli_result<presentation> {
			try {
				return options ? perform(*options, std::move(aReaders)) : tour_cli_result<presentation>{ std::unexpected(options.error()) };
			} catch (...) {
				return refuse("operation_failed", "The operation failed; no completion is claimed. Reopen the exact draft to inspect retained state.", 7);
			}
		}();
		if (json) {
			QJsonValue error{ QJsonValue::Null };
			if (!result) {
				error = QJsonObject{ { "code", text(result.error().Code) }, { "message", text(result.error().Message) }, { "recoverable", true } };
			}
			const QJsonObject envelope{ { "schema", "bpr.tour.cli-result.v1" }, { "command", command }, { "request_id", QUuid::createUuid().toString(QUuid::WithoutBraces) }, { "status", result ? "ok" : (result.error().Exit == 4 ? "conflict" : "error") }, { "data", result ? result->Data : QJsonObject{ } }, { "warnings", result ? result->Warnings : QJsonArray{ } }, { "error", error } };
			std::cout << QJsonDocument{ envelope }.toJson(QJsonDocument::Compact).constData() << '\n';
		} else if (result) {
			std::cout << safe_terminal(result->Human).toUtf8().constData();
			for (const auto& warning : result->Warnings) {
				std::cerr << safe_terminal(warning.toObject()["message"].toString()).toUtf8().constData() << '\n';
			}
		} else {
			std::cerr << safe_terminal(text(result.error().Message)).toUtf8().constData() << '\n';
		}
		return result ? 0 : result.error().Exit;
	}
}
