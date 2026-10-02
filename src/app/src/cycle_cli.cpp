#include "cycle_cli.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/review_cycle_words.hpp"
#include "bpr/core/private_thread_lineage.hpp"
#include "bpr/core/review_cycle_bundle.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
#include "bpr/core/review_tour.hpp"
#include "cycle_commands.hpp"
#include "terminal_text.hpp"
#include "tour_cancellation.hpp"
#include "tour_editor.hpp"
#include "tour_text_input.hpp"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <future>
#include <iostream>
#include <thread>
#include <utility>
namespace bpr::app {
	namespace {
		/// The result envelope every `--json` call writes, exactly once, to standard output.
		constexpr auto result_schema = "bpr.cycle.cli-result.v1";
		/// How long a terminal waits for the profile owner to answer one command.
		constexpr auto owner_answer_wait = std::chrono::seconds{ 600 };
		/// How often a terminal looks again for an owner that went away mid-command.
		constexpr int owner_attempts = 5;

		using failure = cycle_failure;
		using output  = cycle_output;
		template <typename T>
		using result = cycle_result<T>;
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

		// ---- The command line.

		struct command
		{
			QString               Name;
			cycle_invocation      Invocation;
			bool                  Json = false, Interactive = false;
			std::filesystem::path Base;
			std::string           Profile;
		};

		auto command_names() -> QStringList
		{
			QStringList names{ "capabilities" };
			for (const auto command : core::typed_cycle_commands()) {
				names.push_back(text(core::name(command)));
			}
			return names;
		}

		/// A path the caller typed, made absolute against this terminal's working folder.
		auto chosen_path(const QString& aPath) -> QString
		{
			return QDir::cleanPath(QDir::current().absoluteFilePath(aPath));
		}

		auto parse(const QStringList& aArguments) -> result<command>
		{
			if (aArguments.size() > 64) {
				return fail("invalid_input", "Too many command arguments.");
			}
			auto        result = command{ };
			auto        values = QMap<QString, QString>{ };
			QStringList words;
			const auto  known = [](const QString& aOption) {
				const auto option = utf8(aOption);
				if (std::ranges::find(core::common_cycle_options(), std::string_view{ option }) != core::common_cycle_options().end()) {
					return true;
				}
				for (const auto command : core::typed_cycle_commands()) {
					const auto options = core::cycle_command_options(command);
					if (std::ranges::find(options, std::string_view{ option }) != options.end()) {
						return true;
					}
				}
				return false;
			};
			for (qsizetype index = 0; index < aArguments.size(); ++index) {
				const auto& argument = aArguments[index];
				if (argument.size() > 8192) {
					return fail("invalid_input", "A command argument exceeds the bound.");
				}
				if (!argument.startsWith("--")) {
					words.push_back(argument);
					continue;
				}
				if (values.contains(argument)) {
					return fail("invalid_input", "An option was repeated.");
				}
				if (core::cycle_option_is_flag(utf8(argument))) {
					values.insert(argument, "true");
					continue;
				}
				// No generic --yes or --force: authority is never a flag.
				if (!known(argument) || index + 1 == aArguments.size()) {
					return fail("unsupported_option", "Unknown option or missing option value: " + utf8(argument.left(64)) + ".", "bpr cycle capabilities");
				}
				values.insert(argument, aArguments[++index]);
			}
			result.Json        = values.contains("--json");
			result.Interactive = interactive_terminal() && !result.Json && !values.contains("--non-interactive");
			if (values.contains("--schema-version") && values["--schema-version"] != "1") {
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
			const auto parsed = words.front() == "capabilities" ? std::nullopt : core::parse_cycle_command(utf8(words.front()));
			if (words.front() != "capabilities" && !parsed) {
				return fail("unsupported_command", "bpr cycle has no command " + utf8(words.front().left(64)) + ".", "bpr cycle capabilities");
			}
			result.Name           = words.front();
			const auto positional = words.mid(1);
			const auto maximum    = parsed ? 1 : 0;
			const auto needed     = parsed == core::cycle_command::open ? 1 : 0;
			if (positional.size() < needed || positional.size() > maximum) {
				return fail("invalid_input", needed ? "Name the folder or file to open: bpr cycle open PATH." : "Name at most one cycle, as RC-<n>.");
			}
			QStringList allowed{ "--json", "--non-interactive", "--schema-version", "--data-root" };
			if (parsed) {
				allowed.append({ "--profile", "--operation-id" });
				for (const auto option : core::cycle_command_options(*parsed)) {
					allowed.push_back(text(option));
				}
			}
			for (const auto& option : values.keys()) {
				if (!allowed.contains(option)) {
					return fail("unsupported_option", "bpr cycle " + utf8(result.Name) + " does not take " + utf8(option) + ".", "bpr cycle capabilities");
				}
			}
			const auto base = values.value("--data-root", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
			if (!QDir::isAbsolutePath(base)) {
				return fail("invalid_input", "--data-root must be an absolute private path.");
			}
			result.Base    = native_filesystem_path(base);
			result.Profile = utf8(values.value("--profile"));
			auto operation = values.value("--operation-id", QUuid::createUuid().toString(QUuid::WithoutBraces));
			if (!core::valid_cycle_operation_id(utf8(operation))) {
				return fail("invalid_input", "--operation-id takes 1 to 64 letters, digits or hyphens.");
			}
			result.Invocation.Operation = utf8(operation);
			if (!parsed) {
				return result;
			}
			auto& invocation       = result.Invocation;
			invocation.Command     = *parsed;
			invocation.Interactive = result.Interactive;
			invocation.Directory   = QDir::currentPath();
			invocation.Positional  = positional;
			if (*parsed == core::cycle_command::open) {
				invocation.Positional.front() = chosen_path(positional.front());
			}
			for (const auto& common : core::common_cycle_options()) {
				values.remove(text(common));
			}
			// Paths the owner may act on are made absolute here: it has no working folder of ours.
			for (const auto& option : { QStringLiteral("--output"), QStringLiteral("--file") }) {
				if (values.contains(option) && values[option] != "-" && (*parsed == core::cycle_command::export_round || *parsed == core::cycle_command::response_import)) {
					values[option] = chosen_path(values[option]);
				}
			}
			invocation.Values = std::move(values);
			return result;
		}

		/// Text from `--body-file`/`--goal-file`: a chosen file or explicit standard input.
		auto text_input(const QString& aPath) -> result<std::string>
		{
			auto input = read_tour_text_input(aPath == "-" ? aPath : chosen_path(aPath));
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

		/// What the command reads before anything is opened: chosen text, or a response on standard input.
		auto read_inputs(cycle_invocation& aInvocation) -> result<void>
		{
			for (const auto& option : { QStringLiteral("--body-file"), QStringLiteral("--goal-file") }) {
				if (aInvocation.has(option)) {
					auto input = text_input(aInvocation.value(option));
					if (!input) {
						return std::unexpected(input.error());
					}
					aInvocation.Text           = std::move(*input);
					aInvocation.Values[option] = "-";
				}
			}
			if (aInvocation.Command == core::cycle_command::response_import && aInvocation.value("--file") == "-") {
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
				aInvocation.Document = std::string{ bytes.constData(), static_cast<std::size_t>(bytes.size()) };
			}
			return { };
		}

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
				{ "owner", "one writer: while another BetterPullReview process owns the profile, each command is forwarded to it over its same-user command channel (protocol 3) and run on its store; an owner without one is refused with capability_unavailable" },
				{ "operation_id", "--operation-id names a command; repeating it never applies the command twice, and an owner answers a repeat as it answered the first" },
				{ "round_read", "not in this build: export a frozen round, or read it with bpr cycle export --output -" },
				{ "response_append", "reviewer import of a chosen file or standard input; it verifies and publishes nothing" },
				{ "export_channels", QJsonArray{ "local_file", "terminal_output" } },
				{ "limits", QJsonObject{ { "page_entries", decimal(cycle_page_entries) }, { "file_rows", decimal(cycle_file_rows) }, { "note_bytes", decimal(core::maximum_private_reviewer_body_bytes) }, { "document_bytes", decimal(core::maximum_round_document_file_bytes) }, { "bundle_bytes", decimal(core::maximum_bundle_archive_bytes) }, { "response_bytes", decimal(core::maximum_response_document_bytes) }, { "owner_message_bytes", decimal(application::maximum_tour_channel_bytes) } } },
				{ "exit_codes", QJsonObject{ { "0", "ok" }, { "2", "invalid input or target" }, { "3", "interaction_required" }, { "4", "stale_revision, conflict or draft_busy" }, { "5", "refused or unavailable" }, { "6", "uncertain" }, { "7", "storage" }, { "130", "cancelled" } } },
			};
			return { data,
					 "bpr cycle - local Review Cycles from a terminal\n"
					 "CLI contract: 1\n"
					 "Commands: "
						 + command_names().join(", ") + "\n"
														"No window, display, account, provider, agent or network is needed. Add --json for one bpr.cycle.cli-result.v1 document.\n",
					 true };
		}

		// ---- Where a command runs: here, or at the profile's owner.

		/// \brief Runs a command's requests on this profile: on a session of its
		/// own when it can take the lease, otherwise at the process that holds it.
		class runner
		{
		public:
			runner(std::filesystem::path aBase, std::string aProfile)
				: Base{ std::move(aBase) }
				, Profile{ std::move(aProfile) }
			{ }

			auto run(cycle_invocation aInvocation) -> result<output>
			{
				aInvocation.Step = ++Step;
				for (int attempt = 0; attempt < owner_attempts; ++attempt) {
					auto admission = admit();
					if (!admission) {
						return std::unexpected(admission.error());
					}
					if (admission->Lease) {
						auto       held = std::optional<profile_admission>{ std::move(*admission) };
						const auto seed = cycle_operation_seed(aInvocation.Operation, aInvocation.Step);
						const auto host = cycle_host{
							.Open =
								[&]() -> result<std::unique_ptr<cycle_session>> {
								if (!held) {
									return fail("operation_failed", "The command asked for a second session.");
								}
								auto session = std::make_unique<cycle_session>(std::move(*held), seed, [] { return tour_cancelled(); });
								held.reset();
								return session;
							},
							.Draft = { },
						};
						return execute_cycle_command(aInvocation, host);
					}
					if (auto answered = forward(*admission, aInvocation)) {
						return std::move(*answered);
					}
					// The owner went away, or is going: look again, here or at its successor.
					std::this_thread::sleep_for(std::chrono::milliseconds{ 100 * (attempt + 1) });
				}
				return fail("capability_unavailable",
							"Another BetterPullReview process owns this profile and offers no command channel this bpr can reach. Close it (or update it) and retry; nothing was opened a second time.");
			}

		private:
			auto admit() -> result<profile_admission>
			{
				// Admission blocks on private storage, so it runs where store calls do.
				auto lane     = worker_executor{ };
				auto task     = std::packaged_task<tour_cli_result<profile_admission>()>{ [&] { return admit_profile(Base, Profile); } };
				auto admitted = task.get_future();
				lane.post([&task] { task(); });
				auto admission = admitted.get();
				lane.close_and_drain();
				if (!admission) {
					return fail(admission.error().Code, admission.error().Message);
				}
				return std::move(*admission);
			}

			/// One request to the owner. Empty when no owner answered and none
			/// was sent anything: the caller looks for the profile's owner again.
			auto forward(const profile_admission& aAdmission, const cycle_invocation& aInvocation) -> std::optional<result<output>>
			{
				const auto again = " Read the cycle before repeating it, or repeat it with --operation-id " + aInvocation.Operation + " to get the owner's answer again.";
				if (aInvocation.Document && aInvocation.Document->size() > application::maximum_cycle_owner_document_bytes) {
					return fail("over_limit", "A running desktop owns this profile, and a response read from standard input reaches it only up to " + application::grouped_count(application::maximum_cycle_owner_document_bytes) + " bytes. Save it to a file and give --file PATH. Nothing was added.");
				}
				const auto payload = adapters::sqlite::encode_tour_request({ .Operation = application::tour_owner_operation::cycle, .Cycle = cycle_request_of(aInvocation) });
				if (payload.empty()) {
					return fail("over_limit", "This command is larger than the owner's channel carries; nothing was sent.");
				}
				auto stop    = std::stop_source{ };
				auto pending = std::async(std::launch::async, [&] { return adapters::sqlite::send_tour_owner(aAdmission.Root, aAdmission.Profile, payload, stop.get_token(), owner_answer_wait); });
				while (pending.wait_for(std::chrono::milliseconds{ 20 }) != std::future_status::ready) {
					if (tour_cancelled()) {
						stop.request_stop();
					}
				}
				const auto sent = pending.get();
				if (!sent) {
					switch (sent.error()) {
					case application::tour_channel_failure::unavailable:      return std::nullopt;
					case application::tour_channel_failure::cancelled:        return fail("cancelled", "Cancelled; nothing was sent.");
					case application::tour_channel_failure::version_mismatch: return fail("unsupported_version", "The BetterPullReview that owns this profile speaks another command protocol. Update it, or close it; nothing was sent.");
					case application::tour_channel_failure::refused:
						return fail("capability_unavailable", "The process that owns this profile did not admit this bpr (another user, profile or root); nothing was sent.");
					case application::tour_channel_failure::indeterminate: return fail("uncertain", "The owner did not answer; the command may have been applied." + again, "bpr cycle show" + aInvocation.named());
					}
				}
				const auto decoded = adapters::sqlite::decode_tour_response(*sent);
				if (!decoded) {
					return fail("uncertain", "The owner's answer could not be read; the command may have been applied." + again, "bpr cycle show" + aInvocation.named());
				}
				if (!decoded->Error.empty()) {
					if (decoded->Error == "owner_unavailable") {
						// It is closing: the next attempt finds it gone or its successor.
						return std::nullopt;
					}
					return fail("capability_unavailable", "The process that owns this profile refused the command (" + decoded->Error + "); nothing was changed.");
				}
				// An answer to another request is no answer to this one.
				if (!decoded->Cycle || decoded->Cycle->Operation != aInvocation.Operation || decoded->Cycle->Step != aInvocation.Step) {
					return fail("uncertain", "The owner answered another request; this command may have been applied." + again, "bpr cycle show" + aInvocation.named());
				}
				return cycle_result_of(*decoded->Cycle);
			}

			std::filesystem::path Base;
			std::string           Profile;
			std::uint64_t         Step = 0;
		};

		// ---- Commands that may ask a person something, then run.

		auto run_note(runner& aRunner, cycle_invocation aInvocation) -> result<output>
		{
			if (aInvocation.Text || !aInvocation.Interactive) {
				return aRunner.run(std::move(aInvocation));
			}
			// The target is checked first; the editor runs with no lease held.
			auto check  = aInvocation;
			check.Check = true;
			if (auto checked = aRunner.run(std::move(check)); !checked) {
				return std::unexpected(checked.error());
			}
			auto edited = run_tour_editor({ });
			if (!edited) {
				const auto& error = edited.error();
				if (error.Code == "editor_configuration" && error.Exit == 3) {
					return fail("interaction_required", "No trusted editor is configured: set BPR_TOUR_EDITOR_ARGV to a JSON argv array, or VISUAL/EDITOR to one absolute executable. Or give --body-file.",
								"bpr cycle note" + aInvocation.named() + " --body-file -");
				}
				return fail(error.Code == "cancelled" ? "cancelled" : "invalid_input", error.Message + " Nothing was saved.");
			}
			aInvocation.Text = edited->Text;
			cleanup_tour_editor(*edited);
			return aRunner.run(std::move(aInvocation));
		}

		auto run_freeze(runner& aRunner, cycle_invocation aInvocation) -> result<output>
		{
			if (aInvocation.has("--expected-revision") || !aInvocation.Interactive) {
				return aRunner.run(std::move(aInvocation));
			}
			// A person reads the round, then names it by typing; no lease is held meanwhile.
			auto read    = aInvocation;
			read.Command = core::cycle_command::review;
			read.Values.clear();
			auto shown = aRunner.run(std::move(read));
			if (!shown) {
				return std::unexpected(shown.error());
			}
			const auto revision = shown->Data.value("draft_revision").toString();
			std::cout << neutral_terminal_text(shown->Human).toUtf8().constData() << "\nType freeze to freeze revision " << utf8(revision) << " as " << utf8(shown->Data.value("round").toString())
					  << ", or press Enter to cancel: " << std::flush;
			auto answer = read_tour_terminal_line();
			if (!answer || *answer != "freeze") {
				return fail("cancelled", "Nothing was frozen.");
			}
			aInvocation.Values.insert("--expected-revision", revision);
			return aRunner.run(std::move(aInvocation));
		}

		auto run_export(runner& aRunner, const command& aCommand, cycle_invocation aInvocation) -> result<output>
		{
			const auto terminal = aInvocation.value("--output") == "-";
			if (terminal && aCommand.Json && aInvocation.Values.value("--format", "markdown") == "markdown") {
				return fail("invalid_input", "--output - prints the document itself, so it cannot share standard output with --json.");
			}
			auto exported = aRunner.run(aInvocation);
			if (!exported && exported.error().Code == "conflict" && aInvocation.Interactive && exported.error().Detail.contains("existing_digest")) {
				const auto& detail = exported.error().Detail;
				std::cout << utf8(detail.value("file").toString()) << " already holds other content (" << utf8(detail.value("existing_bytes").toString())
						  << " bytes).\nType replace to replace exactly that file, or press Enter to keep it: " << std::flush;
				auto answer = read_tour_terminal_line();
				if (!answer || *answer != "replace") {
					return fail("cancelled", "Nothing was written; " + utf8(detail.value("file").toString()) + " was kept.");
				}
				aInvocation.Values.insert("--replace", detail.value("existing_digest").toString());
				return aRunner.run(std::move(aInvocation));
			}
			if (!exported || !terminal) {
				return exported;
			}
			const auto& bytes = exported->Output;
			if (interactive_terminal()) {
				// A terminal shows controls neutralized: not the exact bytes, so not recorded as retrieved.
				std::cout << neutral_terminal_text(text(bytes)).toUtf8().constData() << std::flush;
				return output{ };
			}
			std::cout.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
			std::cout.flush();
			const auto written = std::cout.good();
			auto       record  = aInvocation;
			record.Command     = core::cycle_command::export_record;
			record.Values      = { { "--round", aInvocation.value("--round") }, { "--state", written ? "retrieved" : "failed" }, { "--digest", exported->Data.value("digest").toString() } };
			if (auto recorded = aRunner.run(std::move(record)); !recorded) {
				return std::unexpected(recorded.error());
			}
			if (!written) {
				return fail("uncertain", "Standard output refused part of the document; it may be incomplete.");
			}
			return output{ };
		}

		auto perform(const command& aCommand) -> result<output>
		{
			if (aCommand.Name == "capabilities") {
				return capabilities();
			}
			auto invocation = aCommand.Invocation;
			if (auto read = read_inputs(invocation); !read) {
				return std::unexpected(read.error());
			}
			auto every = runner{ aCommand.Base, aCommand.Profile };
			switch (invocation.Command) {
			case core::cycle_command::note:         return run_note(every, std::move(invocation));
			case core::cycle_command::freeze:       return run_freeze(every, std::move(invocation));
			case core::cycle_command::export_round: return run_export(every, aCommand, std::move(invocation));
			default:                                return every.run(std::move(invocation));
			}
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
				{ "operation_id", parsed ? text(parsed->Invocation.Operation) : QString{ } },
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
		return value ? 0 : cycle_exit_code(value.error().Code);
	}
}
