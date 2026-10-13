#ifndef BPR_SRC_APP_INCLUDE_BPR_APP_COMMAND_LINE_HPP
#define BPR_SRC_APP_INCLUDE_BPR_APP_COMMAND_LINE_HPP
#include <QMap>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace bpr::app {
	/// \brief One `--name` a command line accepts.
	struct command_line_option
	{
		/// Without the leading `--`.
		std::string_view Name;
		bool             TakesValue = false;
	};

	/// \brief Why a command line was refused.
	enum class command_line_refusal : std::uint8_t {
		/// An undeclared option, a single-dash word, `--name=value` or `--`.
		unknown_option,
		/// A value option is last, or the word after it is itself an option.
		missing_value,
		/// An option appears more than once.
		repeated_option,
	};

	struct command_line_problem
	{
		command_line_refusal Refusal = command_line_refusal::unknown_option;
		/// The option as written, without any value.
		QString Option;
	};

	/// \brief A command line read once, by `QCommandLineParser`, against a
	/// table of flags and value options.
	///
	/// \details [[BPR-1437]]. Every BetterPullReview command line is read
	/// here: the desktop application, `bpr tour` and `bpr cycle`. Qt tokenizes;
	/// this adds the refusals Qt leaves to its caller. Qt takes whatever word
	/// follows a value option, so a value that is itself an option is refused
	/// as a missing value (`--ghes-host --new-window` is not a host). Qt
	/// collects a repeated option, which is refused. Qt also accepts
	/// `--name=value` and treats `--` as the end of options; no command line
	/// here has ever had either spelling, so both are unknown options.
	class command_line
	{
	public:
		/// \brief Reads `aArguments`, which do not include the program name.
		[[nodiscard]] static auto parse(const QStringList& aArguments, std::span<const command_line_option> aOptions)
			-> std::expected<command_line, command_line_problem>;

		/// \brief Whether `aOption`, written as `--name`, was given.
		[[nodiscard]] auto has(std::string_view aOption) const -> bool;
		/// \brief The value of a value option, written as `--name`, that was given.
		[[nodiscard]] auto value(std::string_view aOption) const -> std::optional<QString>;
		/// \brief Every option given, as `--name`, mapped to its value; a flag
		/// maps to `true`.
		[[nodiscard]] auto options() const -> const QMap<QString, QString>&;
		/// \brief The words that are neither options nor option values, in order.
		[[nodiscard]] auto words() const -> const QStringList&;

	private:
		QMap<QString, QString> Options;
		QStringList            Words;
	};
}
#endif
