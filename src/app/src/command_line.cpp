#include "bpr/app/command_line.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>

namespace bpr::app {
	namespace {
		auto text(const std::string_view aName) -> QString
		{
			return QString::fromLatin1(aName.data(), static_cast<qsizetype>(aName.size()));
		}

		/// Qt reports option names without their dashes.
		auto as_written(const QString& aName) -> QString
		{
			return (aName.size() == 1 ? QStringLiteral("-") : QStringLiteral("--")) + aName;
		}

		auto refuse(const command_line_refusal aRefusal, QString aOption) -> std::unexpected<command_line_problem>
		{
			return std::unexpected(command_line_problem{ .Refusal = aRefusal, .Option = std::move(aOption) });
		}
	}

	auto command_line::parse(const QStringList& aArguments, const std::span<const command_line_option> aOptions)
		-> std::expected<command_line, command_line_problem>
	{
		for (const auto& argument : aArguments) {
			if (argument == QStringLiteral("--") || (argument.startsWith(QStringLiteral("--")) && argument.contains(u'='))) {
				return refuse(command_line_refusal::unknown_option, argument.left(argument.indexOf(u'=')));
			}
		}
		auto parser = QCommandLineParser{ };
		for (const auto& option : aOptions) {
			auto declared = QCommandLineOption{ text(option.Name) };
			if (option.TakesValue) {
				declared.setValueName(QStringLiteral("value"));
			}
			parser.addOption(declared);
		}
		// Qt expects the program name first and skips it.
		auto arguments = QStringList{ QString{ } };
		arguments.append(aArguments);
		if (!parser.parse(arguments)) {
			const auto unknown = parser.unknownOptionNames();
			if (!unknown.isEmpty()) {
				return refuse(command_line_refusal::unknown_option, as_written(unknown.front()));
			}
			// With `=` refused above, Qt's only other refusal is a value option
			// given last.
			return refuse(command_line_refusal::missing_value, aArguments.back());
		}
		auto       result = command_line{ };
		const auto given  = parser.optionNames();
		for (const auto& name : given) {
			if (given.count(name) > 1) {
				return refuse(command_line_refusal::repeated_option, as_written(name));
			}
		}
		for (const auto& option : aOptions) {
			const auto name = text(option.Name);
			if (!parser.isSet(name)) {
				continue;
			}
			auto value = option.TakesValue ? parser.value(name) : QStringLiteral("true");
			if (option.TakesValue && value.startsWith(QStringLiteral("--"))) {
				return refuse(command_line_refusal::missing_value, as_written(name));
			}
			result.Options.insert(as_written(name), std::move(value));
		}
		result.Words = parser.positionalArguments();
		return result;
	}

	auto command_line::has(const std::string_view aOption) const -> bool
	{
		return Options.contains(text(aOption));
	}

	auto command_line::value(const std::string_view aOption) const -> std::optional<QString>
	{
		const auto found = Options.constFind(text(aOption));
		return found == Options.cend() ? std::nullopt : std::optional{ *found };
	}

	auto command_line::options() const -> const QMap<QString, QString>&
	{
		return Options;
	}

	auto command_line::words() const -> const QStringList&
	{
		return Words;
	}
}
