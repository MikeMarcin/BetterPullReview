// [[RFC-BPR-0082]] AC06, [[BPR-1467]]. The fixed render set of an intraline
// candidate: the corpus's similar line pairs whose emphasis differs from the
// baseline's, each shown with its surrounding lines in the product's own diff
// workbench, rendered offscreen once with the engine's intraline edits and
// once with the candidate's. The owner judges the pictures in an attended
// review; this tool only selects and renders them.
//
// Usage: tool_diff_intraline_render_set --corpus DIR --candidate NAME --out DIR [--count N]
// Writes pair-NN-baseline.png, pair-NN-candidate.png and render-set.json.

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_compute_components.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "candidates/candidates.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] auto comparison_key(const core::diff_options& aOptions, const std::uint8_t aSeed) -> core::file_comparison_key
	{
		const auto blob = [](const std::uint8_t aValue) {
			std::array<std::uint8_t, 32U> bytes{ };
			for (std::size_t index = 0U; index < 20U; ++index) {
				bytes[index] = static_cast<std::uint8_t>(aValue + static_cast<std::uint8_t>(index));
			}
			return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
		};
		const auto scope       = opaque<core::pull_request_authority_scope_id>(11U);
		const auto incarnation = opaque<core::epoch_catalog_incarnation_id>(12U);
		const auto authority   = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(13U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = scope,
		};
		return core::file_comparison_key{
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = { .Authority = scope, .Incarnation = incarnation, .Kind = core::comparison_anchor_kind::epoch_base, .Epoch = core::epoch_ordinal{ 1 } },
			.RightAnchor = { .Authority = scope, .Incarnation = incarnation, .Kind = core::comparison_anchor_kind::epoch_head, .Epoch = core::epoch_ordinal{ 2 } },
			.LeftBlob    = blob(static_cast<std::uint8_t>(21U + aSeed)),
			.RightBlob   = blob(static_cast<std::uint8_t>(51U + aSeed)),
			.File        = opaque<core::file_identity>(static_cast<std::uint8_t>(31U + aSeed)),
			.Options     = *core::options_digest(aOptions),
			.Algorithm   = core::supported_diff_algorithm_revision,
		};
	}

	[[nodiscard]] auto read_bytes(const std::filesystem::path& aPath) -> std::vector<std::uint8_t>
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	[[nodiscard]] auto compute(const std::vector<std::uint8_t>& aLeft, const std::vector<std::uint8_t>& aRight, const std::uint8_t aSeed,
							   core::diff_components* aComponents) -> std::optional<core::computed_file_diff>
	{
		const auto options = core::diff_options{ };
		const auto input   = core::diff_compute_input{
			.Key        = comparison_key(options, aSeed),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ aLeft },
			.RightBytes = std::span<const std::uint8_t>{ aRight },
		};
		auto outcome = aComponents == nullptr ? core::compute_file_diff(input) : core::compute_file_diff(input, *aComponents);
		return outcome.Result;
	}

	using edit_list = std::vector<std::pair<core::byte_span, core::byte_span>>;

	[[nodiscard]] auto edits_by_pair(const core::computed_file_diff& aDiff) -> std::map<std::pair<std::uint32_t, std::uint32_t>, edit_list>
	{
		auto lines = std::map<core::line_pair_id, std::pair<std::uint32_t, std::uint32_t>>{ };
		for (const auto& pair : aDiff.Alignment.Pairs) {
			lines.emplace(pair.Id, std::pair{ pair.LeftLine, pair.RightLine });
		}
		auto result = std::map<std::pair<std::uint32_t, std::uint32_t>, edit_list>{ };
		for (const auto& edit : aDiff.Intraline) {
			result[lines.at(edit.Pair)].emplace_back(edit.Left, edit.Right);
		}
		return result;
	}

	/// Lines [aBegin, aEnd) of a document, each with its terminator.
	[[nodiscard]] auto window(const core::decoded_text_document& aDocument, const std::uint32_t aBegin, const std::uint32_t aEnd) -> std::vector<std::uint8_t>
	{
		auto result = std::vector<std::uint8_t>{ };
		for (auto line = aBegin; line < aEnd && line < aDocument.Lines.size(); ++line) {
			const auto& record = aDocument.Lines[line];
			result.insert(result.end(), aDocument.Bytes.begin() + static_cast<std::ptrdiff_t>(record.Content.Begin),
						  aDocument.Bytes.begin() + static_cast<std::ptrdiff_t>(record.Terminator.End));
		}
		return result;
	}

	[[nodiscard]] auto render(const core::computed_file_diff& aDiff) -> QImage
	{
		auto workbench = qt::diff_workbench{ };
		// The default appearance supplies the intraline emphasis colours;
		// without a profile the panes draw no emphasis at all.
		workbench.adopt_appearance(qt::appearance_profile{ });
		workbench.resize(1'400, 300);
		workbench.show();
		for (int turn = 0; turn < 4; ++turn) {
			QApplication::processEvents();
		}
		const auto context = core::diff_request_context{
			.Request            = core::request_id{ 1U },
			.ApplicationSession = core::application_session_id{ 1U },
			.Key                = aDiff.Alignment.Key,
			.Generation         = core::request_generation{ 1U },
		};
		workbench.set_loading(context);
		auto publication = application::diff_publication{ .Context = context, .Result = std::make_shared<const core::computed_file_diff>(aDiff) };
		auto index       = core::build_diff_semantic_index(publication.Result);
		if (index) {
			publication.Index = std::make_shared<const core::diff_semantic_index>(std::move(*index));
		}
		workbench.publish(publication);
		auto timer = QElapsedTimer{ };
		timer.start();
		do {
			QApplication::processEvents();
		} while (workbench.geometry_pending() && timer.elapsed() < 5'000);
		for (int turn = 0; turn < 8; ++turn) {
			QApplication::processEvents();
		}
		return workbench.grab().toImage();
	}

	[[nodiscard]] auto text_of(const std::vector<std::uint8_t>& aBytes) -> QString
	{
		return QString::fromUtf8(reinterpret_cast<const char*>(aBytes.data()), static_cast<qsizetype>(aBytes.size()));
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
	::setenv("BPR_DIFF_EVALUATION_CANDIDATE", "baseline", 1);
	auto application = QApplication{ aArgc, aArgv };
	auto corpus      = std::filesystem::path{ };
	auto out         = std::filesystem::path{ };
	auto candidate   = std::string{ };
	auto count       = 10;
	for (int index = 1; index + 1 < aArgc; index += 2) {
		const auto key = std::string{ aArgv[index] };
		if (key == "--corpus") {
			corpus = aArgv[index + 1];
		} else if (key == "--out") {
			out = aArgv[index + 1];
		} else if (key == "--candidate") {
			candidate = aArgv[index + 1];
		} else if (key == "--count") {
			count = std::atoi(aArgv[index + 1]);
		}
	}
	auto components = evaluation::make_candidate(candidate);
	if (!components || corpus.empty() || out.empty()) {
		std::fputs("usage: tool_diff_intraline_render_set --corpus DIR --candidate NAME --out DIR [--count N]\n", stderr);
		return 2;
	}
	std::filesystem::create_directories(out);
	auto manifest = QFile{ QString::fromStdString((corpus / "manifest.json").string()) };
	if (!manifest.open(QIODevice::ReadOnly)) {
		return 2;
	}
	const auto pairs    = QJsonDocument::fromJson(manifest.readAll()).object().value("pairs").toArray();
	auto       selected = QJsonArray{ };
	auto       taken    = 0;
	auto       seen     = std::map<std::string, int>{ };
	for (const auto value : pairs) {
		const auto entry = value.toObject();
		if (taken >= count || entry.value("options").toString() != "default") {
			continue;
		}
		const auto left     = read_bytes(corpus / entry.value("left").toString().toStdString());
		const auto right    = read_bytes(corpus / entry.value("right").toString().toStdString());
		const auto baseline = compute(left, right, 1U, nullptr);
		const auto proposed = compute(left, right, 1U, components.get());
		if (!baseline || !proposed) {
			continue;
		}
		const auto ours   = edits_by_pair(*baseline);
		const auto theirs = edits_by_pair(*proposed);
		// At most two pairs from one comparison, so the set spans the corpus.
		for (const auto& [lines, edits] : theirs) {
			const auto found = ours.find(lines);
			if (found == ours.end() || found->second == edits || taken >= count || seen[entry.value("id").toString().toStdString()] >= 2) {
				continue;
			}
			++seen[entry.value("id").toString().toStdString()];
			const auto [leftLine, rightLine] = lines;
			const auto leftWindow            = window(*baseline->LeftDocument, leftLine > 2U ? leftLine - 2U : 0U, leftLine + 3U);
			const auto rightWindow           = window(*baseline->RightDocument, rightLine > 2U ? rightLine - 2U : 0U, rightLine + 3U);
			const auto seed                  = static_cast<std::uint8_t>(10U + taken);
			const auto small                 = compute(leftWindow, rightWindow, seed, nullptr);
			const auto smallTheirs           = compute(leftWindow, rightWindow, seed, components.get());
			if (!small || !smallTheirs || edits_by_pair(*small) == edits_by_pair(*smallTheirs)) {
				continue;
			}
			const auto stem = QStringLiteral("pair-%1").arg(taken, 2, 10, QLatin1Char('0'));
			render(*small).save(QString::fromStdString((out / (stem + "-baseline.png").toStdString()).string()));
			render(*smallTheirs).save(QString::fromStdString((out / (stem + "-candidate.png").toStdString()).string()));
			selected.append(QJsonObject{
				{ "stem", stem },
				{ "source", entry.value("id") },
				{ "left_line", static_cast<qint64>(leftLine + 1U) },
				{ "right_line", static_cast<qint64>(rightLine + 1U) },
				{ "left", text_of(leftWindow) },
				{ "right", text_of(rightWindow) },
			});
			++taken;
		}
	}
	auto file = QFile{ QString::fromStdString((out / "render-set.json").string()) };
	if (!file.open(QIODevice::WriteOnly)) {
		return 2;
	}
	file.write(QJsonDocument{ QJsonObject{ { "candidate", QString::fromStdString(candidate) }, { "pairs", selected } } }.toJson());
	std::printf("RENDER_SET %d\n", taken);
	return 0;
}
