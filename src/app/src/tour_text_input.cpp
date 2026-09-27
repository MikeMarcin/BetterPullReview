#include "tour_text_input.hpp"
#include "bpr/core/review_preparation.hpp"
#include "tour_cancellation.hpp"
#include <QFile>
#include <QFileInfo>
#if !defined(_WIN32)
	#include <cerrno>
	#include <poll.h>
	#include <unistd.h>
#endif
namespace bpr::app {
	auto read_tour_text_input(const QString& aPath) -> tour_cli_result<std::string>
	{
		QByteArray bytes;
#if !defined(_WIN32)
		if (aPath == "-") {
			while (bytes.size() <= static_cast<qsizetype>(core::maximum_tour_field_bytes)) {
				if (tour_cancelled()) {
					return std::unexpected(tour_cli_error{ "cancelled", "Text input was cancelled; the draft is unchanged.", 130 });
				}
				pollfd     input{ STDIN_FILENO, POLLIN, 0 };
				const auto status = ::poll(&input, 1, 100);
				if (status < 0 && errno == EINTR) {
					continue;
				}
				if (status < 0 || (input.revents & POLLNVAL)) {
					return std::unexpected(tour_cli_error{ "input_unavailable", "Standard input could not be read.", 7 });
				}
				if (!status) {
					continue;
				}
				char       buffer[4096];
				const auto count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
				if (count < 0 && errno == EINTR) {
					continue;
				}
				if (count < 0) {
					return std::unexpected(tour_cli_error{ "input_unavailable", "Standard input could not be read.", 7 });
				}
				if (!count) {
					break;
				}
				bytes.append(buffer, static_cast<qsizetype>(count));
			}
		} else
#endif
		{
			QFile      input;
			const bool opened = aPath == "-" ? input.open(stdin, QIODevice::ReadOnly, QFileDevice::DontCloseHandle) : (input.setFileName(aPath), input.open(QIODevice::ReadOnly));
			if (!opened) {
				return std::unexpected(tour_cli_error{ "input_unavailable", "The explicitly selected text input could not be opened.", 7 });
			}
			bytes = input.read(static_cast<qint64>(core::maximum_tour_field_bytes + 1));
			if (input.error() != QFileDevice::NoError) {
				return std::unexpected(tour_cli_error{ "input_unavailable", "The text input could not be read completely.", 7 });
			}
		}
		if (tour_cancelled()) {
			return std::unexpected(tour_cli_error{ "cancelled", "Text input was cancelled; the draft is unchanged.", 130 });
		}
		if (bytes.size() > static_cast<qsizetype>(core::maximum_tour_field_bytes)) {
			return std::unexpected(tour_cli_error{ "input_too_large", "A Markdown field is limited to 65536 UTF-8 bytes.", 2 });
		}
		const auto value = bytes.toStdString();
		if (!core::valid_writing_utf8(value) || value.find('\0') != std::string::npos) {
			return std::unexpected(tour_cli_error{ "invalid_utf8", "The text must be valid UTF-8 without NUL bytes.", 2 });
		}
		return value;
	}
}
