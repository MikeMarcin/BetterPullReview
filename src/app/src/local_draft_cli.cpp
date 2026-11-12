#include "local_draft_cli.hpp"

#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "local_draft_handoff.hpp"
#include "profile_admission.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>

namespace bpr::app {
	namespace {
		constexpr auto result_schema = "bpr.draft.cli-result.v1";

		auto utf8(const QString& aValue) -> std::string
		{
			const auto bytes = aValue.toUtf8();
			return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		}

		auto print(QJsonObject aResult, const bool aJson) -> int
		{
			const auto ok = aResult.value("ok").toBool();
			aResult.insert("schema", QString::fromLatin1(result_schema));
			if (aJson) {
				std::cout << QJsonDocument{ aResult }.toJson(QJsonDocument::Compact).toStdString() << '\n';
			} else if (ok) {
				const auto data = aResult.value("data").toObject();
				std::cout << data.value("outcome").toString().toStdString() << " local draft " << data.value("draft").toString().toStdString() << ": "
						  << data.value("title").toString().toStdString() << " (" << data.value("branch").toString().toStdString() << " into "
						  << data.value("target").toString().toStdString() << ")\n";
				if (const auto evidence = data.value("evidence").toObject(); !evidence.isEmpty()) {
					std::cout << "imported evidence: " << evidence.value("claims").toInteger() << " claims, " << evidence.value("checks").toInteger() << " checks, "
							  << evidence.value("results").toInteger() << " reported results (" << evidence.value("bound").toInteger() << " bound to a candidate), "
							  << evidence.value("questions").toInteger() << " questions\n";
				}
			} else {
				std::cerr << aResult.value("message").toString().toStdString() << '\n';
			}
			return ok ? 0 : 2;
		}

		auto refuse(const char* aCode, const QString& aMessage, const bool aJson) -> int
		{
			return print(QJsonObject{ { "ok", false }, { "code", QString::fromLatin1(aCode) }, { "message", aMessage } }, aJson);
		}

		auto capabilities() -> int
		{
			std::fputs("bpr draft open [PATH] [--context TEXT] [--evidence FILE] [--profile NAME] [--data-root PATH] [--json]\n"
					   "  Opens or resumes the local PR draft of the Git worktree at PATH (default: the current folder),\n"
					   "  as Home's Prepare Pull Request... does. --context adds a note. --evidence imports one\n"
					   "  bpr.evidence-import document; its results arrive reported. Nothing is published or run.\n"
					   "  While the desktop owns the profile, it shows the draft.\n",
					   stdout);
			return 0;
		}
	}

	auto run_draft_cli(const QStringList& aArguments) -> int
	{
		if (aArguments.isEmpty() || aArguments.front() == QStringLiteral("capabilities") || aArguments.front() == QStringLiteral("help")) {
			return capabilities();
		}
		auto json     = false;
		auto context  = std::optional<std::string>{ };
		auto evidence = std::optional<std::string>{ };
		auto profile  = std::string{ };
		auto base     = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
		auto location = std::optional<QString>{ };
		if (aArguments.front() != QStringLiteral("open")) {
			return refuse("unsupported_command", QStringLiteral("bpr draft knows open and capabilities."), aArguments.contains(QStringLiteral("--json")));
		}
		for (qsizetype index = 1; index < aArguments.size(); ++index) {
			const auto& argument = aArguments[index];
			const auto  value    = [&]() -> std::optional<QString> { return index + 1 < aArguments.size() ? std::optional{ aArguments[++index] } : std::nullopt; };
			if (argument == QStringLiteral("--json")) {
				json = true;
			} else if (argument == QStringLiteral("--context")) {
				const auto text = value();
				if (!text || text->toUtf8().size() > 16 * 1024) {
					return refuse("invalid_input", QStringLiteral("--context takes text of at most 16 KiB."), json);
				}
				context = utf8(*text);
			} else if (argument == QStringLiteral("--evidence")) {
				const auto file = value();
				if (!file || file->isEmpty()) {
					return refuse("invalid_input", QStringLiteral("--evidence takes the path of an evidence document."), json);
				}
				evidence = utf8(QDir::cleanPath(QDir::current().absoluteFilePath(*file)));
			} else if (argument == QStringLiteral("--profile")) {
				const auto text = value();
				if (!text) {
					return refuse("invalid_input", QStringLiteral("--profile takes a name."), json);
				}
				profile = utf8(*text);
			} else if (argument == QStringLiteral("--data-root")) {
				const auto text = value();
				if (!text || !QDir::isAbsolutePath(*text)) {
					return refuse("invalid_input", QStringLiteral("--data-root must be an absolute private path."), json);
				}
				base = *text;
			} else if (!argument.startsWith(QStringLiteral("--")) && !location) {
				location = argument;
			} else {
				return refuse("unsupported_option", QStringLiteral("Unknown option: %1.").arg(argument.left(64)), json);
			}
		}
		// The worktree is named explicitly, made absolute here: never a
		// last-used folder.
		const auto chosen = utf8(QDir::cleanPath(QDir::current().absoluteFilePath(location.value_or(QStringLiteral(".")))));
		for (int attempt = 0; attempt < 5; ++attempt) {
			auto admission = admit_profile(native_filesystem_path(base), profile);
			if (!admission) {
				return refuse("profile_unavailable", QString::fromStdString(admission.error().Message), json);
			}
			if (admission->Lease) {
				const auto git = native_filesystem_path(QFileInfo{ QStandardPaths::findExecutable(QStringLiteral("git")) }.canonicalFilePath());
				return print(open_local_draft_here(admission->Root, git, std::nullopt, chosen, context, evidence).Result, json);
			}
			const auto payload = adapters::sqlite::encode_tour_request(
				{ .Operation = application::tour_owner_operation::local_draft, .LocalDraft = { .Location = chosen, .Context = context, .Evidence = evidence } });
			if (payload.empty()) {
				return refuse("over_limit", QStringLiteral("This request is larger than the owner's channel carries; nothing was sent."), json);
			}
			auto answer = adapters::sqlite::send_tour_owner(admission->Root, admission->Profile, payload, { }, std::chrono::seconds{ 120 });
			if (!answer) {
				if (answer.error() == application::tour_channel_failure::unavailable) {
					continue;
				}
				return refuse("uncertain", QStringLiteral("The desktop did not answer; open the folder again to see whether the draft was opened."), json);
			}
			const auto decoded = adapters::sqlite::decode_tour_response(*answer);
			if (!decoded || decoded->Json.empty()) {
				return refuse("uncertain", QStringLiteral("The desktop's answer could not be read."), json);
			}
			const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(decoded->Json));
			if (!document.isObject()) {
				return refuse("uncertain", QStringLiteral("The desktop's answer could not be read."), json);
			}
			return print(document.object(), json);
		}
		return refuse("capability_unavailable", QStringLiteral("Another BetterPullReview process owns this profile and offers no command channel this bpr can reach."), json);
	}
}
