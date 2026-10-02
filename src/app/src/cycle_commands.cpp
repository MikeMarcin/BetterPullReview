#include "cycle_commands.hpp"
#include "bpr/adapters/workspace/local_export_destination.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/review_cycle_words.hpp"
#include "bpr/core/review_cycle_bundle.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/review_cycle_policy.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/core/review_cycle_retrieval.hpp"
#include "bpr/core/review_cycle_round.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
#include "bpr/core/review_cycle_views.hpp"
#include "bpr/core/review_tour.hpp"
#include "bpr/core/sha256.hpp"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>
#include <charconv>
#include <map>
#include <span>
#include <utility>
namespace bpr::app {
	namespace {
		using failure = cycle_failure;
		using output  = cycle_output;
		template <typename T>
		using result  = cycle_result<T>;
		using command = cycle_invocation;

		constexpr std::size_t page_entries = cycle_page_entries;
		constexpr std::size_t maximum_rows = cycle_file_rows;
		/// The longest note text `show` repeats per note.
		constexpr qsizetype note_preview_characters = 120;

		auto fail(std::string aCode, std::string aMessage, std::string aNext = { }, QJsonObject aDetail = { }) -> std::unexpected<failure>
		{
			return std::unexpected(failure{ std::move(aCode), std::move(aMessage), std::move(aNext), std::move(aDetail) });
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
			case cycle_store_failure::operation_reused: return { "conflict", "That operation ID was already used for another request; nothing was applied again. Read the cycle before repeating it.", { }, { } };
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

		// ---- Sessions, cycles and the working folder.

		auto utf8_path(const std::string& aPath) -> std::filesystem::path
		{
			return native_filesystem_path(text(aPath));
		}
		/// A path the caller made absolute; the owner has no working folder of the caller's.
		auto absolute_path(const QString& aPath, const char* aWhat) -> result<std::string>
		{
			if (!QDir::isAbsolutePath(aPath)) {
				return fail("invalid_input", std::string{ aWhat } + " must be an absolute path.");
			}
			return utf8(QDir::cleanPath(aPath));
		}

		auto open_session(const cycle_host& aHost) -> result<std::unique_ptr<cycle_session>>
		{
			return aHost.Open();
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

		/// The cycle a command names, or the one bound to the caller's working
		/// folder when that is unambiguous. Never a remembered or "current" cycle.
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
				if (!QDir::isAbsolutePath(aCommand.Directory)) {
					return fail("not_found", "No cycle is named, and no working folder was given to find one by.", "bpr cycle list");
				}
				auto root = observe(aSession, utf8(aCommand.Directory));
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

		/// Before a command reads or changes the round draft: a window's unsaved
		/// text is saved first, and composing text holds the command.
		auto guard_draft(const cycle_host& aHost, const command& aCommand, const core::review_cycle_state& aCycle) -> result<void>
		{
			if (!aHost.Draft) {
				return { };
			}
			return aHost.Draft(aCommand.Command, aCycle.Subject.Cycle);
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

		auto run_list(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			auto after = std::optional<core::review_cycle_id>{ };
			if (aCommand.has("--cursor")) {
				after = core::parse_cycle_name(utf8(aCommand.value("--cursor")));
				if (!after) {
					return fail("invalid_input", "--cursor takes the RC-<n> the previous page named.");
				}
			}
			auto session = open_session(aHost);
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

		auto run_open(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			if (aCommand.Positional.isEmpty()) {
				return fail("invalid_input", "Name the folder or file to open: bpr cycle open PATH.");
			}
			const auto location = absolute_path(aCommand.Positional.front(), "The folder or file to open");
			if (!location) {
				return std::unexpected(location.error());
			}
			auto session = open_session(aHost);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned  = **session;
			auto  opened = catalog(owned);
			if (!opened) {
				return std::unexpected(opened.error());
			}
			auto root = observe(owned, *location);
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
				const auto create  = core::create_review_cycle{ .Authority = opened->Authority, .Title = application::default_cycle_title(*location), .Scope = { } };
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

		auto run_show(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			auto session = open_session(aHost);
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

			// What a harness may read: the reviewer's grants, one per round ([[BPR-1169]]).
			auto grants = checked(owned.await<std::vector<core::cycle_round_grant>>([&](auto aDone) { return runtime.grants_selected(std::move(aDone)); }));
			if (!grants) {
				return std::unexpected(grants.error());
			}
			QJsonArray exposed;
			for (const auto& grant : *grants) {
				exposed.append(QJsonObject{ { "round", wire(grant.Round.Id) }, { "read", grant.ReadRound }, { "respond", grant.AppendResponse }, { "revoked", grant.Revoked } });
				human += "Grant " + wire(grant.Round.Id) + ": " + (grant.Revoked ? QString{ "revoked" } : grant.AppendResponse ? QString{ "a harness may read it and add a response" }
																															   : QString{ "a harness may read it" })
					+ ".\n";
			}
			data["grants"] = exposed;
			return output{ data, human, true };
		}

		auto run_capture(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			auto session = open_session(aHost);
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

		/// Whether another write moved the cycle on since it was read.
		auto revision_moved(const application::cycle_store_error& aError) -> bool
		{
			return aError.Failure == application::cycle_store_failure::conflict
				|| (aError.Failure == application::cycle_store_failure::refused && aError.Refusal == core::cycle_refusal::conflict);
		}

		/// `FIRST:LAST` or `LINE`, one-based and inclusive.
		auto line_span_of(const QString& aText) -> std::optional<core::cycle_line_span>
		{
			const auto parts = aText.split(':');
			const auto first = parts.size() <= 2 ? positive(parts.front()) : std::nullopt;
			const auto last  = parts.size() == 2 ? positive(parts.back()) : first;
			if (!first || !last || *last < *first) {
				return std::nullopt;
			}
			return core::cycle_line_span{ .First = *first, .Last = *last };
		}

		auto run_note(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			if (aCommand.has("--lines") && !aCommand.has("--file")) {
				return fail("invalid_input", "--lines needs --file PATH, a path inside the project.");
			}
			auto lines = std::optional<core::cycle_line_span>{ };
			if (aCommand.has("--lines")) {
				lines = line_span_of(aCommand.value("--lines"));
				if (!lines) {
					return fail("invalid_input", "--lines takes FIRST:LAST or LINE, one-based and inclusive, e.g. 40:52.");
				}
			}
			if (!aCommand.Text && !aCommand.Check) {
				// Never an editor, a prompt or stray standard input here: the caller reads the text.
				return fail("interaction_required", "A note needs its text: --body-file PATH, or --body-file - to read standard input.", "bpr cycle note" + aCommand.named() + " --body-file -");
			}

			auto session = open_session(aHost);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
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
				auto       bytes    = retained_file(owned, *cycle, revision, path);
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
			if (aCommand.Check) {
				// The target is checked before a person writes; nothing is saved.
				return output{ { { "cycle", text(name) }, { "target", target_json(target, lines) } }, "A note on " + target_words(target, lines) + " in " + text(name) + ".\n", true };
			}
			const auto& body = *aCommand.Text;
			if (text(body).trimmed().isEmpty()) {
				return fail("invalid_input", "A note needs some text; nothing was saved.");
			}
			if (body.size() > core::maximum_private_reviewer_body_bytes) {
				return fail("over_limit", "A note is limited to " + application::grouped_count(core::maximum_private_reviewer_body_bytes) + " UTF-8 bytes; this one is " + application::grouped_count(body.size()) + ". Nothing was saved.");
			}
			// One operation identity for this note, kept if it is tried again: a
			// note is added once, however often its request arrives.
			const auto operation = owned.operation();
			const auto begin     = [&](const std::uint64_t aRevision) {
				return owned.await<application::cycle_write_receipt>([&](auto aDone) { return owned.runtime().begin_feedback_selected(operation, aRevision, core::begin_cycle_feedback{ .Primary = target, .Body = body }, std::move(aDone)); });
			};
			auto receipt = begin(cycle->Revision);
			if (!receipt && revision_moved(receipt.error())) {
				// Another interface wrote the cycle meanwhile. A note stands on its
				// own target, so it is added to the cycle as it is now; the core
				// checks it again against that state.
				auto current = load_selected(owned);
				if (!current) {
					return std::unexpected(current.error());
				}
				receipt = begin(current->Revision);
			}
			if (!receipt) {
				return std::unexpected(store_failure(receipt.error()));
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

		/// `--context PATH [--context-lines A:B]` or `--drop-context PATH`: a
		/// whole file or some of its lines at revision `aSource`, which must hold it.
		auto context_edit(cycle_session& aSession, const core::review_cycle_state& aCycle, const command& aCommand, const core::cycle_source_revision_id aSource)
			-> result<core::cycle_compose_edit>
		{
			if (aCommand.has("--drop-context")) {
				return core::cycle_compose_edit{ core::exclude_round_context{ .Path = utf8(aCommand.value("--drop-context")) } };
			}
			const auto path  = utf8(aCommand.value("--context"));
			auto       bytes = retained_file(aSession, aCycle, aSource, path);
			if (!bytes) {
				return std::unexpected(bytes.error());
			}
			auto selection = core::cycle_context_selection{ .Revision = aSource, .Path = path, .Kind = core::cycle_context_kind::full_file };
			if (aCommand.has("--context-lines")) {
				const auto lines = line_span_of(aCommand.value("--context-lines"));
				const auto range = lines ? core::line_span_bytes(*bytes, *lines) : std::nullopt;
				if (!range) {
					return fail("invalid_input", "--context-lines takes FIRST:LAST or LINE of " + path + " at " + core::cycle_wire_name(aSource) + ", one-based and inclusive.");
				}
				selection.Kind  = core::cycle_context_kind::excerpt;
				selection.Start = range->Start;
				selection.End   = range->End;
			}
			return core::cycle_compose_edit{ core::include_round_context{ .Context = std::move(selection) } };
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

		auto run_review(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			auto goal = std::optional<std::string>{ };
			if (aCommand.has("--goal-file")) {
				if (!aCommand.Text) {
					return fail("invalid_input", "The goal was not read; give --goal-file PATH, or - for standard input.");
				}
				goal = *aCommand.Text;
			}
			const auto edits = std::ranges::count_if(QStringList{ "--include", "--exclude", "--earlier", "--later", "--action", "--goal-file", "--context", "--drop-context" },
													 [&](const QString& aOption) { return aCommand.has(aOption); });
			if (edits > 1) {
				return fail("invalid_input", "Make one change to the draft per command.");
			}
			if (aCommand.has("--context-lines") && !aCommand.has("--context")) {
				return fail("invalid_input", "--context-lines needs --context PATH, a file of the revision under review.");
			}
			const auto context = aCommand.has("--context") || aCommand.has("--drop-context");
			auto       edit    = composition_edit(aCommand, std::move(goal));
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
			if ((*edit || context) && !expected && !aCommand.Interactive) {
				return fail("interaction_required", "A scripted change to the draft names the revision it read: --expected-revision N.", "bpr cycle review" + aCommand.named() + " --json");
			}
			auto session = open_session(aHost);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			if (auto guarded = guard_draft(aHost, aCommand, *cycle); !guarded) {
				return std::unexpected(guarded.error());
			}
			const auto name  = core::cycle_wire_name(cycle->Subject.Cycle);
			auto       draft = compose(owned);
			if (!draft) {
				auto error = draft.error();
				error.Next = error.Code == "round_not_ready" ? "bpr cycle capture " + name : error.Next;
				return std::unexpected(std::move(error));
			}
			if (context) {
				// Source for the recipient to read, from the revision under review ([[BPR-1169]]).
				auto included = context_edit(owned, *cycle, aCommand, draft->Preview.Selection.Source);
				if (!included) {
					return std::unexpected(included.error());
				}
				*edit = std::move(*included);
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

		auto run_freeze(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			auto expected = std::optional<std::uint64_t>{ };
			if (aCommand.has("--expected-revision")) {
				expected = positive(aCommand.value("--expected-revision"));
				if (!expected) {
					return fail("invalid_input", "--expected-revision takes the draft revision review reported.");
				}
			}
			auto session = open_session(aHost);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			if (auto guarded = guard_draft(aHost, aCommand, *cycle); !guarded) {
				return std::unexpected(guarded.error());
			}
			const auto name  = core::cycle_wire_name(cycle->Subject.Cycle);
			auto       draft = compose(owned);
			if (!draft) {
				return std::unexpected(draft.error());
			}
			if (!expected) {
				// EOF or a pipe is never an approval; a terminal asks before it names the revision.
				const auto shown = draft->Preview.Draft.Revision;
				return fail("interaction_required", "Freezing names the draft revision you read: --expected-revision " + std::to_string(shown) + ".",
							"bpr cycle freeze " + name + " --expected-revision " + std::to_string(shown), QJsonObject{ { "draft_revision", decimal(shown) }, { "round", wire(draft->Preview.Round) } });
			}
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

		auto run_compare(const command& aCommand, const cycle_host& aHost) -> result<output>
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
			auto session = open_session(aHost);
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

		auto sha256_hex(const std::string_view aBytes) -> QString
		{
			return hex(core::cycle_material_digest{ core::sha256(std::span{ reinterpret_cast<const std::uint8_t*>(aBytes.data()), aBytes.size() }) });
		}

		auto run_export(const command& aCommand, const cycle_host& aHost) -> result<output>
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
			if (terminal && format == "bundle") {
				return fail("invalid_input", "A bundle is saved to a file, not printed.");
			}
			auto confirmed = std::optional<core::cycle_material_digest>{ };
			if (aCommand.has("--replace")) {
				confirmed = digest_of(aCommand.value("--replace"));
				if (!confirmed || terminal) {
					return fail("invalid_input", "--replace takes the lowercase sha256 of the file to replace, as the refusal reported it.");
				}
			}
			auto path = std::string{ };
			if (!terminal) {
				auto chosen = absolute_path(aCommand.value("--output"), "--output");
				if (!chosen) {
					return std::unexpected(chosen.error());
				}
				path = std::move(*chosen);
			}
			const auto request = application::cycle_round_export_request{
				.Round   = *round,
				.Channel = terminal ? core::cycle_disclosure_channel::terminal_output : core::cycle_disclosure_channel::local_file,
				.Format  = format == "bundle" ? core::cycle_round_file_kind::bundle : core::cycle_round_file_kind::markdown,
				.Policy  = { },
			};
			auto session = open_session(aHost);
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
			if (terminal) {
				// Rendered here; the caller writes it to its own standard output
				// and then records what happened (`export record`).
				auto exported = checked(owned.await<application::cycle_share_output>([&](auto aDone) { return owned.runtime().export_round_selected(request, std::move(aDone)); }));
				if (!exported) {
					return std::unexpected(exported.error());
				}
				if (exported->Decision != core::cycle_authorization::allowed) {
					return std::unexpected(policy_failure(exported->Decision, request.Channel));
				}
				if (exported->Limit || !exported->Round) {
					return std::unexpected(limit_failure(exported->Limit.value_or(core::cycle_round_output_limit{ })));
				}
				auto data = QJsonObject{ { "cycle", text(name) }, { "round", wire(*round) }, { "format", format }, { "bytes", decimal(exported->Bytes.size()) }, { "digest", sha256_hex(exported->Bytes) } };
				return output{ std::move(data), { }, true, std::move(exported->Bytes) };
			}

			const auto file        = utf8(QFileInfo{ text(path) }.fileName());
			auto       destination = std::shared_ptr<application::cycle_export_destination>{ adapters::workspace::make_local_export_destination(utf8_path(path)) };
			auto       saved       = checked(owned.await<application::cycle_file_share>([&](auto aDone) { return owned.runtime().export_round_to_file_selected(request, destination, file, confirmed, owned.operation(), std::move(aDone)); }));
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
				// A terminal asks before replacing; a script names the exact bytes it replaces.
				const auto& existing = *saved->Save->Existing;
				const auto  next     = "bpr cycle export " + name + " --round " + core::cycle_wire_name(*round) + " --output " + path + " --replace " + utf8(hex(existing.Digest));
				return fail("conflict", file + " already holds other content (" + application::grouped_count(existing.Size) + " bytes); nothing was written. Replace exactly those bytes with --replace.", next,
							QJsonObject{ { "existing_digest", hex(existing.Digest) }, { "existing_bytes", decimal(existing.Size) }, { "file", text(file) } });
			}
			const auto shown = core::cycle_wire_name(*round);
			const auto same  = saved->Save && saved->Save->Action == core::cycle_round_save_action::already_saved;
			return output{ { { "cycle", text(name) }, { "round", text(shown) }, { "format", format }, { "path", text(path) }, { "bytes", decimal(saved->Output.Bytes.size()) }, { "digest", hex(saved->Save->Written.Digest) }, { "written", !same } },
						   same ? text(file) + " already holds " + text(shown) + " exactly (" + text(bytes) + " bytes); nothing was written.\n"
								: "Saved " + text(shown) + " as " + text(file) + " (" + text(bytes) + " bytes). Saved is not received or read.\n",
						   true };
		}

		/// What standard output did with the exact bytes `export --output -` rendered.
		auto run_export_record(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			const auto round  = core::parse_round_name(utf8(aCommand.value("--round")));
			const auto state  = aCommand.value("--state");
			const auto digest = digest_of(aCommand.value("--digest"));
			if (!round || !digest || (state != "retrieved" && state != "failed")) {
				return fail("invalid_input", "A record names the round, retrieved or failed, and the bytes' sha256.");
			}
			const auto request = application::cycle_round_export_request{ .Round = *round, .Channel = core::cycle_disclosure_channel::terminal_output, .Format = core::cycle_round_file_kind::markdown, .Policy = { } };
			auto       session = open_session(aHost);
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
			// A frozen round renders the same bytes every time; anything else is recorded as nothing.
			auto exported = checked(owned.await<application::cycle_share_output>([&](auto aDone) { return owned.runtime().export_round_selected(request, std::move(aDone)); }));
			if (!exported) {
				return std::unexpected(exported.error());
			}
			if (exported->Decision != core::cycle_authorization::allowed || !exported->Round || digest_of(sha256_hex(exported->Bytes)) != digest) {
				return fail("conflict", "Those are not the bytes of " + core::cycle_wire_name(*round) + "; nothing was recorded.");
			}
			const auto delivered   = state == "retrieved" ? core::cycle_delivery_state::retrieved : core::cycle_delivery_state::failed;
			auto       observation = application::delivery_observation(*exported->Round, delivered, core::cycle_disclosure_channel::terminal_output,
																	   delivered == core::cycle_delivery_state::retrieved ? std::string_view{ exported->Bytes } : std::string_view{ }, exported->Format);
			auto       recorded    = checked(owned.await<application::cycle_write_receipt>([&](auto aDone) { return owned.runtime().record_delivery_selected(owned.operation(), std::move(observation), std::move(aDone)); }));
			if (!recorded) {
				return std::unexpected(recorded.error());
			}
			return output{ { { "cycle", text(name) }, { "round", wire(*round) }, { "state", state } }, { }, true };
		}

		/// What appending a response did: its receipt, or why nothing was appended.
		auto response_output(const application::cycle_response_result& aImported, const std::string& aName, const core::cycle_round_id aRound) -> result<output>
		{
			if (!aImported.Receipt) {
				const auto words = application::cycle_response_words(aImported);
				if (aImported.Read) {
					return fail("source_unavailable", words);
				}
				if (aImported.Refusal) {
					using problem   = core::cycle_response_problem;
					const auto kind = aImported.Refusal->Problem;
					return fail(kind == problem::stale_item ? "stale_revision" : kind == problem::reused_identifier || kind == problem::wrong_cycle || kind == problem::wrong_round || kind == problem::wrong_digest ? "conflict"
																																																					 : "invalid_input",
								words, { }, QJsonObject{ { "problem", text(core::stable_name(kind)) } });
				}
				return fail("invalid_input", words, { }, aImported.Decode ? QJsonObject{ { "problem", text(core::stable_name(aImported.Decode->Problem)) } } : QJsonObject{ });
			}
			const auto& receipt = *aImported.Receipt;
			// The same response under the same identifier is appended once.
			const auto again = receipt.Replayed || !receipt.Changed;
			return output{ { { "cycle", text(aName) }, { "round", wire(aRound) }, { "response", decimal(receipt.Allocated) }, { "appended", !again } },
						   text((again ? "That response was already added to " : "Added the response to ") + core::cycle_wire_name(aRound) + " of " + aName
								+ ". It is the recipient's statement: it changes no note and verifies nothing.\n"),
						   true };
		}

		auto run_response_import(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			const auto round = core::parse_round_name(utf8(aCommand.value("--round")));
			if (!round) {
				return fail("invalid_input", "Name the frozen round the response answers: --round F<n>.");
			}
			if (!aCommand.has("--file")) {
				return fail("invalid_input", "Name the response: --file PATH, or --file - for standard input.");
			}
			auto reader = application::cycle_response_file_reader{ };
			if (aCommand.Document) {
				// Read by the caller, explicitly asked for, before any lease was taken.
				reader = [document = std::vector<std::uint8_t>(aCommand.Document->begin(), aCommand.Document->end())](std::stop_token) -> std::expected<std::vector<std::uint8_t>, application::cycle_export_failure> { return document; };
			} else {
				auto path = absolute_path(aCommand.value("--file"), "--file");
				if (!path) {
					return std::unexpected(path.error());
				}
				reader = [chosen = utf8_path(*path)](std::stop_token aStop) {
					return adapters::workspace::read_chosen_file(chosen, core::maximum_response_document_bytes, std::move(aStop));
				};
			}
			auto session = open_session(aHost);
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
			return response_output(*imported, name, *round);
		}
		// ---- [[BPR-1169]]: one frozen round, exposed by the reviewer and read by a granted harness.

		auto round_named(const command& aCommand) -> result<core::cycle_round_id>
		{
			const auto round = core::parse_round_name(utf8(aCommand.value("--round")));
			if (!round) {
				return fail("invalid_input", "Name the frozen round: --round F<n>.");
			}
			return *round;
		}

		auto require_frozen(const core::review_cycle_state& aCycle, const core::cycle_round_id aRound) -> result<void>
		{
			const auto name = core::cycle_wire_name(aCycle.Subject.Cycle);
			if (aRound.value() >= aCycle.Next.Round) {
				return fail("round_not_ready", core::cycle_wire_name(aRound) + " of " + name + " is not frozen; only a frozen round is exposed or read. Nothing was read or changed.", "bpr cycle show " + name);
			}
			return { };
		}

		/// Why a consumer request was refused. Naming a round is never permission.
		auto grant_failure(const core::cycle_authorization aDecision, const std::string& aName, const core::cycle_round_id aRound, const std::string& aDoing) -> failure
		{
			const auto round  = core::cycle_wire_name(aRound);
			const auto detail = QJsonObject{ { "decision", text(core::name(aDecision)) } };
			switch (aDecision) {
			case core::cycle_authorization::not_granted:
				return { "policy_denied", "No grant lets a harness " + aDoing + " " + round + " of " + aName + ". Naming a round is not permission: the reviewer grants it. Nothing was read or changed.",
						 "bpr cycle round grant " + aName + " --round " + round, detail };
			case core::cycle_authorization::revoked:
				return { "policy_denied", "The grant for " + round + " of " + aName + " was revoked. Nothing was read or changed; what was read before cannot be recalled.", { }, detail };
			case core::cycle_authorization::channel_denied:
				return { "policy_denied", "Policy for this project's source does not allow external retrieval. Nothing was read or changed.", "bpr cycle policy", detail };
			case core::cycle_authorization::channel_unconfigured:
				return { "policy_denied", "Nothing configured allows external retrieval for source of unknown origin. Nothing was read or changed.",
						 "bpr cycle policy --classification unclassified --channel external_retrieval --permission allowed", detail };
			default: return { "policy_denied", "Refused (" + std::string{ core::name(aDecision) } + "). Nothing was read or changed.", { }, detail };
			}
		}

		auto retrieval_failure(const core::cycle_retrieval_problem aProblem, const std::string& aRound) -> failure
		{
			const auto detail = QJsonObject{ { "problem", text(core::stable_name(aProblem)) } };
			switch (aProblem) {
			case core::cycle_retrieval_problem::item_over_page:
				return { "requires_partition", "An item of " + aRound + " is larger than any page (" + application::grouped_count(core::maximum_round_page_bytes) + " bytes) and is never cut. Ask the reviewer to split the round; act on none of it meanwhile.", { }, detail };
			case core::cycle_retrieval_problem::page_size:
				return { "invalid_input", "--page-bytes takes " + std::to_string(core::minimum_round_page_bytes) + " to " + std::to_string(core::maximum_round_page_bytes) + ".", { }, detail };
			case core::cycle_retrieval_problem::cursor:
				return { "invalid_input", "That cursor is not one of " + aRound + "'s pages; start again with bpr cycle round read.", { }, detail };
			case core::cycle_retrieval_problem::unknown_source:
				return { "not_found", aRound + " has no attachment by that name; bpr cycle round source lists what it includes. Nothing else of the project is readable here.", { }, detail };
			case core::cycle_retrieval_problem::source_range:
				return { "invalid_input", "--range takes START:END within the attachment's bytes, half-open.", { }, detail };
			default: return { "invalid_input", "That listing position is past " + aRound + "'s last attachment.", { }, detail };
			}
		}

		/// The round a granted consumer may read, and only that; never on the reviewer's authority.
		auto granted_round(cycle_session& aSession, const core::review_cycle_state& aCycle, const core::cycle_round_id aRound) -> result<application::cycle_frozen_round>
		{
			if (auto frozen = require_frozen(aCycle, aRound); !frozen) {
				return std::unexpected(frozen.error());
			}
			auto read = checked(aSession.await<application::cycle_granted_round>([&](auto aDone) { return aSession.runtime().read_granted_round_selected(aRound, { }, std::move(aDone)); }));
			if (!read) {
				return std::unexpected(read.error());
			}
			if (read->Decision != core::cycle_authorization::allowed || !read->Round) {
				return std::unexpected(grant_failure(read->Decision, core::cycle_wire_name(aCycle.Subject.Cycle), aRound, "read"));
			}
			return std::move(*read->Round);
		}

		auto texts_json(const std::vector<std::string>& aTexts) -> QJsonArray
		{
			QJsonArray values;
			for (const auto& value : aTexts) {
				values.append(text(value));
			}
			return values;
		}

		/// What the round asks, and how much of it there is; no item, no source byte.
		auto round_header(const application::cycle_frozen_round& aRound, const core::cycle_round_extent& aExtent, const std::string& aName) -> QJsonObject
		{
			const auto& payload = aRound.Content.Payload;
			return {
				{ "cycle", text(aName) },
				{ "round", text(payload.RoundId) },
				{ "payload_sha256", hex(aRound.Content.Digest) },
				{ "project", QJsonObject{ { "id", text(payload.Project.Id) }, { "label", text(payload.Project.Label) } } },
				{ "source_revision", text(payload.SourceRevision) },
				{ "source_consistency", text(core::stable_name(payload.Consistency)) },
				{ "assignment", QJsonObject{ { "action", text(core::stable_name(payload.Assignment.Action)) }, { "goal", text(payload.Assignment.Goal) }, { "constraints", texts_json(payload.Assignment.Constraints) }, { "write_scope", texts_json(payload.Assignment.WriteScope) } } },
				{ "items_total", decimal(aExtent.Items) },
				{ "suggestions_total", decimal(aExtent.Suggestions) },
				{ "sources_total", decimal(aExtent.Sources) },
				{ "instruction_bytes", decimal(aExtent.InstructionBytes) },
				{ "source_bytes", decimal(aExtent.SourceBytes) },
				{ "intentional_omissions", texts_json(payload.Selection.Omissions) },
				{ "instruction_set_complete", payload.Selection.Complete },
			};
		}

		auto decoded_text(const QJsonValue& aRecord) -> QString
		{
			return QString::fromUtf8(QByteArray::fromBase64(aRecord.toObject().value("base64").toString().toLatin1()));
		}
		auto anchor_words(const QJsonObject& aAnchor) -> QString
		{
			const auto kind = aAnchor.value("kind").toString();
			const auto at   = " @" + aAnchor.value("source_revision_id").toString();
			if (kind == "range") {
				return aAnchor.value("path").toString() + " bytes " + QString::number(aAnchor.value("start_byte").toInteger()) + "-" + QString::number(aAnchor.value("end_byte").toInteger()) + at;
			}
			if (kind == "file") {
				return aAnchor.value("path").toString() + at + " (whole file)";
			}
			return kind == "module" ? aAnchor.value("path").toString() + " (module)" : QStringLiteral("the whole cycle");
		}
		/// One item as a person reads it: where, what, and each suggestion's exact text.
		auto item_words(const QJsonObject& aItem, const QString& aAction) -> QString
		{
			const auto action = aItem.value("action_override").isString() ? aItem.value("action_override").toString() : aAction;
			auto       words  = "## " + aItem.value("id").toString() + " (" + action + ")\n";
			for (const auto& anchor : aItem.value("anchors").toArray()) {
				words += "At " + anchor_words(anchor.toObject()) + "\n";
			}
			auto body = aItem.value("body").toString();
			words += body + (body.endsWith('\n') ? "" : "\n");
			for (const auto& entry : aItem.value("suggestions").toArray()) {
				const auto suggestion = entry.toObject();
				words += "Suggestion " + suggestion.value("id").toString() + " at " + anchor_words(suggestion.value("anchor").toObject()) + "\n--- original\n" + decoded_text(suggestion.value("original"))
					+ "\n--- replacement\n" + decoded_text(suggestion.value("replacement")) + "\n--- end\n";
			}
			return words;
		}

		/// One page of items, each exactly as the payload holds it.
		auto page_of(QJsonObject aData, const core::cycle_round_items_page& aPage, const QString& aAction, QString& aHuman) -> QJsonObject
		{
			QJsonArray items;
			for (const auto& entry : aPage.Items) {
				const auto item = QJsonDocument::fromJson(QByteArray::fromStdString(entry)).object();
				aHuman += "\n" + item_words(item, aAction);
				items.append(item);
			}
			aData["first_item"]    = decimal(aPage.First);
			aData["items_through"] = decimal(aPage.First + aPage.Items.size());
			aData["items"]         = items;
			aData["next"]          = aPage.Next ? QJsonValue{ text(core::encode_round_cursor(*aPage.Next)) } : QJsonValue{ };
			return aData;
		}
		auto page_footer(const core::cycle_round_items_page& aPage, const core::cycle_round_extent& aExtent, const std::string& aName, const std::string& aRound) -> QString
		{
			const auto through = aPage.First + aPage.Items.size();
			if (aPage.Next) {
				return "\nItems " + decimal(aPage.First + 1U) + " to " + decimal(through) + " of " + decimal(aExtent.Items) + ". Read every page before acting on any:\nbpr cycle round page " + text(aName) + " --round "
					+ text(aRound) + " --cursor " + text(core::encode_round_cursor(*aPage.Next)) + "\n";
			}
			return "\nThat is every instruction: items " + decimal(aPage.First + 1U) + " to " + decimal(through) + " of " + decimal(aExtent.Items) + ".\n";
		}

		auto run_round_read(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			const auto round = round_named(aCommand);
			if (!round) {
				return std::unexpected(round.error());
			}
			auto pageBytes = core::default_round_page_bytes;
			if (aCommand.has("--page-bytes")) {
				const auto asked = positive(aCommand.value("--page-bytes"));
				if (!asked) {
					return std::unexpected(retrieval_failure(core::cycle_retrieval_problem::page_size, core::cycle_wire_name(*round)));
				}
				pageBytes = *asked;
			}
			auto budget = std::optional<std::uint64_t>{ };
			if (aCommand.has("--budget-bytes")) {
				budget = positive(aCommand.value("--budget-bytes"));
				if (!budget) {
					return fail("invalid_input", "--budget-bytes takes the most instruction bytes you can hold, a positive number.");
				}
			}
			auto session = open_session(aHost);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			const auto name   = core::cycle_wire_name(cycle->Subject.Cycle);
			auto       frozen = granted_round(owned, *cycle, *round);
			if (!frozen) {
				return std::unexpected(frozen.error());
			}
			const auto extent = core::measure_round(frozen->Content.Payload);
			if (!extent) {
				return fail("source_unavailable", "The round could not be measured; nothing was read.");
			}
			const auto shown = core::cycle_wire_name(*round);
			if (budget && core::admit_round_budget(*extent, *budget)) {
				return fail("requires_partition",
							shown + "'s instructions are " + application::grouped_count(extent->InstructionBytes) + " bytes (" + application::grouped_count(extent->Items) + " items), more than the "
								+ application::grouped_count(*budget) + " you can hold. Nothing was read: ask the reviewer to split it into rounds, and act on no part of it meanwhile.",
							{ }, QJsonObject{ { "instruction_bytes", decimal(extent->InstructionBytes) }, { "items_total", decimal(extent->Items) }, { "budget_bytes", decimal(*budget) } });
			}
			auto page = core::page_round_items(frozen->Content, *round, 0, pageBytes);
			if (!page) {
				return std::unexpected(retrieval_failure(page.error(), shown));
			}
			const auto& payload = frozen->Content.Payload;
			const auto  action  = text(core::stable_name(payload.Assignment.Action));
			auto        human   = text(name) + " " + text(shown) + " (sha256 " + hex(frozen->Content.Digest) + "): asks to " + action + " " + decimal(extent->Items) + (extent->Items == 1U ? " item" : " items")
				+ " against " + text(payload.SourceRevision) + ".\n";
			if (!payload.Assignment.Goal.empty()) {
				human += "Goal: " + text(payload.Assignment.Goal) + "\n";
			}
			if (extent->Sources > 0U) {
				human += decimal(extent->Sources) + (extent->Sources == 1U ? " source attachment" : " source attachments") + ": bpr cycle round source " + text(name) + " --round " + text(shown) + "\n";
			}
			auto data = page_of(round_header(*frozen, *extent, name), *page, action, human);
			human += page_footer(*page, *extent, name, shown);
			return output{ std::move(data), human, !page->Next };
		}

		auto run_round_page(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			const auto round = round_named(aCommand);
			if (!round) {
				return std::unexpected(round.error());
			}
			const auto cursor = core::parse_round_cursor(utf8(aCommand.value("--cursor")));
			if (!cursor) {
				return std::unexpected(retrieval_failure(core::cycle_retrieval_problem::cursor, core::cycle_wire_name(*round)));
			}
			auto session = open_session(aHost);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			const auto name   = core::cycle_wire_name(cycle->Subject.Cycle);
			const auto shown  = core::cycle_wire_name(*round);
			auto       frozen = granted_round(owned, *cycle, *round);
			if (!frozen) {
				return std::unexpected(frozen.error());
			}
			const auto extent = core::measure_round(frozen->Content.Payload);
			if (!extent) {
				return fail("source_unavailable", "The round could not be measured; nothing was read.");
			}
			if (const auto problem = core::admit_round_cursor(*cursor, *round, frozen->Content.Digest, *extent)) {
				return std::unexpected(retrieval_failure(*problem, shown));
			}
			auto page = core::page_round_items(frozen->Content, *round, cursor->Next, cursor->PageBytes);
			if (!page) {
				return std::unexpected(retrieval_failure(page.error(), shown));
			}
			auto human = text(name) + " " + text(shown) + " (sha256 " + hex(frozen->Content.Digest) + "), continued.\n";
			auto data  = QJsonObject{ { "cycle", text(name) }, { "round", text(shown) }, { "payload_sha256", hex(frozen->Content.Digest) }, { "items_total", decimal(extent->Items) } };
			data       = page_of(std::move(data), *page, text(core::stable_name(frozen->Content.Payload.Assignment.Action)), human);
			human += page_footer(*page, *extent, name, shown);
			return output{ std::move(data), human, !page->Next };
		}

		auto source_json(const core::cycle_wire_source& aSource) -> QJsonObject
		{
			return {
				{ "id", text(aSource.Id) },
				{ "path", text(aSource.Path) },
				{ "source_revision", text(aSource.Revision) },
				{ "kind", text(core::stable_name(aSource.Kind)) },
				{ "start_byte", decimal(aSource.Start) },
				{ "end_byte", decimal(aSource.End) },
				{ "blob_sha256", hex(aSource.Blob) },
				{ "bytes", decimal(aSource.Bytes.size()) },
			};
		}

		auto run_round_source(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			const auto round = round_named(aCommand);
			if (!round) {
				return std::unexpected(round.error());
			}
			const auto shown = core::cycle_wire_name(*round);
			if (aCommand.has("--range") && !aCommand.has("--source")) {
				return fail("invalid_input", "--range needs --source S<n>, an attachment of the round.");
			}
			if (aCommand.has("--cursor") && aCommand.has("--source")) {
				return fail("invalid_input", "--cursor continues a listing; --source reads one attachment.");
			}
			auto range = std::optional<core::cycle_source_range>{ };
			if (aCommand.has("--range")) {
				range = core::parse_source_range(utf8(aCommand.value("--range")));
				if (!range) {
					return std::unexpected(retrieval_failure(core::cycle_retrieval_problem::source_range, shown));
				}
			}
			auto from = std::uint64_t{ 0 };
			if (aCommand.has("--cursor")) {
				const auto position = positive(aCommand.value("--cursor"));
				if (!position) {
					return std::unexpected(retrieval_failure(core::cycle_retrieval_problem::source_cursor, shown));
				}
				from = *position;
			}
			auto session = open_session(aHost);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			const auto name   = core::cycle_wire_name(cycle->Subject.Cycle);
			auto       frozen = granted_round(owned, *cycle, *round);
			if (!frozen) {
				return std::unexpected(frozen.error());
			}
			const auto& payload = frozen->Content.Payload;
			auto        data    = QJsonObject{ { "cycle", text(name) }, { "round", text(shown) }, { "payload_sha256", hex(frozen->Content.Digest) }, { "sources_total", decimal(payload.Sources.size()) } };
			if (!aCommand.has("--source")) {
				const auto listing = core::list_round_sources(payload, from);
				if (!listing) {
					return std::unexpected(retrieval_failure(listing.error(), shown));
				}
				QJsonArray sources;
				auto       human = text(name) + " " + text(shown) + " includes " + decimal(payload.Sources.size()) + (payload.Sources.size() == 1U ? " source attachment" : " source attachments") + (payload.Sources.empty() ? "." : ":") + "\n";
				for (auto index = listing->First; index < listing->First + listing->Count; ++index) {
					const auto& source = payload.Sources[index];
					sources.append(source_json(source));
					human += "  " + text(source.Id) + "  " + text(source.Path) + " @" + text(source.Revision) + "  " + text(core::stable_name(source.Kind)) + ", " + decimal(source.Bytes.size()) + " bytes\n";
				}
				if (listing->Next) {
					human += "More: bpr cycle round source " + text(name) + " --round " + text(shown) + " --cursor " + decimal(*listing->Next) + "\n";
				}
				data["first"]   = decimal(listing->First);
				data["sources"] = sources;
				data["next"]    = listing->Next ? QJsonValue{ decimal(*listing->Next) } : QJsonValue{ };
				return output{ std::move(data), human, !listing->Next };
			}
			const auto slice = core::slice_round_source(payload, utf8(aCommand.value("--source")), range);
			if (!slice) {
				return std::unexpected(retrieval_failure(slice.error(), shown));
			}
			const auto& source = payload.Sources[slice->Source];
			const auto  bytes  = std::string_view{ source.Bytes }.substr(slice->Range.Start, slice->Range.End - slice->Range.Start);
			data["source"]     = source_json(source);
			data["start"]      = decimal(slice->Range.Start);
			data["end"]        = decimal(slice->Range.End);
			data["base64"]     = text(core::encode_cycle_base64(bytes));
			data["sha256"]     = sha256_hex(bytes);
			data["next_range"] = slice->Next ? QJsonValue{ decimal(slice->Next->Start) + ":" + decimal(slice->Next->End) } : QJsonValue{ };
			auto human         = text(source.Id) + "  " + text(source.Path) + " @" + text(source.Revision) + ", bytes " + decimal(slice->Range.Start) + " to " + decimal(slice->Range.End) + " of " + decimal(source.Bytes.size()) + ":\n"
				+ text(bytes) + (bytes.ends_with('\n') ? "" : "\n");
			if (slice->Next) {
				human += "More: bpr cycle round source " + text(name) + " --round " + text(shown) + " --source " + text(source.Id) + " --range " + decimal(slice->Next->Start) + ":" + decimal(slice->Next->End) + "\n";
			}
			return output{ std::move(data), human, !slice->Next };
		}

		auto run_round_grant(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			const auto round = round_named(aCommand);
			if (!round) {
				return std::unexpected(round.error());
			}
			auto session = open_session(aHost);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			if (auto frozen = require_frozen(*cycle, *round); !frozen) {
				return std::unexpected(frozen.error());
			}
			const auto name     = core::cycle_wire_name(cycle->Subject.Cycle);
			const auto shown    = core::cycle_wire_name(*round);
			const auto readOnly = aCommand.has("--read-only");
			auto       granted  = checked(owned.await<application::cycle_grant_result>([&](auto aDone) { return owned.runtime().grant_round_selected(owned.operation(), *round, readOnly, { }, std::move(aDone)); }));
			if (!granted) {
				return std::unexpected(granted.error());
			}
			if (granted->Decision != core::cycle_authorization::allowed || !granted->Grant) {
				auto refusal    = grant_failure(granted->Decision, name, *round, "read");
				refusal.Message = "The round cannot be exposed: " + refusal.Message;
				return std::unexpected(std::move(refusal));
			}
			const auto invocation = "bpr cycle round read " + name + " --round " + shown;
			return output{ { { "cycle", text(name) }, { "round", text(shown) }, { "payload_sha256", hex(granted->Grant->Digest) }, { "read", true }, { "respond", granted->Grant->AppendResponse }, { "revoked", false }, { "changed", granted->Receipt && granted->Receipt->Changed }, { "invocation", text(invocation) } },
						   text(name) + " " + text(shown) + " can now be read by a harness through the local interface" + (readOnly ? QString{ "" } : QString{ ", which may also add a response to it" })
							   + "; nothing else of this profile is readable that way. Nothing was sent or started.\nThe harness runs: " + text(invocation) + "\nWithdraw it: bpr cycle round revoke " + text(name) + " --round " + text(shown) + "\n",
						   true };
		}

		auto run_round_revoke(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			const auto round = round_named(aCommand);
			if (!round) {
				return std::unexpected(round.error());
			}
			auto session = open_session(aHost);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			const auto name    = core::cycle_wire_name(cycle->Subject.Cycle);
			const auto shown   = core::cycle_wire_name(*round);
			auto       revoked = checked(owned.await<application::cycle_grant_result>([&](auto aDone) { return owned.runtime().revoke_round_selected(owned.operation(), *round, std::move(aDone)); }));
			if (!revoked) {
				return std::unexpected(revoked.error());
			}
			if (!revoked->Grant) {
				return fail("not_found", shown + " of " + name + " has no grant to revoke.", "bpr cycle show " + name);
			}
			return output{ { { "cycle", text(name) }, { "round", text(shown) }, { "revoked", true }, { "changed", revoked->Receipt && revoked->Receipt->Changed } },
						   "Revoked the grant for " + text(name) + " " + text(shown) + ": no harness can read it or add a response to it now. What was read before cannot be recalled.\n",
						   true };
		}

		auto run_response_append(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			const auto round = round_named(aCommand);
			if (!round) {
				return std::unexpected(round.error());
			}
			if (!aCommand.has("--file")) {
				return fail("invalid_input", "Name the response: --file PATH, or --file - for standard input.");
			}
			auto reader = application::cycle_response_file_reader{ };
			if (aCommand.Document) {
				reader = [document = std::vector<std::uint8_t>(aCommand.Document->begin(), aCommand.Document->end())](std::stop_token) -> std::expected<std::vector<std::uint8_t>, application::cycle_export_failure> { return document; };
			} else {
				auto path = absolute_path(aCommand.value("--file"), "--file");
				if (!path) {
					return std::unexpected(path.error());
				}
				reader = [chosen = utf8_path(*path)](std::stop_token aStop) { return adapters::workspace::read_chosen_file(chosen, core::maximum_response_document_bytes, std::move(aStop)); };
			}
			auto session = open_session(aHost);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned = **session;
			auto  cycle = resolve(owned, aCommand);
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			if (auto frozen = require_frozen(*cycle, *round); !frozen) {
				return std::unexpected(frozen.error());
			}
			const auto name     = core::cycle_wire_name(cycle->Subject.Cycle);
			auto       appended = checked(owned.await<application::cycle_granted_response>([&](auto aDone) { return owned.runtime().append_granted_response_selected(owned.operation(), *round, std::move(reader), std::move(aDone)); }));
			if (!appended) {
				return std::unexpected(appended.error());
			}
			if (appended->Decision != core::cycle_authorization::allowed || !appended->Result) {
				return std::unexpected(grant_failure(appended->Decision, name, *round, "add a response to"));
			}
			return response_output(*appended->Result, name, *round);
		}

		auto run_policy(const command& aCommand, const cycle_host& aHost) -> result<output>
		{
			if (!aCommand.Positional.isEmpty()) {
				return fail("invalid_input", "The disclosure policy is the profile's, not a cycle's: bpr cycle policy takes no RC-<n>.");
			}
			const auto setting        = std::ranges::count_if(QStringList{ "--classification", "--channel", "--permission" }, [&](const QString& aOption) { return aCommand.has(aOption); });
			const auto classification = core::parse_classification(utf8(aCommand.value("--classification")));
			const auto channel        = core::parse_channel(utf8(aCommand.value("--channel")));
			const auto permission     = core::parse_permission(utf8(aCommand.value("--permission")));
			if (setting != 0 && (setting != 3 || !classification || !channel || !permission)) {
				return fail("invalid_input",
							"Set one channel with all three: --classification unclassified|public_source|enterprise_restricted, --channel clipboard|local_file|terminal_output|external_retrieval|agent_adapter and --permission allowed|denied|unconfigured.");
			}
			auto session = open_session(aHost);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto& owned   = **session;
			auto  changed = std::optional<bool>{ };
			if (setting == 3) {
				auto written = checked(owned.await<application::cycle_write_receipt>([&](auto aDone) { return owned.runtime().set_channel_permission(owned.operation(), *classification, *channel, *permission, std::move(aDone)); }));
				if (!written) {
					return std::unexpected(written.error());
				}
				changed = written->Changed;
			}
			auto policy = checked(owned.await<core::cycle_disclosure_policy>([&](auto aDone) { return owned.runtime().disclosure_policy(std::move(aDone)); }));
			if (!policy) {
				return std::unexpected(policy.error());
			}
			QJsonArray rows;
			auto       human = changed ? QString{ *changed ? "Saved.\n" : "Already so; nothing changed.\n" } : QString{ };
			human += "Disclosure policy of this profile (what each channel decides for each kind of source):\n";
			for (const auto kind : { core::cycle_data_classification::unclassified, core::cycle_data_classification::public_source, core::cycle_data_classification::enterprise_restricted }) {
				for (std::size_t index = 0; index < core::cycle_disclosure_channel_count; ++index) {
					const auto way        = static_cast<core::cycle_disclosure_channel>(index);
					const auto configured = core::configured_permission(*policy, kind, way);
					const auto decision   = core::channel_permission(*policy, kind, way);
					rows.append(QJsonObject{ { "classification", text(core::stable_name(kind)) }, { "channel", text(core::stable_name(way)) }, { "configured", text(core::stable_name(configured)) }, { "decision", text(core::name(decision)) } });
					human += "  " + text(core::stable_name(kind)).leftJustified(22) + text(core::stable_name(way)).leftJustified(20) + text(core::name(decision))
						+ (configured == core::cycle_channel_permission::unconfigured ? QString{ " (default)" } : QString{ }) + "\n";
				}
			}
			auto data = QJsonObject{ { "policy", rows } };
			if (changed) {
				data["changed"] = *changed;
			}
			return output{ std::move(data), human, true };
		}

	}

	auto cycle_invocation::named() const -> std::string
	{
		return Positional.isEmpty() || Command == core::cycle_command::open ? std::string{ } : " " + utf8(Positional.front());
	}

	auto cycle_exit_code(const std::string_view aCode) -> int
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

	auto execute_cycle_command(const cycle_invocation& aInvocation, const cycle_host& aHost) -> cycle_result<cycle_output>
	{
		auto value = [&]() -> result<output> {
			switch (aInvocation.Command) {
			case core::cycle_command::list:            return run_list(aInvocation, aHost);
			case core::cycle_command::open:            return run_open(aInvocation, aHost);
			case core::cycle_command::show:            return run_show(aInvocation, aHost);
			case core::cycle_command::capture:         return run_capture(aInvocation, aHost);
			case core::cycle_command::note:            return run_note(aInvocation, aHost);
			case core::cycle_command::review:          return run_review(aInvocation, aHost);
			case core::cycle_command::freeze:          return run_freeze(aInvocation, aHost);
			case core::cycle_command::export_round:    return run_export(aInvocation, aHost);
			case core::cycle_command::compare:         return run_compare(aInvocation, aHost);
			case core::cycle_command::response_import: return run_response_import(aInvocation, aHost);
			case core::cycle_command::export_record:   return run_export_record(aInvocation, aHost);
			case core::cycle_command::round_grant:     return run_round_grant(aInvocation, aHost);
			case core::cycle_command::round_revoke:    return run_round_revoke(aInvocation, aHost);
			case core::cycle_command::round_read:      return run_round_read(aInvocation, aHost);
			case core::cycle_command::round_page:      return run_round_page(aInvocation, aHost);
			case core::cycle_command::round_source:    return run_round_source(aInvocation, aHost);
			case core::cycle_command::response_append: return run_response_append(aInvocation, aHost);
			case core::cycle_command::policy:          return run_policy(aInvocation, aHost);
			}
			return fail("unsupported_command", "bpr cycle has no such command.", "bpr cycle capabilities");
		}();
		if (value && core::may_write(aInvocation.Command) && !aInvocation.Check) {
			value->Changed = core::parse_cycle_name(utf8(value->Data.value("cycle").toString()));
		}
		return value;
	}

	auto cycle_request_of(const cycle_invocation& aInvocation) -> application::cycle_owner_request
	{
		auto request = application::cycle_owner_request{
			.Command     = aInvocation.Command,
			.Operation   = aInvocation.Operation,
			.Step        = aInvocation.Step,
			.Positional  = { },
			.Options     = { },
			.Directory   = utf8(aInvocation.Directory),
			.Text        = aInvocation.Text,
			.Document    = aInvocation.Document,
			.Check       = aInvocation.Check,
			.Interactive = aInvocation.Interactive,
		};
		for (const auto& positional : aInvocation.Positional) {
			request.Positional.push_back(utf8(positional));
		}
		for (auto option = aInvocation.Values.cbegin(); option != aInvocation.Values.cend(); ++option) {
			request.Options.emplace_back(utf8(option.key()), utf8(option.value()));
		}
		return request;
	}

	auto cycle_invocation_of(const application::cycle_owner_request& aRequest) -> cycle_invocation
	{
		auto invocation = cycle_invocation{
			.Command     = aRequest.Command,
			.Positional  = { },
			.Values      = { },
			.Operation   = aRequest.Operation,
			.Step        = aRequest.Step,
			.Directory   = text(aRequest.Directory),
			.Text        = aRequest.Text,
			.Document    = aRequest.Document,
			.Check       = aRequest.Check,
			.Interactive = aRequest.Interactive,
		};
		for (const auto& positional : aRequest.Positional) {
			invocation.Positional.push_back(text(positional));
		}
		for (const auto& [option, value] : aRequest.Options) {
			invocation.Values.insert(text(option), text(value));
		}
		return invocation;
	}

	auto cycle_response_of(const cycle_invocation& aInvocation, const cycle_result<cycle_output>& aResult) -> application::cycle_owner_response
	{
		auto response = application::cycle_owner_response{ .Operation = aInvocation.Operation, .Step = aInvocation.Step, .Ok = aResult.has_value() };
		if (aResult) {
			response.Data     = QJsonDocument{ aResult->Data }.toJson(QJsonDocument::Compact).toStdString();
			response.Human    = utf8(aResult->Human);
			response.Complete = aResult->Complete;
			response.Output   = aResult->Output;
			return response;
		}
		const auto& problem = aResult.error();
		response.Code       = problem.Code.empty() ? std::string{ "operation_failed" } : problem.Code;
		response.Message    = problem.Message;
		response.Next       = problem.Next;
		response.Detail     = problem.Detail.isEmpty() ? std::string{ } : QJsonDocument{ problem.Detail }.toJson(QJsonDocument::Compact).toStdString();
		return response;
	}

	auto cycle_result_of(const application::cycle_owner_response& aResponse) -> cycle_result<cycle_output>
	{
		const auto object = [](const std::string& aJson) -> std::optional<QJsonObject> {
			if (aJson.empty()) {
				return QJsonObject{ };
			}
			const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(aJson));
			return document.isObject() ? std::optional{ document.object() } : std::nullopt;
		};
		if (!aResponse.Ok) {
			const auto detail = object(aResponse.Detail);
			return fail(aResponse.Code, aResponse.Message, aResponse.Next, detail.value_or(QJsonObject{ }));
		}
		const auto data = object(aResponse.Data);
		if (!data) {
			return fail("uncertain", "The owner's answer could not be read; the command may have been applied. Read the cycle before repeating it.", "bpr cycle list");
		}
		return cycle_output{ *data, text(aResponse.Human), aResponse.Complete, aResponse.Output };
	}
}
