#ifndef BPR_APP_CYCLE_COMMANDS_HPP
#define BPR_APP_CYCLE_COMMANDS_HPP
#include "bpr/application/tour_owner_channel.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_command.hpp"
#include "cycle_session.hpp"
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
namespace bpr::app {
	/// \brief Why a cycle command did not complete: a stable code, the words a
	/// person reads, a next command and any facts the code has.
	struct cycle_failure
	{
		std::string Code, Message, Next;
		QJsonObject Detail;
	};
	template <typename T>
	using cycle_result = std::expected<T, cycle_failure>;

	/// \brief What a completed cycle command says.
	struct cycle_output
	{
		cycle_output() = default;
		cycle_output(QJsonObject aData, QString aHuman, const bool aComplete = true, std::string aOutput = { })
			: Data{ std::move(aData) }
			, Human{ std::move(aHuman) }
			, Complete{ aComplete }
			, Output{ std::move(aOutput) }
		{
		}

		QJsonObject Data;
		QString     Human;
		/// A page with more after it is not the complete answer.
		bool Complete = true;
		/// Exported bytes for the caller's own standard output.
		std::string Output;
		/// The cycle a writing command wrote, for windows showing it.
		std::optional<core::review_cycle_id> Changed;
	};

	/// \brief One `bpr cycle` command as both interfaces run it (BPR-1168):
	/// every path absolute, every chosen input already read, nothing left to
	/// ask a person.
	struct cycle_invocation
	{
		core::cycle_command Command = core::cycle_command::show;
		/// The cycle (RC-n), or for `open` the absolute location.
		QStringList Positional;
		/// The command's own options, as typed (a flag's value is "true").
		QMap<QString, QString> Values;
		/// The caller's operation identity and which of its requests this is.
		std::string   Operation;
		std::uint64_t Step = 0;
		/// The caller's absolute working folder.
		QString Directory;
		/// Note or goal text the caller read.
		std::optional<std::string> Text;
		/// A response document the caller read from standard input.
		std::optional<std::string> Document;
		/// Check a note's target without writing it.
		bool Check = false;
		/// A person at a terminal: a draft change may omit the revision it read.
		bool Interactive = false;

		[[nodiscard]] auto has(const QString& aOption) const -> bool { return Values.contains(aOption); }
		[[nodiscard]] auto value(const QString& aOption) const -> QString { return Values.value(aOption); }
		/// The cycle as the caller named it, for a next command.
		[[nodiscard]] auto named() const -> std::string;
	};

	/// \brief Where a command runs (BPR-1168): the session it opens, and what
	/// it checks before it reads or changes a round draft a window may hold.
	struct cycle_host
	{
		std::function<cycle_result<std::unique_ptr<cycle_session>>()>                 Open;
		std::function<cycle_result<void>(core::cycle_command, core::review_cycle_id)> Draft;
	};

	/// \brief Runs one command on the host's session. Never prompts, opens an
	/// editor or reads standard input: the caller did that already.
	[[nodiscard]] auto execute_cycle_command(const cycle_invocation& aInvocation, const cycle_host& aHost) -> cycle_result<cycle_output>;

	/// \brief Most entries one page of a list or comparison returns.
	inline constexpr std::size_t cycle_page_entries = 200U;
	/// \brief Most diff or source rows one file comparison prints.
	inline constexpr std::size_t cycle_file_rows = 2000U;

	/// \brief One exit status per kind of outcome, the same for human and JSON output.
	[[nodiscard]] auto cycle_exit_code(std::string_view aCode) -> int;

	/// \brief The invocation as the owner channel carries it, and back. The
	/// owner admits only the command's own registry options.
	[[nodiscard]] auto cycle_request_of(const cycle_invocation& aInvocation) -> application::cycle_owner_request;
	[[nodiscard]] auto cycle_invocation_of(const application::cycle_owner_request& aRequest) -> cycle_invocation;
	/// \brief A command's result as the owner channel carries it, and back.
	[[nodiscard]] auto cycle_response_of(const cycle_invocation& aInvocation, const cycle_result<cycle_output>& aResult) -> application::cycle_owner_response;
	[[nodiscard]] auto cycle_result_of(const application::cycle_owner_response& aResponse) -> cycle_result<cycle_output>;
}
#endif
