// [[BPR-1442]]. Classifies every corpus file with the shipped classifier and
// nothing else: no candidate, no shim, no tree-sitter. RFC-BPR-0081 AC07
// compares its output at the revision before the evaluation with its output
// after it. Prints one line per file: path, status, span count and an FNV-1a
// digest of every span field.
//
// Usage: syntax_span_dump --root DIR --index FILE

#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {
	namespace app    = bpr::application;
	namespace core   = bpr::core;
	namespace syntax = bpr::adapters::syntax;

	class corpus_blob final : public syntax::blob_source
	{
	public:
		[[nodiscard]] auto blob_bytes(const core::git_blob_oid&) -> std::optional<std::string> override
		{
			return Bytes;
		}

		std::string Bytes;
	};

	[[nodiscard]] auto digest(const app::token_span_list& aSpans) -> std::string
	{
		auto hash = std::uint64_t{ 0xcbf29ce484222325ULL };
		auto mix  = [&hash](const std::uint32_t aValue) {
			for (auto shift = 0U; shift < 32U; shift += 8U) {
				hash ^= (aValue >> shift) & 0xFFU;
				hash *= 0x100000001b3ULL;
			}
		};
		for (const auto& span : aSpans) {
			mix(span.Line);
			mix(span.Column);
			mix(span.Length);
			mix(static_cast<std::uint32_t>(span.Role));
		}
		auto text = std::array<char, 17U>{ };
		std::snprintf(text.data(), text.size(), "%016llx", static_cast<unsigned long long>(hash));
		return text.data();
	}
}

auto main(int aCount, char** aArguments) -> int
{
	if (aCount != 5 || std::string_view{ aArguments[1] } != "--root" || std::string_view{ aArguments[3] } != "--index") {
		std::cerr << "usage: syntax_span_dump --root DIR --index FILE\n";
		return EXIT_FAILURE;
	}
	QStandardPaths::setTestModeEnabled(true);
	const auto root  = std::string{ aArguments[2] };
	auto       index = QFile{ QString::fromUtf8(aArguments[4]) };
	if (!index.open(QIODevice::ReadOnly)) {
		std::cerr << "cannot read the corpus index\n";
		return EXIT_FAILURE;
	}
	const auto document   = QJsonDocument::fromJson(index.readAll());
	const auto entries    = document.isArray() ? document.array() : document.object().value(QStringLiteral("files")).toArray();
	const auto catalog    = syntax::packaged_definition_catalog{ };
	auto       blobs      = corpus_blob{ };
	auto       classifier = syntax::packaged_syntax_classifier{ catalog, blobs };
	auto       seed       = std::uint32_t{ 0U };
	for (const auto& entry : entries) {
		const auto path       = entry.toObject().value(QStringLiteral("path")).toString().toStdString();
		const auto definition = catalog.definition_for_path(path);
		if (definition.Name != "C++") {
			continue;
		}
		auto stream = std::ifstream{ std::filesystem::path{ root } / path, std::ios::binary };
		auto bytes  = std::ostringstream{ };
		bytes << stream.rdbuf();
		blobs.Bytes = bytes.str();
		auto key    = std::array<std::uint8_t, 32U>{ };
		++seed;
		key[0]             = static_cast<std::uint8_t>(seed);
		key[1]             = static_cast<std::uint8_t>(seed >> 8U);
		key[2]             = static_cast<std::uint8_t>(seed >> 16U);
		const auto request = app::syntax_classification_request{
			.Context      = { },
			.Key          = { .Blob = core::git_blob_oid{ core::git_hash_algorithm::sha1, key }, .Definition = definition },
			.MaximumSpans = 262'144U,
		};
		const auto result = classifier.classify(request, { });
		std::cout << path << '\t' << static_cast<int>(result.Status) << '\t' << result.Spans.size() << '\t' << digest(result.Spans) << '\n';
	}
	return EXIT_SUCCESS;
}
