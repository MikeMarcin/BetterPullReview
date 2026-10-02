#include "cycle_cli.hpp"
#include "bpr/adapters/workspace/local_export_destination.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/review_cycle_words.hpp"
#include "bpr/core/review_cycle_bundle.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/core/review_cycle_round.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
#include "bpr/core/review_cycle_views.hpp"
#include "bpr/core/review_tour.hpp"
#include "bpr/core/sha256.hpp"
#include "cycle_session.hpp"
#include "terminal_text.hpp"
#include "tour_cancellation.hpp"
#include "tour_editor.hpp"
#include "tour_text_input.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <iostream>
#include <map>
#include <span>
#include <utility>
namespace bpr::app {
	namespace {
		/// The result envelope every `--json` call writes, exactly once, to standard output.
		constexpr auto result_schema = "bpr.cycle.cli-result.v1";
		/// Most entries one page of a list or comparison returns.
		constexpr std::size_t page_entries = 200U;
		/// Most diff or source rows one file comparison prints.
		constexpr std::size_t maximum_rows = 2000U;
		/// The longest note text `show` repeats per note.
		constexpr qsizetype note_preview_characters = 120;

		struct failure
		{
			std::string Code, Message, Next;
			QJsonObject Detail;
		};
		template <typename T>
		using result = std::expected<T, failure>;
		auto fail(std::string aCode, std::string aMessage, std::string aNext = { }, QJsonObject aDetail = { }) -> std::unexpected<failure>
		{
			return std::unexpected(failure{ std::move(aCode), std::move(aMessage), std::move(aNext), std::move(aDetail) });
		}

		/// One exit status per kind of outcome, the same for human and JSON output.
		auto exit_code(const std::string_view aCode) -> int
		{
			const auto any = [aCode](std::initializer_list<std::string_view> aCodes) { return std::ranges::find(aCodes, aCode) != aCodes.end(); };
			if (aCode == "interaction_required") {
				return 3;
			}
			if (any({ "stale_revision", "conflict", "draft_busy" })) {
				return 4;
			}
			if (any({ "policy_denied", "capability_unavailable", "source_unavailable", "destination_unavailable", "round_not_ready", "capture_unstable", "over_limit", "requires_partition" })) {
				return 5;
			}
			if (aCode == "uncertain") {
				return 6;
			}
			if (any({ "storage_unavailable", "profile_unavailable", "operation_failed" })) {
				return 7;
			}
			if (aCode == "cancelled") {
				return 130;
			}
			return 2;
		}

		auto text(const std::string_view aValue) -> QString
		{
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		}
		auto utf8(const QString& aValue) -> std::string
		{
			const auto bytes = aValue.toUtf8();
			return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		}
		auto decimal(const std::uint64_t aValue) -> QString
		{
			return QString::number(aValue);
		}
		template <typename TIdentity>
		auto wire(const TIdentity aIdentity) -> QString
		{
			return text(core::cycle_wire_name(aIdentity));
		}
		template <typename TTag>
		auto hex(const core::fixed_identity_256<TTag>& aDigest) -> QString
		{
			QString result;
			for (const auto byte : aDigest.bytes()) {
				result += QStringLiteral("%1").arg(byte, 2, 16, QChar{ '0' });
			}
			return result;
		}
		auto digest_of(const QString& aHex) -> std::optional<core::cycle_material_digest>
		{
			if (aHex.size() != 64) {
				return std::nullopt;
			}
			auto bytes = std::array<std::uint8_t, 32>{ };
			for (std::size_t index = 0; index < bytes.size(); ++index) {
				bool       parsed = false;
				const auto at     = static_cast<qsizetype>(index) * 2;
				const auto value  = aHex.mid(at, 2).toUInt(&parsed, 16);
				if (!parsed || aHex[at].isUpper() || aHex[at + 1].isUpper()) {
					return std::nullopt;
				}
				bytes[index] = static_cast<std::uint8_t>(value);
			}
			return core::cycle_material_digest{ bytes };
		}
		auto positive(const QString& aValue) -> std::optional<std::uint64_t>
		{
			const auto    bytes = aValue.toUtf8();
			std::uint64_t value = 0;
			if (bytes.isEmpty() || bytes.front() == '0') {
				return std::nullopt;
			}
			const auto parsed = std::from_chars(bytes.data(), bytes.data() + bytes.size(), value);
			if (parsed.ec != std::errc{ } || parsed.ptr != bytes.data() + bytes.size()) {
				return std::nullopt;
			}
			return value;
		}

		// ---- Words for the terminal: what the desktop says, and stable names for scripts.

		auto store_failure(const application::cycle_store_error& aError) -> failure
		{
			using application::cycle_store_failure;
			const auto words = application::cycle_store_words(aError);
			switch (aError.Failure) {
			case cycle_store_failure::conflict:         return { "stale_revision", words + " Read it again before repeating the change.", { }, { } };
			case cycle_store_failure::operation_reused: return { "conflict", "That operation identity was already used for another change; nothing was changed.", { }, { } };
			case cycle_store_failure::not_found:        return { "not_found", "That is not part of this cycle.", { }, { } };
			case cycle_store_failure::refused:
				if (aError.Refusal == core::cycle_refusal::conflict || aError.Refusal == core::cycle_refusal::stale_workspace) {
					return { "stale_revision", words + " Read it again before repeating the change.", { }, { } };
				}
				return { aError.Refusal == core::cycle_refusal::archived ? "conflict" : "invalid_input", words, { }, { } };
			case cycle_store_failure::quota:              return { "over_limit", words, { }, { } };
			case cycle_store_failure::schema_mismatch:    return { "unsupported_version", words, { }, { } };
			case cycle_store_failure::cancelled:          return { "cancelled", "Cancelled; nothing further was changed.", { }, { } };
			case cycle_store_failure::corrupt:
			case cycle_store_failure::object_unavailable: return { "source_unavailable", words, { }, { } };
			default:                                      return { "storage_unavailable", words, { }, { } };
			}
		}
		template <typename T>
		auto checked(application::cycle_store_result<T> aValue) -> result<T>
		{
			if (!aValue) {
				return std::unexpected(store_failure(aValue.error()));
			}
			return std::move(*aValue);
		}

		auto kind_name(const core::workspace_root_kind aKind) -> QString
		{
			switch (aKind) {
			case core::workspace_root_kind::folder: return "folder";
			case core::workspace_root_kind::file:   return "file";
			default:                                return "git_checkout";
			}
		}
		auto change_name(const core::cycle_path_change aChange) -> QString
		{
			switch (aChange) {
			case core::cycle_path_change::unchanged: return "unchanged";
			case core::cycle_path_change::modified:  return "modified";
			case core::cycle_path_change::added:     return "added";
			case core::cycle_path_change::deleted:   return "deleted";
			case core::cycle_path_change::unknown:   return "unknown";
			}
			return "unknown";
		}
		auto state_name(const core::cycle_path_state aState) -> QString
		{
			switch (aState) {
			case core::cycle_path_state::present:       return "present";
			case core::cycle_path_state::deleted:       return "deleted";
			case core::cycle_path_state::absent:        return "absent";
			case core::cycle_path_state::unknown:       return "unknown";
			case core::cycle_path_state::outside_scope: return "outside_scope";
			case core::cycle_path_state::unavailable:   return "unavailable";
			}
			return "unknown";
		}
		auto delivery_name(const core::cycle_delivery_state aState) -> QString
		{
			switch (aState) {
			case core::cycle_delivery_state::copied:                 return "copied";
			case core::cycle_delivery_state::saved:                  return "saved";
			case core::cycle_delivery_state::retrieved:              return "retrieved";
			case core::cycle_delivery_state::accepted_by_adapter:    return "accepted_by_adapter";
			case core::cycle_delivery_state::recipient_acknowledged: return "recipient_acknowledged";
			case core::cycle_delivery_state::failed:                 return "failed";
			case core::cycle_delivery_state::uncertain:              return "uncertain";
			}
			return "uncertain";
		}
		auto channel_name(const core::cycle_disclosure_channel aChannel) -> QString
		{
			switch (aChannel) {
			case core::cycle_disclosure_channel::clipboard:          return "clipboard";
			case core::cycle_disclosure_channel::local_file:         return "local_file";
			case core::cycle_disclosure_channel::terminal_output:    return "terminal_output";
			case core::cycle_disclosure_channel::external_retrieval: return "external_retrieval";
			case core::cycle_disclosure_channel::agent_adapter:      return "agent_adapter";
			}
			return "unknown";
		}
		/// A refused export: nothing left, and no other channel stood in for it.
		auto policy_failure(const core::cycle_authorization aDecision, const core::cycle_disclosure_channel aChannel) -> failure
		{
			const auto where = aChannel == core::cycle_disclosure_channel::terminal_output ? std::string{ "printing to the terminal" } : std::string{ "saving to a file" };
			const auto why   = aDecision == core::cycle_authorization::channel_denied ? "Policy for this project's source does not allow " + where + "."
				: aDecision == core::cycle_authorization::channel_unconfigured        ? "Nothing configured allows " + where + " for source of unknown origin."
																					  : "The export was refused (" + std::string{ core::name(aDecision) } + ").";
			return { "policy_denied", why + " Nothing was written, and no other way was used instead.", { }, QJsonObject{ { "decision", text(core::name(aDecision)) } } };
		}
		auto limit_failure(const core::cycle_round_output_limit& aLimit) -> failure
		{
			return { "over_limit",
					 "The round is " + application::grouped_count(aLimit.RequiredBytes) + " bytes (" + application::grouped_count(aLimit.Items) + " items, " + application::grouped_count(aLimit.Sources)
						 + " sources), over the limit of " + application::grouped_count(aLimit.BudgetBytes) + " bytes. Nothing was cut short or written.",
					 { },
					 QJsonObject{ { "required_bytes", decimal(aLimit.RequiredBytes) }, { "budget_bytes", decimal(aLimit.BudgetBytes) } } };
		}
		auto round_failure(const core::cycle_round_refusal& aRefusal, const std::string& aRetry) -> failure
		{
			const auto problem = text(core::stable_name(aRefusal.Problem));
			const auto detail  = QJsonObject{ { "problem", problem }, { "item_index", aRefusal.Item ? QJsonValue{ decimal(*aRefusal.Item) } : QJsonValue{ } } };
			switch (aRefusal.Problem) {
			case core::cycle_round_problem::empty:            return { "invalid_input", "The round holds no note. Include one with bpr cycle review --include I<n>.", { }, detail };
			case core::cycle_round_problem::oversized:        return { "requires_partition", "The round is over its size budget; split it into rounds you choose. Nothing was frozen.", { }, detail };
			case core::cycle_round_problem::archived_item:    return { "conflict", "A note in the round is archived; leave it out first. Nothing was frozen.", { }, detail };
			case core::cycle_round_problem::stale_wording:
			case core::cycle_round_problem::stale_suggestion:
			case core::cycle_round_problem::stale_draft:      return { "stale_revision", "The round changed since it was read; read it again. Nothing was frozen.", aRetry, detail };
			default:                                          return { "invalid_input", "This round cannot be frozen as it stands (" + utf8(problem) + "); nothing was frozen.", { }, detail };
			}
		}

		// ---- The command line.

		struct command
		{
			QString                Name;
			QStringList            Positional;
			QMap<QString, QString> Values;
			bool                   Json = false, Interactive = false;
			QString                Operation = QUuid::createUuid().toString(QUuid::WithoutBraces);
			std::filesystem::path  Base;
			std::string            Profile;
			[[nodiscard]] auto     has(const QString& aOption) const -> bool { return Values.contains(aOption); }
			[[nodiscard]] auto     value(const QString& aOption) const -> QString { return Values.value(aOption); }
			/// The cycle as the caller named it, for a next command.
			[[nodiscard]] auto named() const -> std::string { return Positional.isEmpty() || Name == "open" ? std::string{ } : " " + utf8(Positional.front()); }
		};
		struct output
		{
			QJsonObject Data;
			QString     Human;
			/// A page with more after it is not the complete answer.
			bool Complete = true;
		};
		struct command_spec
		{
			QString     Name;
			int         Positional = 1;
			QStringList Options;
		};
		auto specs() -> const std::vector<command_spec>&
		{
			static const auto table = std::vector<command_spec>{
				{ "capabilities", 0, { } },
				{ "list", 0, { "--cursor" } },
				{ "open", 1, { } },
				{ "show", 1, { } },
				{ "capture", 1, { "--again" } },
				{ "note", 1, { "--file", "--lines", "--body-file" } },
				{ "review", 1, { "--include", "--exclude", "--earlier", "--later", "--action", "--goal-file", "--expected-revision" } },
				{ "freeze", 1, { "--expected-revision" } },
				{ "export", 1, { "--round", "--output", "--format", "--replace" } },
				{ "compare", 1, { "--from", "--to", "--changed-only", "--cursor", "--path" } },
				{ "response import", 1, { "--round", "--file" } },
			};
			return table;
		}
		auto command_names() -> QStringList
		{
			QStringList names;
			for (const auto& spec : specs()) {
				names.push_back(spec.Name);
			}
			return names;
		}

		auto parse(const QStringList& aArguments) -> result<command>
		{
			if (aArguments.size() > 64) {
				return fail("invalid_input", "Too many command arguments.");
			}
			const QStringList flags{ "--json", "--non-interactive", "--again", "--changed-only" };
			const QStringList valued{ "--profile", "--data-root", "--schema-version", "--cursor", "--file", "--lines", "--body-file", "--include", "--exclude", "--earlier", "--later", "--action",
									  "--goal-file", "--expected-revision", "--round", "--output", "--format", "--replace", "--from", "--to", "--path" };
			auto              result = command{ };
			QStringList       words;
			for (qsizetype index = 0; index < aArguments.size(); ++index) {
				const auto& argument = aArguments[index];
				if (argument.size() > 8192) {
					return fail("invalid_input", "A command argument exceeds the bound.");
				}
				if (!argument.startsWith("--")) {
					words.push_back(argument);
					continue;
				}
				if (result.Values.contains(argument)) {
					return fail("invalid_input", "An option was repeated.");
				}
				if (flags.contains(argument)) {
					result.Values.insert(argument, "true");
					continue;
				}
				// No generic --yes or --force: authority is never a flag.
				if (!valued.contains(argument) || index + 1 == aArguments.size()) {
					return fail("unsupported_option", "Unknown option or missing option value: " + utf8(argument.left(64)) + ".", "bpr cycle capabilities");
				}
				result.Values.insert(argument, aArguments[++index]);
			}
			result.Json        = result.Values.contains("--json");
			result.Interactive = interactive_terminal() && !result.Json && !result.Values.contains("--non-interactive");
			if (result.Values.contains("--schema-version") && result.Values["--schema-version"] != "1") {
				return fail("unsupported_version", "This bpr reads and writes CLI schema version 1 only.", "bpr cycle capabilities --json");
			}
			if (words.isEmpty() || words.front() == "help") {
				result.Name = "capabilities";
				return result;
			}
			if (words.front() == "response") {
				if (words.size() < 2 || words[1] != "import") {
					return fail("unsupported_command", "Use bpr cycle response import RC-<n> --round F<n> --file PATH.", "bpr cycle capabilities");
				}
				words[1] = "response import";
				words.removeFirst();
			}
			const auto spec = std::ranges::find(specs(), words.front(), &command_spec::Name);
			if (spec == specs().end()) {
				return fail("unsupported_command", "bpr cycle has no command " + utf8(words.front().left(64)) + ".", "bpr cycle capabilities");
			}
			result.Name       = spec->Name;
			result.Positional = words.mid(1);
			const auto needed = result.Name == "open" ? 1 : 0;
			if (result.Positional.size() < needed || result.Positional.size() > spec->Positional) {
				return fail("invalid_input", result.Name == "open" ? "Name the folder or file to open: bpr cycle open PATH." : "Name at most one cycle, as RC-<n>.");
			}
			QStringList allowed{ "--json", "--non-interactive", "--schema-version", "--data-root" };
			if (result.Name != "capabilities") {
				allowed.append("--profile");
			}
			allowed.append(spec->Options);
			for (const auto& option : result.Values.keys()) {
				if (!allowed.contains(option)) {
					return fail("unsupported_option", "bpr cycle " + utf8(result.Name) + " does not take " + utf8(option) + ".", "bpr cycle capabilities");
				}
			}
			const auto base = result.Values.value("--data-root", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
			if (!QDir::isAbsolutePath(base)) {
				return fail("invalid_input", "--data-root must be an absolute private path.");
			}
			result.Base    = native_filesystem_path(base);
			result.Profile = utf8(result.Values.value("--profile"));
			return result;
		}

		/// A path the caller typed, made absolute against the working folder; UTF-8.
		auto chosen_path(const QString& aPath) -> std::string
		{
			return utf8(QDir::cleanPath(QDir::current().absoluteFilePath(aPath)));
		}
		auto utf8_path(const std::string& aPath) -> std::filesystem::path
		{
			return native_filesystem_path(text(aPath));
		}

		// ---- Sessions, cycles and the working folder.

		auto session_for(const command& aCommand) -> result<std::unique_ptr<cycle_session>>
		{
			auto opened = cycle_session::open(aCommand.Base, aCommand.Profile);
			if (!opened) {
				const auto& error = opened.error();
				return fail(error.Code, error.Message);
			}
			return std::move(*opened);
		}

		auto catalog(cycle_session& aSession) -> result<core::review_cycle_catalog>
		{
			return checked(aSession.await<core::review_cycle_catalog>([&](auto aDone) { return aSession.runtime().open(std::move(aDone)); }));
		}
		auto all_cycles(cycle_session& aSession) -> result<std::vector<core::review_cycle_projection>>
		{
			auto cycles = std::vector<core::review_cycle_projection>{ };
			auto after  = std::optional<core::review_cycle_id>{ };
			do {
				auto page = checked(aSession.await<application::cycle_page>([&](auto aDone) { return aSession.runtime().list(after, std::move(aDone)); }));
				if (!page) {
					return std::unexpected(page.error());
				}
				std::ranges::move(page->Cycles, std::back_inserter(cycles));
				after = page->Next;
			} while (after);
			return cycles;
		}
		auto load_cycle(cycle_session& aSession, const core::local_cycle_subject& aSubject) -> result<std::optional<core::review_cycle_state>>
		{
			return checked(aSession.await<std::optional<core::review_cycle_state>>([&](auto aDone) { return aSession.runtime().load(aSubject, std::move(aDone)); }));
		}
		auto observe(cycle_session& aSession, const std::string& aLocation) -> result<core::workspace_root_observation>
		{
			auto observed = checked(aSession.await<application::workspace_root_observation_result>([&](auto aDone) { return aSession.runtime().observe_workspace(aSession.workspace(), aLocation, std::move(aDone)); }));
			if (!observed) {
				return std::unexpected(observed.error());
			}
			if (!observed->Root) {
				return fail("source_unavailable", application::cycle_workspace_words(observed->Failure.value_or(application::workspace_failure::io)));
			}
			return std::move(*observed->Root);
		}
		/// Every active cycle bound to this root object, by the rule Open Project uses.
		auto bound_cycles(cycle_session& aSession, const core::workspace_root_observation& aRoot) -> result<std::vector<core::review_cycle_state>>
		{
			auto cycles = all_cycles(aSession);
			if (!cycles) {
				return std::unexpected(cycles.error());
			}
			auto bound = std::vector<core::review_cycle_state>{ };
			for (const auto& cycle : *cycles) {
				if (cycle.WorkspaceKind != aRoot.Kind || cycle.Lifecycle != core::cycle_lifecycle::active) {
					continue;
				}
				auto state = load_cycle(aSession, cycle.Subject);
				if (!state) {
					return std::unexpected(state.error());
				}
				if (*state && core::bound_to(**state, aRoot)) {
					bound.push_back(std::move(**state));
				}
			}
			return bound;
		}
		auto load_selected(cycle_session& aSession) -> result<core::review_cycle_state>
		{
			auto state = checked(aSession.await<std::optional<core::review_cycle_state>>([&](auto aDone) { return aSession.runtime().load_selected(std::move(aDone)); }));
			if (!state) {
				return std::unexpected(state.error());
			}
			if (!*state) {
				return fail("not_found", "The cycle no longer exists.", "bpr cycle list");
			}
			return std::move(**state);
		}

		/// The cycle a command names, or the one bound to the working folder when
		/// that is unambiguous. Never a remembered or "current" cycle.
		auto resolve(cycle_session& aSession, const command& aCommand) -> result<core::review_cycle_state>
		{
			if (auto opened = catalog(aSession); !opened) {
				return std::unexpected(opened.error());
			}
			auto subject = std::optional<core::local_cycle_subject>{ };
			if (!aCommand.Positional.isEmpty()) {
				const auto named = core::parse_cycle_name(utf8(aCommand.Positional.front()));
				if (!named) {
					return fail("invalid_input", "Name a cycle as RC-<number>, as bpr cycle list shows it.", "bpr cycle list");
				}
				auto cycles = all_cycles(aSession);
				if (!cycles) {
					return std::unexpected(cycles.error());
				}
				const auto found = std::ranges::find(*cycles, *named, [](const core::review_cycle_projection& aCycle) { return aCycle.Subject.Cycle; });
				if (found == cycles->end()) {
					return fail("not_found", utf8(aCommand.Positional.front()) + " is not a cycle of this profile.", "bpr cycle list");
				}
				subject = found->Subject;
			} else {
				auto root = observe(aSession, utf8(QDir::currentPath()));
				if (!root) {
					return fail("not_found", "No cycle is named, and the working folder cannot be read: " + root.error().Message, "bpr cycle list");
				}
				auto bound = bound_cycles(aSession, *root);
				if (!bound) {
					return std::unexpected(bound.error());
				}
				if (bound->empty()) {
					return fail("not_found", "No cycle is named, and none is bound to the working folder. Name one, or open this folder as a cycle.", "bpr cycle open .");
				}
				if (bound->size() > 1U) {
					QJsonArray candidates;
					for (const auto& cycle : *bound) {
						candidates.append(wire(cycle.Subject.Cycle));
					}
					return fail("target_ambiguous", "Several cycles are bound to the working folder; name one.", "bpr cycle list", QJsonObject{ { "candidates", candidates } });
				}
				subject = bound->front().Subject;
			}
			aSession.runtime().select(core::review_subject{ *subject });
			return load_selected(aSession);
		}

		auto cycle_json(const core::review_cycle_state& aCycle) -> QJsonObject
		{
			const auto& latest = aCycle.Latest;
			return {
				{ "cycle", wire(aCycle.Subject.Cycle) },
				{ "title", text(aCycle.Title) },
				{ "location", aCycle.Workspace ? QJsonValue{ text(aCycle.Workspace->Root.Location) } : QJsonValue{ } },
				{ "workspace_kind", aCycle.Workspace ? QJsonValue{ kind_name(aCycle.Workspace->Root.Kind) } : QJsonValue{ } },
				{ "lifecycle", aCycle.Lifecycle == core::cycle_lifecycle::active ? "active" : "archived" },
				{ "revision", decimal(aCycle.Revision) },
				{ "captures", decimal(aCycle.Next.SourceRevision - 1U) },
				{ "latest", latest ? QJsonValue{ wire(latest->Id.Id) } : QJsonValue{ } },
				{ "latest_entries", latest ? QJsonValue{ decimal(latest->Capture.Entries) } : QJsonValue{ } },
				{ "latest_coverage", latest ? QJsonValue{ latest->Capture.Coverage == core::cycle_capture_coverage::complete ? "complete" : "partial" } : QJsonValue{ } },
			};
		}
		auto cycle_words(const core::review_cycle_state& aCycle) -> QString
		{
			auto words = wire(aCycle.Subject.Cycle) + " \"" + text(aCycle.Title) + "\"";
			if (aCycle.Workspace) {
				words += " at " + text(aCycle.Workspace->Root.Location);
			}
			words += "\n";
			if (aCycle.Latest) {
				words += "Latest capture " + wire(aCycle.Latest->Id.Id) + " of " + decimal(aCycle.Next.SourceRevision - 1U) + " (" + decimal(aCycle.Latest->Capture.Entries) + " entries"
					+ (aCycle.Latest->Capture.Coverage == core::cycle_capture_coverage::complete ? "" : ", partial") + ").\n";
			} else {
				words += "Nothing is captured yet.\n";
			}
			return words;
		}

		struct capture_report
		{
			bool Recorded = false;
		};
		/// Reads the bound folder now: the desktop's Capture revision.
		auto capture_now(cycle_session& aSession, const core::review_cycle_state& aCycle, const bool aDeliberate) -> result<capture_report>
		{
			if (!aCycle.Workspace) {
				return fail("source_unavailable", "The cycle has no folder to capture.");
			}
			auto request = application::cycle_capture_request{ .Operation = aSession.operation(), .ExpectedRevision = aCycle.Revision, .Deliberate = aDeliberate };
			auto outcome = checked(aSession.await<application::cycle_capture_outcome>([&](auto aDone) { return aSession.runtime().capture_workspace(aSession.workspace(), std::move(request), std::move(aDone)); }));
			if (!outcome) {
				return std::unexpected(outcome.error());
			}
			const auto retry = "bpr cycle capture " + core::cycle_wire_name(aCycle.Subject.Cycle);
			switch (outcome->Status) {
			case application::cycle_capture_status::captured:         return capture_report{ .Recorded = !outcome->Receipt || outcome->Receipt->Changed };
			case application::cycle_capture_status::unstable:
			case application::cycle_capture_status::root_changed:     return fail("capture_unstable", application::cycle_capture_words(*outcome), retry);
			case application::cycle_capture_status::too_many_entries: return fail("over_limit", application::cycle_capture_words(*outcome));
			case application::cycle_capture_status::store:            return std::unexpected(outcome->Store ? store_failure(*outcome->Store) : failure{ "storage_unavailable", application::cycle_capture_words(*outcome), { }, { } });
			case application::cycle_capture_status::cancelled:        return fail("cancelled", "Cancelled; nothing was recorded.");
			default:                                                  return fail("source_unavailable", application::cycle_capture_words(*outcome));
			}
		}

		/// One file of a revision, exactly as retained.
		auto retained_file(cycle_session& aSession, const core::review_cycle_state& aCycle, const core::cycle_source_revision_id aRevision, const std::string& aPath) -> result<std::vector<std::uint8_t>>
		{
			const auto pair = core::cycle_comparison_pair{ .Left = std::nullopt, .Right = { .Cycle = aCycle.Subject.Cycle, .Id = aRevision } };
			auto       file = checked(aSession.await<application::cycle_file_observation>([&](auto aDone) { return aSession.runtime().compare_file_selected(pair, aPath, { }, std::move(aDone)); }));
			if (!file) {
				return std::unexpected(file.error());
			}
			switch (file->Status) {
			case application::review_material_status::available: break;
			case application::review_material_status::missing:   return fail("not_found", wire(aRevision).toStdString() + " holds no file " + aPath + ".");
			case application::review_material_status::too_large: return fail("over_limit", aPath + " is too large to read here.");
			default:                                             return fail("source_unavailable", "The retained bytes of " + aPath + " could not be read; nothing was substituted.");
			}
			if (file->Path && file->Path->Right.State != core::cycle_path_state::present) {
				return fail("not_found", wire(aRevision).toStdString() + " holds no file " + aPath + ".");
			}
			return std::move(file->Source);
		}
		auto require_revision(const core::review_cycle_state& aCycle, const core::cycle_source_revision_id aRevision) -> result<void>
		{
			if (aRevision.value() >= aCycle.Next.SourceRevision) {
				return fail("not_found", wire(aRevision).toStdString() + " was never captured in " + core::cycle_wire_name(aCycle.Subject.Cycle) + "; nothing else is shown in its place.");
			}
			return { };
		}

		// ---- Commands.

		auto capabilities() -> output
		{
			const auto data = QJsonObject{
				{ "cli_contract", "1" },
				{ "result_schema", result_schema },
				{ "commands", QJsonArray::fromStringList(command_names()) },
				{ "gui_required", false },
				{ "display_required", false },
				{ "ai_required", false },
				{ "provider_required", false },
				{ "account_required", false },
				{ "network", false },
				{ "cycle_names", "RC-<n> cycles, R<n> captures, I<n> notes, F<n> frozen rounds" },
				{ "target_resolution", "an explicit RC-<n>, or the one active cycle bound to the working folder; never a remembered or active-window cycle" },
				{ "owner", "one writer: a profile another BetterPullReview process owns is refused with capability_unavailable; desktop forwarding is not in this build" },
				{ "round_read", "not in this build: export a frozen round, or read it with bpr cycle export --output -" },
				{ "response_append", "reviewer import of a chosen file or standard input; it verifies and publishes nothing" },
				{ "export_channels", QJsonArray{ "local_file", "terminal_output" } },
				{ "limits", QJsonObject{ { "page_entries", decimal(page_entries) }, { "file_rows", decimal(maximum_rows) }, { "note_bytes", decimal(core::maximum_private_reviewer_body_bytes) }, { "document_bytes", decimal(core::maximum_round_document_file_bytes) }, { "bundle_bytes", decimal(core::maximum_bundle_archive_bytes) }, { "response_bytes", decimal(core::maximum_response_document_bytes) } } },
				{ "exit_codes", QJsonObject{ { "0", "ok" }, { "2", "invalid input or target" }, { "3", "interaction_required" }, { "4", "stale_revision or conflict" }, { "5", "refused or unavailable" }, { "6", "uncertain" }, { "7", "storage" }, { "130", "cancelled" } } },
			};
			return { data,
					 "bpr cycle - local Review Cycles from a terminal\n"
					 "CLI contract: 1\n"
					 "Commands: "
						 + command_names().join(", ") + "\n"
														"No window, display, account, provider, agent or network is needed. Add --json for one bpr.cycle.cli-result.v1 document.\n",
					 true };
		}

		auto run_list(const command& aCommand) -> result<output>
		{
			auto after = std::optional<core::review_cycle_id>{ };
			if (aCommand.has("--cursor")) {
				after = core::parse_cycle_name(utf8(aCommand.value("--cursor")));
				if (!after) {
					return fail("invalid_input", "--cursor takes the RC-<n> the previous page named.");
				}
			}
			auto session = session_for(aCommand);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			if (auto opened = catalog(owned); !opened) {
				return std::unexpected(opened.error());
			}
			auto page = checked(owned.await<application::cycle_page>([&](auto aDone) { return owned.runtime().list(after, std::move(aDone)); }));
			if (!page) {
				return std::unexpected(page.error());
			}
			QJsonArray cycles;
			QString    human;
			for (const auto& cycle : page->Cycles) {
				auto state = load_cycle(owned, cycle.Subject);
				if (!state) {
					return std::unexpected(state.error());
				}
				if (!*state) {
					continue;
				}
				cycles.append(cycle_json(**state));
				human += cycle_words(**state);
			}
			if (cycles.isEmpty()) {
				human = "No cycles yet. Open a folder: bpr cycle open PATH\n";
			}
			if (page->Next) {
				human += "More: bpr cycle list --cursor " + wire(*page->Next) + "\n";
			}
			return output{ { { "cycles", cycles }, { "next", page->Next ? QJsonValue{ wire(*page->Next) } : QJsonValue{ } } }, human, !page->Next };
		}

		auto run_open(const command& aCommand) -> result<output>
		{
			const auto location = chosen_path(aCommand.Positional.front());
			auto       session  = session_for(aCommand);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned  = **session;
			auto  opened = catalog(owned);
			if (!opened) {
				return std::unexpected(opened.error());
			}
			auto root = observe(owned, location);
			if (!root) {
				return std::unexpected(root.error());
			}
			auto bound = bound_cycles(owned, *root);
			if (!bound) {
				return std::unexpected(bound.error());
			}
			auto subject = bound->empty() ? std::optional<core::local_cycle_subject>{ } : std::optional{ bound->front().Subject };
			if (!subject) {
				// Open Project's first open: create, bind and capture.
				const auto create  = core::create_review_cycle{ .Authority = opened->Authority, .Title = application::default_cycle_title(location), .Scope = { } };
				auto       created = checked(owned.await<application::cycle_write_receipt>([&](auto aDone) { return owned.runtime().create(owned.operation(), create, std::move(aDone)); }));
				if (!created) {
					return std::unexpected(created.error());
				}
				subject = core::local_cycle_subject{ .Authority = opened->Authority, .Cycle = created->Cycle, .Generation = 1 };
				owned.runtime().select(core::review_subject{ *subject });
				auto bind    = core::review_cycle_change{ core::bind_cycle_workspace{ .Root = *root } };
				auto binding = checked(owned.await<application::cycle_write_receipt>([&](auto aDone) { return owned.runtime().apply_selected(owned.operation(), created->Revision, std::move(bind), std::move(aDone)); }));
				if (!binding) {
					return std::unexpected(binding.error());
				}
			}
			owned.runtime().select(core::review_subject{ *subject });
			auto state = load_selected(owned);
			if (!state) {
				return std::unexpected(state.error());
			}
			const auto created = bound->empty();
			if (!state->Latest && state->Workspace) {
				if (auto captured = capture_now(owned, *state, false); !captured) {
					auto error = captured.error();
					error.Detail.insert("cycle", wire(subject->Cycle));
					error.Next = "bpr cycle capture " + core::cycle_wire_name(subject->Cycle);
					return std::unexpected(std::move(error));
				}
				state = load_selected(owned);
				if (!state) {
					return std::unexpected(state.error());
				}
			}
			auto data       = cycle_json(*state);
			data["created"] = created;
			return output{ data, (created ? "Created " : "Opened ") + cycle_words(*state) + "Saved locally; nothing is sent or published.\n", true };
		}

		auto target_json(const core::cycle_feedback_target& aTarget, const std::optional<core::cycle_line_span>& aLines) -> QJsonObject
		{
			if (const auto* range = std::get_if<core::cycle_source_anchor>(&aTarget)) {
				auto value = QJsonObject{ { "kind", "range" }, { "path", text(range->Path) }, { "revision", wire(range->Revision) }, { "start_byte", decimal(range->Start) }, { "end_byte", decimal(range->End) } };
				if (aLines) {
					value["first_line"] = decimal(aLines->First);
					value["last_line"]  = decimal(aLines->Last);
				}
				return value;
			}
			if (const auto* file = std::get_if<core::cycle_file_target>(&aTarget)) {
				return { { "kind", "file" }, { "path", text(file->Path) }, { "revision", wire(file->Revision) } };
			}
			if (const auto* module = std::get_if<core::cycle_module_target>(&aTarget)) {
				return { { "kind", "module" }, { "path", text(module->Path) }, { "revision", module->Revision ? QJsonValue{ wire(*module->Revision) } : QJsonValue{ } } };
			}
			return { { "kind", "cycle" } };
		}
		auto target_words(const core::cycle_feedback_target& aTarget, const std::optional<core::cycle_line_span>& aLines) -> QString
		{
			if (const auto* range = std::get_if<core::cycle_source_anchor>(&aTarget)) {
				const auto lines = !aLines ? QString{ } : aLines->First == aLines->Last ? ":" + decimal(aLines->First)
																						: ":" + decimal(aLines->First) + "-" + decimal(aLines->Last);
				return text(range->Path) + lines + " @" + wire(range->Revision);
			}
			if (const auto* file = std::get_if<core::cycle_file_target>(&aTarget)) {
				return text(file->Path) + " @" + wire(file->Revision) + " (whole file)";
			}
			if (const auto* module = std::get_if<core::cycle_module_target>(&aTarget)) {
				return text(module->Path) + " (module)";
			}
			return "the whole cycle";
		}
		auto first_line(const std::string& aBody) -> QString
		{
			auto line = text(aBody).section('\n', 0, 0).trimmed();
			return line.size() > note_preview_characters ? line.left(note_preview_characters) + "..." : line;
		}

		auto run_show(const command& aCommand) -> result<output>
		{
			auto session = session_for(aCommand);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			auto& runtime = owned.runtime();
			auto  data    = cycle_json(*cycle);
			auto  human   = cycle_words(*cycle);

			// Notes, at their current wording, where they were made.
			auto threads = std::vector<core::cycle_feedback_thread>{ };
			auto after   = std::optional<core::cycle_feedback_item_id>{ };
			do {
				auto page = checked(owned.await<application::cycle_feedback_page>([&](auto aDone) { return runtime.feedback_selected(after, std::move(aDone)); }));
				if (!page) {
					return std::unexpected(page.error());
				}
				std::ranges::move(page->Threads, std::back_inserter(threads));
				after = page->Next;
			} while (after);
			auto       files = std::map<std::pair<std::uint64_t, std::string>, std::vector<std::uint8_t>>{ };
			QJsonArray notes;
			human += threads.empty() ? QString{ "No notes yet.\n" } : "Notes (" + decimal(threads.size()) + "):\n";
			for (const auto& thread : threads) {
				auto history = std::vector<core::cycle_feedback_revision>{ };
				auto turn    = std::optional<core::private_review_turn_revision_id>{ };
				do {
					auto page = checked(owned.await<application::cycle_feedback_revision_page>([&](auto aDone) { return runtime.feedback_revisions_selected(thread.Id, turn, std::move(aDone)); }));
					if (!page) {
						return std::unexpected(page.error());
					}
					std::ranges::move(page->Revisions, std::back_inserter(history));
					turn = page->Next;
				} while (turn);
				const auto wording = std::find_if(history.rbegin(), history.rend(), [](const core::cycle_feedback_revision& aRevision) { return aRevision.Identity.Position.Turn == core::private_review_turn_id{ 1 }; });
				const auto body    = wording != history.rend() ? wording->Body : std::string{ };
				auto       lines   = std::optional<core::cycle_line_span>{ };
				if (const auto* range = std::get_if<core::cycle_source_anchor>(&thread.Primary)) {
					const auto key = std::pair{ range->Revision.value(), range->Path };
					if (!files.contains(key)) {
						auto bytes = retained_file(owned, *cycle, range->Revision, range->Path);
						files[key] = bytes ? std::move(*bytes) : std::vector<std::uint8_t>{ };
					}
					lines = core::project_lines(files[key], range->Start, range->End);
				}
				const auto lifecycle = text(core::stable_name(thread.Lineage.Lifecycle));
				notes.append(QJsonObject{ { "item", wire(thread.Id.Id) }, { "target", target_json(thread.Primary, lines) }, { "lifecycle", lifecycle }, { "version", decimal(thread.Lineage.Version) }, { "body", text(body) } });
				human += "  " + wire(thread.Id.Id) + "  " + target_words(thread.Primary, lines) + "  " + lifecycle + "  " + first_line(body) + "\n";
			}
			data["notes"] = notes;

			// Frozen rounds: immutable, each with its digest.
			auto rounds = std::vector<application::stored_cycle_round>{ };
			auto next   = std::optional<core::cycle_round_id>{ };
			do {
				auto page = checked(owned.await<application::cycle_round_page>([&](auto aDone) { return runtime.rounds_selected(next, std::move(aDone)); }));
				if (!page) {
					return std::unexpected(page.error());
				}
				std::ranges::move(page->Rounds, std::back_inserter(rounds));
				next = page->Next;
			} while (next);
			QJsonArray frozen;
			human += rounds.empty() ? QString{ "No frozen rounds.\n" } : "Frozen rounds (" + decimal(rounds.size()) + "):\n";
			for (const auto& round : rounds) {
				frozen.append(QJsonObject{ { "round", wire(round.Round.Id.Id) }, { "digest", hex(round.Round.Digest) }, { "draft_revision", decimal(round.Round.DraftRevision) } });
				human += "  " + wire(round.Round.Id.Id) + "  sha256 " + hex(round.Round.Digest).left(16) + "  from draft revision " + decimal(round.Round.DraftRevision) + "\n";
			}
			data["rounds"] = frozen;

			auto deliveries = checked(owned.await<std::vector<core::cycle_delivery_record>>([&](auto aDone) { return runtime.deliveries_selected(std::move(aDone)); }));
			if (!deliveries) {
				return std::unexpected(deliveries.error());
			}
			QJsonArray observed;
			if (!deliveries->empty()) {
				human += "Deliveries (" + decimal(deliveries->size()) + "):\n";
			}
			for (const auto& record : *deliveries) {
				const auto& seen = record.Observation;
				observed.append(QJsonObject{ { "ordinal", decimal(record.Ordinal) }, { "round", wire(seen.Round.Id) }, { "state", delivery_name(seen.State) }, { "channel", channel_name(seen.Channel) }, { "destination", seen.Destination.empty() ? QJsonValue{ } : QJsonValue{ text(seen.Destination) } }, { "bytes", seen.Output ? QJsonValue{ decimal(seen.Output->Bytes) } : QJsonValue{ } } });
				human += "  " + wire(seen.Round.Id) + " " + delivery_name(seen.State) + " via " + channel_name(seen.Channel) + (seen.Destination.empty() ? QString{ } : " as " + text(seen.Destination)) + "\n";
			}
			data["deliveries"] = observed;
			if (!deliveries->empty()) {
				human += "None of these means a round was received, read, acted on or accepted.\n";
			}

			auto responses = checked(owned.await<application::cycle_response_entries>([&](auto aDone) { return runtime.responses_selected(std::move(aDone)); }));
			if (!responses) {
				return std::unexpected(responses.error());
			}
			QJsonArray statements;
			for (const auto& [record, content] : *responses) {
				statements.append(QJsonObject{ { "response", decimal(record.Id.Id.value()) }, { "round", wire(record.Round) }, { "channel", text(core::stable_name(record.Channel)) }, { "foreign_id", text(record.ForeignId) }, { "bytes", decimal(record.Size) } });
			}
			data["responses"] = statements;
			human += "Responses: " + decimal(responses->size()) + ". A response is a recipient's statement; it verifies nothing.\n";
			return output{ data, human, true };
		}

		auto run_capture(const command& aCommand) -> result<output>
		{
			auto session = session_for(aCommand);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			auto captured = capture_now(owned, *cycle, aCommand.has("--again"));
			if (!captured) {
				return std::unexpected(captured.error());
			}
			auto state = load_selected(owned);
			if (!state) {
				return std::unexpected(state.error());
			}
			auto data         = cycle_json(*state);
			data["recorded"]  = captured->Recorded;
			const auto latest = state->Latest ? wire(state->Latest->Id.Id) : QString{ "nothing" };
			return output{ data,
						   captured->Recorded ? "Captured " + latest + " (" + decimal(state->Latest ? state->Latest->Capture.Entries : 0U) + " entries).\n"
											  : "No change since " + latest + "; nothing new was recorded. Use --again to record another observation of the same bytes.\n",
						   true };
		}

		/// Text from `--body-file`/`--goal-file`: a chosen file or explicit standard input.
		auto text_input(const QString& aPath) -> result<std::string>
		{
			auto input = read_tour_text_input(aPath == "-" ? aPath : text(chosen_path(aPath)));
			if (!input) {
				const auto& error = input.error();
				if (error.Code == "cancelled") {
					return fail("cancelled", "Text input was cancelled; nothing was changed.");
				}
				if (error.Code == "input_too_large") {
					return fail("over_limit", "Text input is limited to " + application::grouped_count(core::maximum_tour_field_bytes) + " UTF-8 bytes; nothing was changed.");
				}
				if (error.Code == "invalid_utf8") {
					return fail("invalid_input", "The text must be valid UTF-8 without NUL bytes; nothing was changed.");
				}
				return fail("invalid_input", "Select an ordinary text file, or - for standard input; nothing was changed.");
			}
			return std::move(*input);
		}

		auto run_note(const command& aCommand) -> result<output>
		{
			if (aCommand.has("--lines") && !aCommand.has("--file")) {
				return fail("invalid_input", "--lines needs --file PATH, a path inside the project.");
			}
			auto lines = std::optional<core::cycle_line_span>{ };
			if (aCommand.has("--lines")) {
				const auto parts = aCommand.value("--lines").split(':');
				const auto first = parts.size() <= 2 ? positive(parts.front()) : std::nullopt;
				const auto last  = parts.size() == 2 ? positive(parts.back()) : first;
				if (!first || !last || *last < *first) {
					return fail("invalid_input", "--lines takes FIRST:LAST or LINE, one-based and inclusive, e.g. 40:52.");
				}
				lines = core::cycle_line_span{ .First = *first, .Last = *last };
			}
			auto body = std::optional<std::string>{ };
			if (aCommand.has("--body-file")) {
				auto input = text_input(aCommand.value("--body-file"));
				if (!input) {
					return std::unexpected(input.error());
				}
				body = std::move(*input);
			} else if (!aCommand.Interactive) {
				// Never an editor, a prompt or stray standard input without a terminal.
				return fail("interaction_required", "A note needs its text: --body-file PATH, or --body-file - to read standard input.",
							"bpr cycle note" + aCommand.named() + " --body-file -");
			}

			auto session = session_for(aCommand);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto cycle = resolve(**session, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			const auto name   = core::cycle_wire_name(cycle->Subject.Cycle);
			auto       target = core::cycle_feedback_target{ core::cycle_whole_target{ } };
			if (aCommand.has("--file")) {
				if (!cycle->Latest) {
					return fail("round_not_ready", "Nothing is captured yet; capture the folder first.", "bpr cycle capture " + name);
				}
				const auto revision = cycle->Latest->Id.Id;
				const auto path     = utf8(aCommand.value("--file"));
				auto       bytes    = retained_file(**session, *cycle, revision, path);
				if (!bytes) {
					return std::unexpected(bytes.error());
				}
				if (lines) {
					const auto range  = core::line_span_bytes(*bytes, *lines);
					auto       anchor = range ? core::make_anchor(revision, path, *bytes, range->Start, range->End) : std::nullopt;
					if (!anchor) {
						return fail("invalid_input", "Lines " + std::to_string(lines->First) + "-" + std::to_string(lines->Last) + " are not in " + path + " at " + core::cycle_wire_name(revision) + ", or hold more than one note keeps (64 KiB).");
					}
					target = std::move(*anchor);
				} else {
					target = core::cycle_file_target{ .Revision = revision, .Path = path, .Blob = core::cycle_material_digest{ core::sha256(*bytes) } };
				}
			}
			if (!body) {
				// The editor runs with no lease held; the cycle is read again after.
				session->reset();
				auto edited = run_tour_editor({ });
				if (!edited) {
					const auto& error = edited.error();
					if (error.Code == "editor_configuration" && error.Exit == 3) {
						return fail("interaction_required", "No trusted editor is configured: set BPR_TOUR_EDITOR_ARGV to a JSON argv array, or VISUAL/EDITOR to one absolute executable. Or give --body-file.",
									"bpr cycle note" + aCommand.named() + " --body-file -");
					}
					return fail(error.Code == "cancelled" ? "cancelled" : "invalid_input", error.Message + " Nothing was saved.");
				}
				body = edited->Text;
				cleanup_tour_editor(*edited);
				session = session_for(aCommand);
				if (!session) {
					return std::unexpected(session.error());
				}
				cycle = resolve(**session, aCommand);
				if (!cycle) {
					return std::unexpected(cycle.error());
				}
			}
			if (text(*body).trimmed().isEmpty()) {
				return fail("invalid_input", "A note needs some text; nothing was saved.");
			}
			if (body->size() > core::maximum_private_reviewer_body_bytes) {
				return fail("over_limit", "A note is limited to " + application::grouped_count(core::maximum_private_reviewer_body_bytes) + " UTF-8 bytes; this one is " + application::grouped_count(body->size()) + ". Nothing was saved.");
			}
			auto& owned   = **session;
			auto  note    = core::begin_cycle_feedback{ .Primary = target, .Body = std::move(*body) };
			auto  receipt = checked(owned.await<application::cycle_write_receipt>([&](auto aDone) { return owned.runtime().begin_feedback_selected(owned.operation(), cycle->Revision, std::move(note), std::move(aDone)); }));
			if (!receipt) {
				return std::unexpected(receipt.error());
			}
			const auto item = core::cycle_feedback_item_id{ receipt->Allocated };
			return output{ { { "cycle", text(name) }, { "item", wire(item) }, { "target", target_json(target, lines) }, { "revision", decimal(receipt->Revision) } },
						   "Noted " + wire(item) + " on " + target_words(target, lines) + " in " + text(name) + ". Saved locally; nothing was sent.\n",
						   true };
		}

		/// The composition edit `review` names, if any.
		auto composition_edit(const command& aCommand, std::optional<std::string> aGoal) -> result<std::optional<core::cycle_compose_edit>>
		{
			for (const auto& option : { QStringLiteral("--include"), QStringLiteral("--exclude"), QStringLiteral("--earlier"), QStringLiteral("--later") }) {
				if (!aCommand.has(option)) {
					continue;
				}
				const auto item = core::parse_item_name(utf8(aCommand.value(option)));
				if (!item) {
					return fail("invalid_input", utf8(option) + " takes a note as I<n>, as bpr cycle show lists it.");
				}
				if (option == "--include" || option == "--exclude") {
					return core::cycle_compose_edit{ core::include_round_item{ .Item = *item, .Include = option == "--include" } };
				}
				return core::cycle_compose_edit{ core::move_round_item{ .Item = *item, .Earlier = option == "--earlier" } };
			}
			if (aCommand.has("--action")) {
				const auto action = aCommand.value("--action");
				for (const auto value : { core::cycle_round_action::discuss, core::cycle_round_action::plan, core::cycle_round_action::implement }) {
					if (action == text(core::stable_name(value))) {
						return core::cycle_compose_edit{ core::set_round_action{ .Action = value } };
					}
				}
				return fail("invalid_input", "--action takes discuss, plan or implement.");
			}
			if (aGoal) {
				return core::cycle_compose_edit{ core::set_round_goal{ .Goal = std::move(*aGoal) } };
			}
			return std::optional<core::cycle_compose_edit>{ };
		}

		auto compose(cycle_session& aSession) -> result<application::cycle_share_draft>
		{
			auto draft = aSession.await<application::cycle_share_draft>([&](auto aDone) { return aSession.runtime().open_share_selected(aSession.operation(), aSession.operation(), std::move(aDone)); });
			if (!draft && draft.error().Failure == application::cycle_store_failure::refused && draft.error().Refusal == core::cycle_refusal::invalid_target) {
				return fail("round_not_ready", "Nothing is captured yet; capture the folder before composing a round.");
			}
			return checked(std::move(draft));
		}

		auto draft_json(const core::review_cycle_state& aCycle, const application::cycle_share_draft& aDraft) -> QJsonObject
		{
			const auto& preview = aDraft.Preview;
			const auto& select  = preview.Selection;
			QJsonArray  included;
			for (const auto& item : select.Items) {
				included.append(wire(item.Item));
			}
			QJsonArray leftOut;
			for (const auto& note : aDraft.Notes) {
				const auto in = std::ranges::find(select.Items, note.Item, &core::cycle_round_item_selection::Item) != select.Items.end();
				if (!in && note.Lifecycle == core::private_thread_lifecycle::open) {
					leftOut.append(wire(note.Item));
				}
			}
			auto suggestions = std::uint64_t{ 0 };
			auto sources     = std::uint64_t{ 0 };
			if (preview.Content) {
				for (const auto& item : preview.Content->Payload.Items) {
					suggestions += item.Suggestions.size();
				}
				sources = preview.Content->Payload.Sources.size();
			}
			return {
				{ "cycle", wire(aCycle.Subject.Cycle) },
				{ "draft", decimal(preview.Draft.Id.Id.value()) },
				{ "draft_revision", decimal(preview.Draft.Revision) },
				{ "round", wire(preview.Round) },
				{ "frozen", preview.Frozen.has_value() },
				{ "amends", !preview.Frozen && preview.Prior ? QJsonValue{ wire(preview.Prior->Round.Id.Id) } : QJsonValue{ } },
				{ "source", wire(select.Source) },
				{ "action", text(core::stable_name(select.Action)) },
				{ "goal", text(select.Goal) },
				{ "items", included },
				{ "left_out", leftOut },
				{ "suggestions", decimal(suggestions) },
				{ "sources", decimal(sources) },
				{ "digest", preview.Content ? QJsonValue{ hex(preview.Content->Digest) } : QJsonValue{ } },
				{ "refusal", preview.Refusal ? QJsonValue{ text(core::stable_name(preview.Refusal->Problem)) } : QJsonValue{ } },
				{ "document_bytes", decimal(aDraft.DocumentBytes) },
				{ "document", aDraft.Document ? QJsonValue{ text(*aDraft.Document) } : QJsonValue{ } },
			};
		}
		auto draft_words(const application::cycle_share_draft& aDraft, const QJsonObject& aData, const std::string& aName) -> QString
		{
			const auto& preview = aDraft.Preview;
			auto        words   = text(aName) + ": the next round is " + aData["round"].toString() + " (draft " + aData["draft"].toString() + ", revision " + aData["draft_revision"].toString() + ").\n";
			QStringList items;
			for (const auto& item : aData["items"].toArray()) {
				items.push_back(item.toString());
			}
			QStringList leftOut;
			for (const auto& item : aData["left_out"].toArray()) {
				leftOut.push_back(item.toString());
			}
			words += "Asks to " + aData["action"].toString() + " " + decimal(static_cast<std::uint64_t>(items.size())) + (items.size() == 1 ? " note" : " notes") + (items.isEmpty() ? QString{ } : ": " + items.join(", ")) + ".";
			words += leftOut.isEmpty() ? QString{ "\n" } : " Left out: " + leftOut.join(", ") + ".\n";
			if (!aData["goal"].toString().isEmpty()) {
				words += "Goal: " + aData["goal"].toString() + "\n";
			}
			if (preview.Frozen) {
				words += "This revision is frozen as " + wire(preview.Frozen->Round.Id.Id) + "; freezing again reuses it.\n";
			} else if (preview.Prior) {
				words += "Freezing it makes " + aData["round"].toString() + ", which amends " + wire(preview.Prior->Round.Id.Id) + "; that round stays as it was.\n";
			}
			if (preview.Refusal) {
				words += "It cannot be frozen as it stands (" + text(core::stable_name(preview.Refusal->Problem)) + ").\n";
				return words;
			}
			words += aData["suggestions"].toString() + " suggestions, " + aData["sources"].toString() + " sources; " + text(application::grouped_count(aDraft.DocumentBytes)) + " bytes; sha256 " + aData["digest"].toString().left(16) + "\n";
			words += "Freeze exactly this: bpr cycle freeze " + text(aName) + " --expected-revision " + aData["draft_revision"].toString() + "\n";
			if (aDraft.Document) {
				words += "\n" + text(*aDraft.Document);
				if (!aDraft.Document->ends_with('\n')) {
					words += "\n";
				}
			} else {
				words += "The document is too large to show here; export it to read it.\n";
			}
			return words;
		}

		auto run_review(const command& aCommand) -> result<output>
		{
			auto goal = std::optional<std::string>{ };
			if (aCommand.has("--goal-file")) {
				auto input = text_input(aCommand.value("--goal-file"));
				if (!input) {
					return std::unexpected(input.error());
				}
				goal = std::move(*input);
			}
			const auto edits = std::ranges::count_if(QStringList{ "--include", "--exclude", "--earlier", "--later", "--action", "--goal-file" }, [&](const QString& aOption) { return aCommand.has(aOption); });
			if (edits > 1) {
				return fail("invalid_input", "Make one change to the draft per command.");
			}
			auto edit = composition_edit(aCommand, std::move(goal));
			if (!edit) {
				return std::unexpected(edit.error());
			}
			auto expected = std::optional<std::uint64_t>{ };
			if (aCommand.has("--expected-revision")) {
				expected = positive(aCommand.value("--expected-revision"));
				if (!expected) {
					return fail("invalid_input", "--expected-revision takes the draft revision review reported.");
				}
			}
			if (*edit && !expected && !aCommand.Interactive) {
				return fail("interaction_required", "A scripted change to the draft names the revision it read: --expected-revision N.", "bpr cycle review" + aCommand.named() + " --json");
			}
			auto session = session_for(aCommand);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			const auto name  = core::cycle_wire_name(cycle->Subject.Cycle);
			auto       draft = compose(owned);
			if (!draft) {
				auto error = draft.error();
				error.Next = error.Code == "round_not_ready" ? "bpr cycle capture " + name : error.Next;
				return std::unexpected(std::move(error));
			}
			auto changed = false;
			if (*edit) {
				const auto shown = draft->Preview.Draft.Revision;
				if (expected && *expected != shown) {
					return fail("stale_revision", "The draft is at revision " + std::to_string(shown) + ", not " + std::to_string(*expected) + "; read it again. Nothing was changed.",
								"bpr cycle review " + name + " --json", QJsonObject{ { "draft_revision", decimal(shown) } });
				}
				auto edited = checked(owned.await<application::cycle_share_draft>([&](auto aDone) { return owned.runtime().edit_share_selected(owned.operation(), draft->Preview.Draft.Id, shown, **edit, std::move(aDone)); }));
				if (!edited) {
					return std::unexpected(edited.error());
				}
				changed = edited->Preview.Draft.Revision != shown;
				draft   = std::move(*edited);
			}
			auto data = draft_json(*cycle, *draft);
			if (*edit) {
				data["changed"] = changed;
			}
			auto human = draft_words(*draft, data, name);
			if (*edit && !changed) {
				human = "That changed nothing: the note is not open, or is already there.\n" + human;
			}
			return output{ data, human, true };
		}

		auto run_freeze(const command& aCommand) -> result<output>
		{
			auto expected = std::optional<std::uint64_t>{ };
			if (aCommand.has("--expected-revision")) {
				expected = positive(aCommand.value("--expected-revision"));
				if (!expected) {
					return fail("invalid_input", "--expected-revision takes the draft revision review reported.");
				}
			}
			auto session = session_for(aCommand);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto cycle = resolve(**session, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			const auto name  = core::cycle_wire_name(cycle->Subject.Cycle);
			auto       draft = compose(**session);
			if (!draft) {
				return std::unexpected(draft.error());
			}
			if (!expected) {
				const auto shown = draft->Preview.Draft.Revision;
				const auto next  = "bpr cycle freeze " + name + " --expected-revision " + std::to_string(shown);
				if (!aCommand.Interactive) {
					// EOF or a pipe is never an approval.
					return fail("interaction_required", "Freezing names the draft revision you read: --expected-revision " + std::to_string(shown) + ".", next,
								QJsonObject{ { "draft_revision", decimal(shown) }, { "round", wire(draft->Preview.Round) } });
				}
				const auto data = draft_json(*cycle, *draft);
				std::cout << neutral_terminal_text(draft_words(*draft, data, name)).toUtf8().constData()
						  << "\nType freeze to freeze revision " << shown << " as " << core::cycle_wire_name(draft->Preview.Round) << ", or press Enter to cancel: " << std::flush;
				// No lease is held while a person answers.
				session->reset();
				auto answer = read_tour_terminal_line();
				if (!answer || *answer != "freeze") {
					return fail("cancelled", "Nothing was frozen.");
				}
				expected = shown;
				session  = session_for(aCommand);
				if (!session) {
					return std::unexpected(session.error());
				}
				cycle = resolve(**session, aCommand);
				if (!cycle) {
					return std::unexpected(cycle.error());
				}
				draft = compose(**session);
				if (!draft) {
					return std::unexpected(draft.error());
				}
			}
			auto&      owned = **session;
			const auto retry = "bpr cycle review " + name + " --json";
			if (draft->Preview.Draft.Revision != *expected) {
				return fail("stale_revision", "The draft is at revision " + std::to_string(draft->Preview.Draft.Revision) + ", not " + std::to_string(*expected) + "; read it again. Nothing was frozen.", retry,
							QJsonObject{ { "draft_revision", decimal(draft->Preview.Draft.Revision) } });
			}
			auto request = application::cycle_round_freeze_request{
				.Operation             = owned.operation(),
				.Draft                 = draft->Preview.Draft.Id,
				.ExpectedDraftRevision = *expected,
				.Editor                = { .ShownRevision = *expected },
			};
			auto frozen = checked(owned.await<application::cycle_round_freeze_result>([&](auto aDone) { return owned.runtime().freeze_round_selected(std::move(request), std::move(aDone)); }));
			if (!frozen) {
				return std::unexpected(frozen.error());
			}
			if (!frozen->Frozen) {
				return std::unexpected(round_failure(frozen->Refusal.value_or(core::cycle_round_refusal{ }), retry));
			}
			const auto& round = frozen->Frozen->Round.Round;
			const auto  items = frozen->Frozen->Content.Payload.Items.size();
			return output{ { { "cycle", text(name) }, { "round", wire(round.Id.Id) }, { "digest", hex(round.Digest) }, { "draft_revision", decimal(round.DraftRevision) }, { "items", decimal(items) }, { "reused", frozen->Frozen->Reused } },
						   (frozen->Frozen->Reused ? "Already frozen as " : "Froze ") + wire(round.Id.Id) + " (" + decimal(items) + (items == 1U ? " note" : " notes") + ", sha256 " + hex(round.Digest).left(16)
							   + "). Nothing was sent.\nExport it: bpr cycle export " + text(name) + " --round " + wire(round.Id.Id) + " --output PATH\n",
						   true };
		}

		auto run_export(const command& aCommand) -> result<output>
		{
			const auto round = core::parse_round_name(utf8(aCommand.value("--round")));
			if (!round) {
				return fail("invalid_input", "Name the frozen round: --round F<n>.");
			}
			if (!aCommand.has("--output")) {
				return fail("invalid_input", "Name where it goes: --output PATH, or --output - for standard output.");
			}
			const auto format = aCommand.Values.value("--format", "markdown");
			if (format != "markdown" && format != "bundle") {
				return fail("invalid_input", "--format takes markdown or bundle.");
			}
			const auto terminal = aCommand.value("--output") == "-";
			if (terminal && (format == "bundle" || aCommand.Json)) {
				return fail("invalid_input", format == "bundle" ? "A bundle is saved to a file, not printed." : "--output - prints the document itself, so it cannot share standard output with --json.");
			}
			auto confirmed = std::optional<core::cycle_material_digest>{ };
			if (aCommand.has("--replace")) {
				confirmed = digest_of(aCommand.value("--replace"));
				if (!confirmed || terminal) {
					return fail("invalid_input", "--replace takes the lowercase sha256 of the file to replace, as the refusal reported it.");
				}
			}
			const auto request = application::cycle_round_export_request{
				.Round   = *round,
				.Channel = terminal ? core::cycle_disclosure_channel::terminal_output : core::cycle_disclosure_channel::local_file,
				.Format  = format == "bundle" ? core::cycle_round_file_kind::bundle : core::cycle_round_file_kind::markdown,
				.Policy  = { },
			};
			auto session = session_for(aCommand);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto cycle = resolve(**session, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			const auto name = core::cycle_wire_name(cycle->Subject.Cycle);
			if (round->value() >= cycle->Next.Round) {
				return fail("not_found", core::cycle_wire_name(*round) + " is not a frozen round of " + name + ".", "bpr cycle show " + name);
			}
			if (terminal) {
				auto& owned    = **session;
				auto  exported = checked(owned.await<application::cycle_share_output>([&](auto aDone) { return owned.runtime().export_round_selected(request, std::move(aDone)); }));
				if (!exported) {
					return std::unexpected(exported.error());
				}
				if (exported->Decision != core::cycle_authorization::allowed) {
					return std::unexpected(policy_failure(exported->Decision, request.Channel));
				}
				if (exported->Limit || !exported->Round) {
					return std::unexpected(limit_failure(exported->Limit.value_or(core::cycle_round_output_limit{ })));
				}
				if (interactive_terminal()) {
					// A terminal shows controls neutralized: not the exact bytes, so not recorded as retrieved.
					std::cout << neutral_terminal_text(text(exported->Bytes)).toUtf8().constData() << std::flush;
					return output{ { }, { }, true };
				}
				std::cout.write(exported->Bytes.data(), static_cast<std::streamsize>(exported->Bytes.size()));
				std::cout.flush();
				const auto state       = std::cout.good() ? core::cycle_delivery_state::retrieved : core::cycle_delivery_state::failed;
				auto       observation = application::delivery_observation(*exported->Round, state, core::cycle_disclosure_channel::terminal_output,
																		   state == core::cycle_delivery_state::retrieved ? std::string_view{ exported->Bytes } : std::string_view{ }, exported->Format);
				auto       recorded    = checked(owned.await<application::cycle_write_receipt>([&](auto aDone) { return owned.runtime().record_delivery_selected(owned.operation(), std::move(observation), std::move(aDone)); }));
				if (!recorded) {
					return std::unexpected(recorded.error());
				}
				if (state == core::cycle_delivery_state::failed) {
					return fail("uncertain", "Standard output refused part of the document; it may be incomplete.");
				}
				return output{ { }, { }, true };
			}

			const auto path = chosen_path(aCommand.value("--output"));
			const auto file = utf8(QFileInfo{ text(path) }.fileName());
			for (;;) {
				auto& owned       = **session;
				auto  destination = std::shared_ptr<application::cycle_export_destination>{ adapters::workspace::make_local_export_destination(utf8_path(path)) };
				auto  saved       = checked(owned.await<application::cycle_file_share>([&](auto aDone) { return owned.runtime().export_round_to_file_selected(request, destination, file, confirmed, owned.operation(), std::move(aDone)); }));
				if (!saved) {
					return std::unexpected(saved.error());
				}
				if (saved->Output.Decision != core::cycle_authorization::allowed) {
					return std::unexpected(policy_failure(saved->Output.Decision, request.Channel));
				}
				if (saved->Output.Limit || !saved->Output.Round) {
					return std::unexpected(limit_failure(saved->Output.Limit.value_or(core::cycle_round_output_limit{ })));
				}
				if (saved->Failure) {
					const auto code = *saved->Failure == application::cycle_export_failure::exists || *saved->Failure == application::cycle_export_failure::changed ? "conflict"
						: *saved->Failure == application::cycle_export_failure::not_regular                                                                         ? "invalid_input"
						: *saved->Failure == application::cycle_export_failure::cancelled                                                                           ? "cancelled"
																																									: "destination_unavailable";
					return fail(code, application::cycle_export_words(*saved->Failure));
				}
				const auto bytes = application::grouped_count(saved->Output.Bytes.size());
				if (saved->Save && saved->Save->Action == core::cycle_round_save_action::conflict && saved->Save->Existing) {
					const auto& existing = *saved->Save->Existing;
					const auto  next     = "bpr cycle export " + name + " --round " + core::cycle_wire_name(*round) + " --output " + utf8(aCommand.value("--output")) + " --replace " + utf8(hex(existing.Digest));
					if (!aCommand.Interactive) {
						return fail("conflict", file + " already holds other content (" + application::grouped_count(existing.Size) + " bytes); nothing was written. Replace exactly those bytes with --replace.", next,
									QJsonObject{ { "existing_digest", hex(existing.Digest) }, { "existing_bytes", decimal(existing.Size) } });
					}
					std::cout << file << " already holds other content (" << application::grouped_count(existing.Size) << " bytes).\nType replace to replace exactly that file, or press Enter to keep it: " << std::flush;
					session->reset();
					auto answer = read_tour_terminal_line();
					if (!answer || *answer != "replace") {
						return fail("cancelled", "Nothing was written; " + file + " was kept.");
					}
					confirmed = existing.Digest;
					session   = session_for(aCommand);
					if (!session) {
						return std::unexpected(session.error());
					}
					if (cycle = resolve(**session, aCommand); !cycle) {
						return std::unexpected(cycle.error());
					}
					continue;
				}
				const auto shown = core::cycle_wire_name(*round);
				const auto same  = saved->Save && saved->Save->Action == core::cycle_round_save_action::already_saved;
				return output{ { { "cycle", text(name) }, { "round", text(shown) }, { "format", format }, { "path", text(path) }, { "bytes", decimal(saved->Output.Bytes.size()) }, { "digest", hex(saved->Save->Written.Digest) }, { "written", !same } },
							   same ? text(file) + " already holds " + text(shown) + " exactly (" + text(bytes) + " bytes); nothing was written.\n"
									: "Saved " + text(shown) + " as " + text(file) + " (" + text(bytes) + " bytes). Saved is not received or read.\n",
							   true };
			}
		}

		auto run_compare(const command& aCommand) -> result<output>
		{
			auto revisionOf = [&](const QString& aOption) -> result<std::optional<core::cycle_source_revision_id>> {
				if (!aCommand.has(aOption)) {
					return std::optional<core::cycle_source_revision_id>{ };
				}
				const auto revision = core::parse_revision_name(utf8(aCommand.value(aOption)));
				if (!revision) {
					return fail("invalid_input", utf8(aOption) + " takes a capture as R<n>.");
				}
				return revision;
			};
			auto from = revisionOf("--from");
			auto to   = revisionOf("--to");
			if (!from || !to) {
				return std::unexpected(!from ? from.error() : to.error());
			}
			auto session = session_for(aCommand);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			if (!cycle->Latest) {
				return fail("round_not_ready", "Nothing is captured yet.", "bpr cycle capture " + core::cycle_wire_name(cycle->Subject.Cycle));
			}
			const auto right = to->value_or(cycle->Latest->Id.Id);
			for (const auto revision : { std::optional{ right }, *from }) {
				if (revision) {
					if (auto known = require_revision(*cycle, *revision); !known) {
						return std::unexpected(known.error());
					}
				}
			}
			const auto pair = core::cycle_comparison_pair{
				.Left  = *from ? std::optional{ core::cycle_source_revision_ref{ .Cycle = cycle->Subject.Cycle, .Id = **from } } : std::nullopt,
				.Right = { .Cycle = cycle->Subject.Cycle, .Id = right },
			};
			auto data = QJsonObject{ { "cycle", wire(cycle->Subject.Cycle) }, { "from", *from ? QJsonValue{ wire(**from) } : QJsonValue{ } }, { "to", wire(right) } };
			if (aCommand.has("--path")) {
				const auto path = utf8(aCommand.value("--path"));
				auto       file = checked(owned.await<application::cycle_file_observation>([&](auto aDone) { return owned.runtime().compare_file_selected(pair, path, { }, std::move(aDone)); }));
				if (!file) {
					return std::unexpected(file.error());
				}
				if (file->Status != application::review_material_status::available) {
					return fail(file->Status == application::review_material_status::missing ? "not_found" : "source_unavailable", "The retained bytes of " + path + " could not be read; nothing was substituted.");
				}
				auto rows = std::vector<core::cycle_diff_row>{ };
				if (file->Diff) {
					rows = core::diff_rows(*file->Diff);
				} else {
					auto line = std::uint32_t{ 0 };
					for (const auto& value : text(std::string_view{ reinterpret_cast<const char*>(file->Source.data()), file->Source.size() }).split('\n')) {
						rows.push_back({ .Kind = core::cycle_diff_row_kind::context, .RightLine = ++line, .Text = utf8(value) });
					}
					if (!rows.empty() && rows.back().Text.empty()) {
						rows.pop_back();
					}
				}
				const auto truncated = rows.size() > maximum_rows;
				QJsonArray listed;
				QString    human = text(path) + (*from ? "  " + wire(**from) + " -> " + wire(right) : "  @" + wire(right)) + (file->Path && file->Path->Change ? "  " + change_name(*file->Path->Change) : QString{ }) + "\n";
				for (std::size_t index = 0; index < std::min(rows.size(), maximum_rows); ++index) {
					const auto& row  = rows[index];
					const auto  mark = row.Kind == core::cycle_diff_row_kind::added ? "+" : row.Kind == core::cycle_diff_row_kind::removed ? "-"
																																		   : " ";
					const auto  at   = row.RightLine ? *row.RightLine : row.LeftLine.value_or(0U);
					listed.append(QJsonObject{ { "kind", row.Kind == core::cycle_diff_row_kind::added ? "added" : row.Kind == core::cycle_diff_row_kind::removed ? "removed"
																																								 : "context" },
											   { "left_line", row.LeftLine ? QJsonValue{ decimal(*row.LeftLine) } : QJsonValue{ } },
											   { "right_line", row.RightLine ? QJsonValue{ decimal(*row.RightLine) } : QJsonValue{ } },
											   { "text", text(row.Text) } });
					human += QString{ mark } + QStringLiteral("%1 | ").arg(at, 5) + text(row.Text) + "\n";
				}
				if (truncated) {
					human += "... " + decimal(rows.size() - maximum_rows) + " more rows not shown.\n";
				}
				data["path"]       = text(path);
				data["change"]     = file->Path && file->Path->Change ? QJsonValue{ change_name(*file->Path->Change) } : QJsonValue{ };
				data["rows"]       = listed;
				data["rows_total"] = decimal(rows.size());
				data["truncated"]  = truncated;
				return output{ data, human, !truncated };
			}
			const auto cursor = aCommand.has("--cursor") ? std::optional{ utf8(aCommand.value("--cursor")) } : std::nullopt;
			auto       page   = checked(owned.await<application::cycle_comparison_page_observation>([&](auto aDone) { return owned.runtime().compare_selected(pair, cursor, page_entries, aCommand.has("--changed-only"), std::move(aDone)); }));
			if (!page) {
				return std::unexpected(page.error());
			}
			if (!page->Page) {
				return fail("source_unavailable", "The captured file list could not be read; nothing was substituted.");
			}
			QJsonArray entries;
			QString    human = wire(cycle->Subject.Cycle) + (*from ? "  " + wire(**from) + " -> " + wire(right) : "  files of " + wire(right)) + "\n";
			for (const auto& entry : page->Page->Entries) {
				const auto change = entry.Change ? change_name(*entry.Change) : state_name(entry.Right.State);
				entries.append(QJsonObject{ { "path", text(entry.Path) }, { "change", change } });
				human += "  " + change.leftJustified(10) + " " + text(entry.Path) + "\n";
			}
			if (page->Page->Next) {
				human += "More: bpr cycle compare" + text(aCommand.named()) + " --cursor " + text(*page->Page->Next) + "\n";
			}
			data["entries"] = entries;
			data["next"]    = page->Page->Next ? QJsonValue{ text(*page->Page->Next) } : QJsonValue{ };
			return output{ data, human, !page->Page->Next };
		}

		auto run_response_import(const command& aCommand) -> result<output>
		{
			const auto round = core::parse_round_name(utf8(aCommand.value("--round")));
			if (!round) {
				return fail("invalid_input", "Name the frozen round the response answers: --round F<n>.");
			}
			if (!aCommand.has("--file")) {
				return fail("invalid_input", "Name the response: --file PATH, or --file - for standard input.");
			}
			auto reader = application::cycle_response_file_reader{ };
			if (aCommand.value("--file") == "-") {
				// Read now, explicitly asked for, before any lease is taken.
				QFile input;
				if (!input.open(stdin, QIODevice::ReadOnly, QFileDevice::DontCloseHandle)) {
					return fail("invalid_input", "Standard input could not be read.");
				}
				const auto bytes = input.read(static_cast<qint64>(core::maximum_response_document_bytes) + 1);
				if (input.error() != QFileDevice::NoError) {
					return fail("invalid_input", "Standard input could not be read completely.");
				}
				if (bytes.size() > static_cast<qsizetype>(core::maximum_response_document_bytes)) {
					return fail("over_limit", "A response is limited to " + application::grouped_count(core::maximum_response_document_bytes) + " bytes; nothing was added.");
				}
				reader = [document = std::vector<std::uint8_t>(bytes.begin(), bytes.end())](std::stop_token) -> std::expected<std::vector<std::uint8_t>, application::cycle_export_failure> { return document; };
			} else {
				reader = [path = utf8_path(chosen_path(aCommand.value("--file")))](std::stop_token aStop) {
					return adapters::workspace::read_chosen_file(path, core::maximum_response_document_bytes, std::move(aStop));
				};
			}
			auto session = session_for(aCommand);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			const auto name = core::cycle_wire_name(cycle->Subject.Cycle);
			if (round->value() >= cycle->Next.Round) {
				return fail("not_found", core::cycle_wire_name(*round) + " is not a frozen round of " + name + ".", "bpr cycle show " + name);
			}
			auto imported = checked(owned.await<application::cycle_response_result>([&](auto aDone) { return owned.runtime().import_response_selected(owned.operation(), *round, std::move(reader), std::move(aDone)); }));
			if (!imported) {
				return std::unexpected(imported.error());
			}
			if (!imported->Receipt) {
				const auto words = application::cycle_response_words(*imported);
				if (imported->Read) {
					return fail("source_unavailable", words);
				}
				if (imported->Refusal) {
					using problem   = core::cycle_response_problem;
					const auto kind = imported->Refusal->Problem;
					return fail(kind == problem::stale_item ? "stale_revision" : kind == problem::reused_identifier || kind == problem::wrong_cycle || kind == problem::wrong_round || kind == problem::wrong_digest ? "conflict"
																																																					 : "invalid_input",
								words, { }, QJsonObject{ { "problem", text(core::stable_name(kind)) } });
				}
				return fail("invalid_input", words, { }, imported->Decode ? QJsonObject{ { "problem", text(core::stable_name(imported->Decode->Problem)) } } : QJsonObject{ });
			}
			const auto& receipt = *imported->Receipt;
			// The same response under the same identifier is appended once.
			const auto again = receipt.Replayed || !receipt.Changed;
			return output{ { { "cycle", text(name) }, { "round", wire(*round) }, { "response", decimal(receipt.Allocated) }, { "appended", !again } },
						   text((again ? "That response was already added to " : "Added the response to ") + core::cycle_wire_name(*round) + " of " + name
								+ ". It is the recipient's statement: it changes no note and verifies nothing.\n"),
						   true };
		}

		auto perform(const command& aCommand) -> result<output>
		{
			if (aCommand.Name == "capabilities") {
				return capabilities();
			}
			if (aCommand.Name == "list") {
				return run_list(aCommand);
			}
			if (aCommand.Name == "open") {
				return run_open(aCommand);
			}
			if (aCommand.Name == "show") {
				return run_show(aCommand);
			}
			if (aCommand.Name == "capture") {
				return run_capture(aCommand);
			}
			if (aCommand.Name == "note") {
				return run_note(aCommand);
			}
			if (aCommand.Name == "review") {
				return run_review(aCommand);
			}
			if (aCommand.Name == "freeze") {
				return run_freeze(aCommand);
			}
			if (aCommand.Name == "export") {
				return run_export(aCommand);
			}
			if (aCommand.Name == "compare") {
				return run_compare(aCommand);
			}
			return run_response_import(aCommand);
		}
	}

	auto run_cycle_cli(const QStringList& aArguments) -> int
	{
		const auto interrupts = tour_signal_scope{ };
		auto       parsed     = parse(aArguments);
		const auto json       = aArguments.contains("--json");
		const auto name       = parsed ? parsed->Name : QStringLiteral("invalid");
		auto       value      = [&]() -> result<output> {
			if (!parsed) {
				return std::unexpected(parsed.error());
			}
			try {
				return perform(*parsed);
			} catch (...) {
				return fail("operation_failed", "The command failed; no completion is claimed. Read the cycle again before repeating it.");
			}
		}();
		if (json) {
			auto error = QJsonValue{ };
			if (!value) {
				const auto& problem = value.error();
				auto        object  = QJsonObject{ { "code", text(problem.Code) }, { "message", text(problem.Message) }, { "next", problem.Next.empty() ? QJsonValue{ } : QJsonValue{ text(problem.Next) } } };
				if (!problem.Detail.isEmpty()) {
					object["detail"] = problem.Detail;
				}
				error = object;
			}
			const auto envelope = QJsonObject{
				{ "schema", result_schema },
				{ "command", "cycle " + name },
				{ "operation_id", parsed ? parsed->Operation : QString{ } },
				{ "outcome", value ? "ok" : "error" },
				{ "complete", value && value->Complete },
				{ "data", value ? QJsonValue{ value->Data } : QJsonValue{ } },
				{ "error", error },
			};
			std::cout << QJsonDocument{ envelope }.toJson(QJsonDocument::Compact).constData() << '\n';
		} else if (value) {
			std::cout << neutral_terminal_text(value->Human).toUtf8().constData();
		} else {
			std::cerr << neutral_terminal_text(text(value.error().Message)).toUtf8().constData() << '\n';
			if (!value.error().Next.empty()) {
				std::cerr << "Next: " << neutral_terminal_text(text(value.error().Next)).toUtf8().constData() << '\n';
			}
		}
		std::cout.flush();
		return value ? 0 : exit_code(value.error().Code);
	}
}
