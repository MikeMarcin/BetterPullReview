#ifndef BPR_TESTS_DIAGNOSTIC_JSON_HPP
#define BPR_TESTS_DIAGNOSTIC_JSON_HPP
#include "test_expectation.hpp"
#include <QByteArray>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <algorithm>
#include <map>
#include <string_view>
#include <vector>
namespace bpr::tests {
	/// \brief Independently parses real JSONL and checks record identity, time and ordering.
	inline auto diagnostic_json_records(std::string_view aBytes) -> std::vector<QJsonObject>
	{
		std::vector<QJsonObject>                     records;
		std::map<QString, std::pair<qint64, qint64>> sessions;
		while (!aBytes.empty()) {
			const auto end = aBytes.find('\n');
			expect(end != std::string_view::npos && end < 16U * 1024U, "complete bounded JSONL record");
			const auto      line = aBytes.substr(0, end);
			QJsonParseError error;
			const auto      document = QJsonDocument::fromJson(QByteArray{ line.data(), static_cast<qsizetype>(line.size()) }, &error);
			expect(error.error == QJsonParseError::NoError && document.isObject(), "independent JSON parser accepts a complete object");
			const auto record = document.object();
			expect(record.size() == 30 && record.value("v").toInteger() == 3, "explicit complete version-three schema");
			const auto session = record.value("session").toString();
			expect(session.size() == 32 && std::ranges::all_of(session, [](QChar aValue) { return (aValue >= '0' && aValue <= '9') || (aValue >= 'a' && aValue <= 'f'); }), "opaque 128-bit session identity");
			const auto utc = record.value("utc_ms").toInteger(-1);
			const auto now = QDateTime::currentMSecsSinceEpoch();
			expect(utc > 0 && utc >= now - 600000 && utc <= now + 600000, "recorded UTC milliseconds describe this run");
			const auto sequence = record.value("sequence").toInteger();
			const auto elapsed  = record.value("elapsed_us").toInteger(-1);
			const auto previous = sessions.find(session);
			expect(sequence > 0 && elapsed >= 0 && (previous == sessions.end() || (sequence > previous->second.first && elapsed >= previous->second.second)), "session sequence and monotonic elapsed time progress");
			sessions[session]    = { sequence, elapsed };
			const auto operation = record.value("op").toString();
			const auto outcome   = record.value("outcome").toString();
			expect(record.value("event").toString() == operation + '.' + outcome, "event identity is closed operation plus outcome");
			expect(record.value("category").toString() == (operation.startsWith("github.") ? "github" : operation), "category binds the event family");
			expect(record.value("severity").toString() == (outcome == "failed" ? "error" : record.value("http_status").toInteger() >= 400 ? "warning"
																																		  : "info"),
				   "severity preserves failure and HTTP warning facts");
			const auto build = record.value("build").toString();
			expect(build.size() >= 70 && !build.contains('/') && !build.contains('\\'), "build binds version, content digest and configuration without local paths");
			if (!records.empty()) {
				expect(build == records.front().value("build").toString(), "one candidate has a stable build identity");
			}
			records.push_back(record);
			aBytes.remove_prefix(end + 1U);
		}
		expect(!records.empty(), "at least one persisted diagnostic");
		return records;
	}
}
#endif
