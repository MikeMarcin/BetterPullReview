#ifndef BPR_APP_TERMINAL_DIAGNOSTICS_HPP
#define BPR_APP_TERMINAL_DIAGNOSTICS_HPP

#include <QIODevice>
#include <QTextStream>

#include <array>
#include <memory>
#include <string_view>

namespace bpr::app::detail {
	/// \brief Explicit process-owned terminal lifetime; close after producer quiescence.
	class terminal_diagnostics
	{
	public:
		terminal_diagnostics() noexcept;
		~terminal_diagnostics() noexcept;
		terminal_diagnostics(const terminal_diagnostics&)                    = delete;
		auto operator=(const terminal_diagnostics&) -> terminal_diagnostics& = delete;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
		QtMessageHandler                PreviousHandler = nullptr;
	};

	/// \brief Submits already classified application text without waiting for stderr.
	void record_terminal_diagnostic(std::string_view aText) noexcept;

	/// \brief One bounded terminal record. Oversized records are dropped whole.
	class terminal_text_stream
	{
	public:
		terminal_text_stream()
			: Stream{ &Buffer }
		{ }
		~terminal_text_stream()
		{
			Stream.flush();
			record_terminal_diagnostic(Buffer.text());
		}
		template <typename TValue>
		auto operator<<(const TValue& aValue) -> terminal_text_stream&
		{
			Stream << aValue;
			return *this;
		}
		operator QTextStream&() { return Stream; }

	private:
		class bounded_buffer final : public QIODevice
		{
		public:
			bounded_buffer() { open(QIODevice::WriteOnly); }
			auto text() const -> std::string_view { return Overflow ? std::string_view{ } : std::string_view{ Bytes.data(), Size }; }

		private:
			auto                    readData(char*, qint64) -> qint64 override { return -1; }
			auto                    writeData(const char* aData, qint64 aSize) -> qint64 override;
			std::array<char, 2048U> Bytes{ };
			std::size_t             Size     = 0U;
			bool                    Overflow = false;
		};
		bounded_buffer Buffer;
		QTextStream    Stream;
	};
} // namespace bpr::app::detail
#endif
