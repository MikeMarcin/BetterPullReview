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

namespace bpr::qt
{
namespace
{
void erase_bytes(std::byte* const bytes, std::size_t size) noexcept
{
    auto* cursor = reinterpret_cast<volatile unsigned char*>(bytes);
    while(size != 0U)
    {
        *cursor = 0U;
        ++cursor;
        --size;
    }
}

void erase(QString& text) noexcept
{
    if(!text.isEmpty())
    {
        text.fill(QChar{});
    }
    text.clear();
    text.squeeze();
}

void erase(QByteArray& bytes) noexcept
{
    if(!bytes.isEmpty())
    {
        bytes.fill('\0');
    }
    bytes.clear();
    bytes.squeeze();
}

void erase(QLineEdit& editor)
{
    const auto length = editor.text().size();
    if(length > 0)
    {
        editor.setText(QString{length, QChar{}});
    }
    editor.clear();
}

void show_unavailable_warning(QWidget* const parent)
{
    QMessageBox warning{
        QMessageBox::Warning,
        QStringLiteral("Credential unavailable"),
        QStringLiteral(
            "The credential was empty, exceeded the supported limit, "
            "or could not be retained securely."),
        QMessageBox::Ok,
        parent};
    warning.setObjectName(
        QStringLiteral("bpr-sensitive-text-unavailable-warning"));
    warning.setTextFormat(Qt::PlainText);
    warning.setTextInteractionFlags(Qt::NoTextInteraction);
    warning.exec();
}

[[nodiscard]] auto valid_maximum(const std::size_t maximum_bytes) noexcept
    -> bool
{
    constexpr auto maximum_line_edit_length =
        static_cast<std::size_t>((std::numeric_limits<int>::max)());
    return maximum_bytes != 0U
        && maximum_bytes <= maximum_line_edit_length;
}
}

sensitive_bytes::sensitive_bytes(
    std::unique_ptr<std::byte[]> bytes,
    const std::size_t size) noexcept
    : Bytes{std::move(bytes)}
    , Size{size}
{
}

sensitive_bytes::~sensitive_bytes() noexcept
{
    reset();
}

sensitive_bytes::sensitive_bytes(sensitive_bytes&& other) noexcept
    : Bytes{std::move(other.Bytes)}
    , Size{std::exchange(other.Size, 0U)}
{
}

auto sensitive_bytes::operator=(sensitive_bytes&& other) noexcept
    -> sensitive_bytes&
{
    if(this == &other)
    {
        return *this;
    }

    reset();
    Bytes = std::move(other.Bytes);
    Size = std::exchange(other.Size, 0U);
    return *this;
}

auto sensitive_bytes::bytes() const noexcept -> std::span<const std::byte>
{
    return {Bytes.get(), Size};
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
    const std::size_t maximum_bytes,
    QWidget* const parent) -> std::optional<sensitive_bytes>
{
    if(!valid_maximum(maximum_bytes))
    {
        show_unavailable_warning(parent);
        return std::nullopt;
    }

    QDialog dialog{parent};
    dialog.setObjectName(QStringLiteral("bpr-sensitive-text-prompt"));
    dialog.setWindowTitle(QStringLiteral("Authenticate GitHub host"));
    dialog.setModal(true);

    QVBoxLayout layout{&dialog};
    QLabel label{
        QStringLiteral(
            "Enter a GitHub credential for this connection. "
            "It will not be written to settings or logs."),
        &dialog};
    label.setWordWrap(true);

    QLineEdit editor{&dialog};
    editor.setObjectName(QStringLiteral("bpr-sensitive-text-editor"));
    editor.setAccessibleName(QStringLiteral("GitHub credential"));
    editor.setEchoMode(QLineEdit::Password);
    editor.setMaxLength(static_cast<int>(maximum_bytes));
    editor.setInputMethodHints(
        Qt::ImhHiddenText | Qt::ImhNoPredictiveText);

    QDialogButtonBox buttons{
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog};
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
    auto text = editor.text();
    erase(editor);
    if(!accepted)
    {
        erase(text);
        return std::nullopt;
    }

    auto utf8 = text.toUtf8();
    erase(text);
    const auto byte_count = static_cast<std::size_t>(utf8.size());
    if(byte_count == 0U || byte_count > maximum_bytes)
    {
        erase(utf8);
        show_unavailable_warning(parent);
        return std::nullopt;
    }

    auto owned = std::unique_ptr<std::byte[]>{
        new(std::nothrow) std::byte[byte_count]};
    if(owned == nullptr)
    {
        erase(utf8);
        show_unavailable_warning(parent);
        return std::nullopt;
    }

    const auto input = std::as_bytes(std::span{
        utf8.constData(),
        static_cast<std::size_t>(utf8.size())});
    std::ranges::copy(input, owned.get());
    erase(utf8);
    return sensitive_bytes{std::move(owned), byte_count};
}
}
