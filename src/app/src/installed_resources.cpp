#include "installed_resources.hpp"

#include <QByteArray>
#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QResource>

#include <utility>

#if defined(BPR_EXTERNAL_QT_RESOURCES)
	#if !defined(_WIN32)
		#error "Only the tests-disabled Windows image keeps its Qt resources outside itself"
	#endif
	// Generated beside the bundle by cmake/BprQtResourceBundleDigest.cmake.
	#include "better_pull_review_resources_sha256.hpp"

	#include <array>

	#include <windows.h>
#endif

namespace bpr::app {
	namespace {
		// The bundle is about 3.5 MB. Anything far larger is not this bundle,
		// and is refused before it is read into memory.
		constexpr qint64 maximum_bundle_bytes = 64LL * 1024 * 1024;
	}

	struct installed_resources::registration
	{
		QByteArray Bytes;
	};

	void installed_resources::registration_release::operator()(registration* aRegistration) const noexcept
	{
		if (aRegistration == nullptr) {
			return;
		}
		QResource::unregisterResource(reinterpret_cast<const uchar*>(aRegistration->Bytes.constData()));
		delete aRegistration;
	}

	auto installed_resources::registered() const noexcept -> bool
	{
		return Registration != nullptr;
	}

	auto installed_resources_refusal_name(const installed_resources_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case installed_resources_refusal::missing:
			return "missing";
		case installed_resources_refusal::unreadable:
			return "unreadable";
		case installed_resources_refusal::digest_mismatch:
			return "digest-mismatch";
		case installed_resources_refusal::not_a_resource_file:
			return "not-a-resource-file";
		}
		return "unknown";
	}

	auto load_installed_resources(const QString& aPath, const std::string_view aExpectedSha256)
		-> std::expected<installed_resources, installed_resources_refusal>
	{
		QFile file{ aPath };
		if (!file.exists()) {
			return std::unexpected{ installed_resources_refusal::missing };
		}
		const auto size = file.size();
		if (!file.open(QIODevice::ReadOnly) || size <= 0 || size > maximum_bundle_bytes) {
			return std::unexpected{ installed_resources_refusal::unreadable };
		}
		// The registered bytes are the ones hashed: read once, then kept for
		// as long as the registration lasts.
		auto registration   = std::make_unique<installed_resources::registration>();
		registration->Bytes = file.read(size);
		if (registration->Bytes.size() != size || !file.atEnd()) {
			return std::unexpected{ installed_resources_refusal::unreadable };
		}
		const auto digest = QCryptographicHash::hash(registration->Bytes, QCryptographicHash::Sha256).toHex();
		if (QByteArrayView{ digest } != QByteArrayView{ aExpectedSha256.data(), static_cast<qsizetype>(aExpectedSha256.size()) }) {
			return std::unexpected{ installed_resources_refusal::digest_mismatch };
		}
		if (!QResource::registerResource(reinterpret_cast<const uchar*>(registration->Bytes.constData()))) {
			return std::unexpected{ installed_resources_refusal::not_a_resource_file };
		}
		auto result = installed_resources{ };
		result.Registration.reset(registration.release());
		return result;
	}

	auto load_image_resources() -> std::expected<installed_resources, installed_resources_refusal>
	{
#if defined(BPR_EXTERNAL_QT_RESOURCES)
		// Before QApplication, so the executable's directory comes from the
		// loader rather than from QCoreApplication.
		std::array<wchar_t, 32768> buffer{ };
		const auto                 size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (size == 0U || size >= buffer.size()) {
			return std::unexpected{ installed_resources_refusal::missing };
		}
		const auto directory = QFileInfo{ QString::fromWCharArray(buffer.data(), static_cast<qsizetype>(size)) }.absolutePath();
		const auto name      = QString::fromLatin1(installed_resources_file_name.data(), static_cast<qsizetype>(installed_resources_file_name.size()));
		return load_installed_resources(QDir{ directory }.filePath(name), BPR_QT_RESOURCE_BUNDLE_SHA256);
#else
		return installed_resources{ };
#endif
	}
}
