#include "bpr/qt/sensitive_text_input.hpp"

#include <QByteArray>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QString>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace bpr::qt {
	namespace {
		void erase_bytes(std::byte* const aBytes, std::size_t aSize) noexcept
		{
			auto* cursor = reinterpret_cast<volatile unsigned char*>(aBytes);
			while (aSize != 0U) {
				*cursor = 0U;
				++cursor;
				--aSize;
			}
		}

		void erase(QString& aText) noexcept
		{
			if (!aText.isEmpty()) {
				aText.fill(QChar{ });
			}
			aText.clear();
			aText.squeeze();
		}

		void erase(QByteArray& aBytes) noexcept
		{
			if (!aBytes.isEmpty()) {
				aBytes.fill('\0');
			}
			aBytes.clear();
			aBytes.squeeze();
		}

		void erase(QLineEdit& aEditor)
		{
			const auto length = aEditor.text().size();
			if (length > 0) {
				aEditor.setText(QString{ length, QChar{ } });
			}
			aEditor.clear();
		}

		void show_unavailable_warning(QWidget* const aParent)
		{
			QMessageBox warning{
				QMessageBox::Warning,
				QStringLiteral("Credential unavailable"),
				QStringLiteral(
					"The credential was empty, exceeded the supported limit, "
					"or could not be retained securely."),
				QMessageBox::Ok,
				aParent
			};
			warning.setObjectName(
				QStringLiteral("bpr-sensitive-text-unavailable-warning"));
			warning.setTextFormat(Qt::PlainText);
			warning.setTextInteractionFlags(Qt::NoTextInteraction);
			warning.exec();
		}

		[[nodiscard]] auto valid_maximum(const std::size_t aMaximumBytes) noexcept
			-> bool
		{
			constexpr auto maximumLineEditLength = static_cast<std::size_t>((std::numeric_limits<int>::max)());
			return aMaximumBytes != 0U
				&& aMaximumBytes <= maximumLineEditLength;
		}
	}

	sensitive_bytes::sensitive_bytes(
		std::unique_ptr<std::byte[]> aBytes,
		const std::size_t            aSize) noexcept
		: Bytes{ std::move(aBytes) }
		, Size{ aSize }
	{
	}

	sensitive_bytes::~sensitive_bytes() noexcept
	{
		reset();
	}

	sensitive_bytes::sensitive_bytes(sensitive_bytes&& aOther) noexcept
		: Bytes{ std::move(aOther.Bytes) }
		, Size{ std::exchange(aOther.Size, 0U) }
	{
	}

	auto sensitive_bytes::operator=(sensitive_bytes&& aOther) noexcept
		-> sensitive_bytes&
	{
		if (this == &aOther) {
			return *this;
		}

		reset();
		Bytes = std::move(aOther.Bytes);
		Size  = std::exchange(aOther.Size, 0U);
		return *this;
	}

	auto sensitive_bytes::bytes() const noexcept -> std::span<const std::byte>
	{
		return { Bytes.get(), Size };
	}

	auto sensitive_bytes::size() const noexcept -> std::size_t
	{
		return Size;
	}

	auto sensitive_bytes::empty() const noexcept -> bool
	{
		return Size == 0U;
	}

	void sensitive_bytes::reset() noexcept
	{
		erase_bytes(Bytes.get(), Size);
		Bytes.reset();
		Size = 0U;
	}

	auto prompt_for_sensitive_text(
		const std::size_t aMaximumBytes,
		QWidget* const    aParent) -> std::optional<sensitive_bytes>
	{
		if (!valid_maximum(aMaximumBytes)) {
			show_unavailable_warning(aParent);
			return std::nullopt;
		}

		QDialog dialog{ aParent };
		dialog.setObjectName(QStringLiteral("bpr-sensitive-text-prompt"));
		dialog.setWindowTitle(QStringLiteral("Authenticate GitHub host"));
		dialog.setModal(true);

		QVBoxLayout layout{ &dialog };
		QLabel      label{
			QStringLiteral(
				"Enter a GitHub credential for this connection. "
				"It will not be written to settings or logs."),
			&dialog
		};
		label.setWordWrap(true);

		QLineEdit editor{ &dialog };
		editor.setObjectName(QStringLiteral("bpr-sensitive-text-editor"));
		editor.setAccessibleName(QStringLiteral("GitHub credential"));
		editor.setEchoMode(QLineEdit::Password);
		editor.setMaxLength(static_cast<int>(aMaximumBytes));
		editor.setInputMethodHints(
			Qt::ImhHiddenText | Qt::ImhNoPredictiveText);

		QDialogButtonBox buttons{
			QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
			&dialog
		};
		QObject::connect(
			&buttons,
			&QDialogButtonBox::accepted,
			&dialog,
			&QDialog::accept);
		QObject::connect(
			&buttons,
			&QDialogButtonBox::rejected,
			&dialog,
			&QDialog::reject);

		layout.addWidget(&label);
		layout.addWidget(&editor);
		layout.addWidget(&buttons);
		editor.setFocus();

		const auto accepted = dialog.exec() == QDialog::Accepted;
		auto       text     = editor.text();
		erase(editor);
		if (!accepted) {
			erase(text);
			return std::nullopt;
		}

		auto utf8 = text.toUtf8();
		erase(text);
		const auto byteCount = static_cast<std::size_t>(utf8.size());
		if (byteCount == 0U || byteCount > aMaximumBytes) {
			erase(utf8);
			show_unavailable_warning(aParent);
			return std::nullopt;
		}

		auto owned = std::unique_ptr<std::byte[]>{
			new (std::nothrow) std::byte[byteCount]
		};
		if (owned == nullptr) {
			erase(utf8);
			show_unavailable_warning(aParent);
			return std::nullopt;
		}

		const auto input = std::as_bytes(std::span{
			utf8.constData(),
			static_cast<std::size_t>(utf8.size()) });
		std::ranges::copy(input, owned.get());
		erase(utf8);
		return sensitive_bytes{ std::move(owned), byteCount };
	}
}
