#include "tour_cli.hpp"
#include "bpr/core/review_tour.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <iostream>
namespace bpr::app {
	auto run_tour_cli(const QStringList& aArguments) -> int
	{
		const bool json      = aArguments.contains(QStringLiteral("--json"));
		auto       arguments = aArguments;
		arguments.removeAll(QStringLiteral("--json"));
		const bool capabilities = arguments == QStringList{ QStringLiteral("tour"), QStringLiteral("capabilities") };
		const auto command      = arguments.size() > 1 ? arguments[1] : QStringLiteral("help");
		if (!capabilities && !arguments.isEmpty() && arguments != QStringList{ QStringLiteral("--help") } && arguments != QStringList{ QStringLiteral("tour"), QStringLiteral("--help") }) {
			if (json) {
				const QJsonObject result{ { "schema", "bpr.tour.cli-result.v1" }, { "command", command }, { "request_id", QUuid::createUuid().toString(QUuid::WithoutBraces) }, { "status", "error" }, { "data", QJsonObject{ } }, { "warnings", QJsonArray{ } }, { "error", QJsonObject{ { "code", "unsupported_command" }, { "message", "Use bpr tour capabilities." }, { "recoverable", true } } } };
				std::cout << QJsonDocument{ result }.toJson(QJsonDocument::Compact).constData() << '\n';
			} else {
				std::cerr << "Unsupported command. Use bpr tour capabilities.\n";
			}
			return 2;
		}
		if (json) {
			const QJsonObject data{ { "cli_contract", "1" }, { "commands", QJsonArray{ "capabilities" } }, { "max_field_bytes", QString::number(core::maximum_tour_field_bytes) }, { "gui_required", false }, { "ai_required", false }, { "remote_writes", false } };
			const QJsonObject result{ { "schema", "bpr.tour.cli-result.v1" }, { "command", command }, { "request_id", QUuid::createUuid().toString(QUuid::WithoutBraces) }, { "status", "ok" }, { "data", data }, { "warnings", QJsonArray{ } }, { "error", QJsonValue::Null } };
			std::cout << QJsonDocument{ result }.toJson(QJsonDocument::Compact).constData() << '\n';
		} else {
			std::cout << "bpr tour — local review-tour terminal\nCLI contract: 1\nAvailable: capabilities\nNo GUI or AI provider required.\n";
		}
		return 0;
	}
}
