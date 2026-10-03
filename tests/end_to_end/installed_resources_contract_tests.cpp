// [[BPR-0974]]. A tests-disabled Windows image keeps its Qt resources -- the
// application icon, the Dif strips and the syntax definitions -- in one bundle
// beside the executable, and refuses to start unless the bundle is exactly the
// one it was built with. That image has no tests, so this contract holds the
// same bundle, built by the same rule in the tests-enabled Windows graph, and
// the application's own registration code, compiled as the image compiles it:
//
// - the digest the image compiles in is the bundle's;
// - a missing, unreadable, altered or foreign file is refused, and nothing is
//   registered;
// - the bundle beside the executable registers, and every resource the
//   application reads is there with the bytes it was built from: nine icon
//   sizes, ten uncompressed Dif strips, KSyntaxHighlighting's own syntax
//   collection, index included, and the four compressed faces of the bundled
//   code font ([[BPR-1270]]);
// - releasing the owner unregisters it.
//
// This executable links Qt Core and nothing that embeds a resource, so every
// path it finds comes from the bundle.
#include "../../src/app/src/installed_resources.hpp"

#include "better_pull_review_resources_sha256.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QResource>
#include <QString>

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace {
	using bpr::app::installed_resources_refusal;

	constexpr auto icon_resource   = std::string_view{ ":/icons/16x16/better_pull_review.png" };
	constexpr auto syntax_root     = std::string_view{ ":/org.kde.syntax-highlighting/syntax" };
	constexpr auto expected_digest = std::string_view{ BPR_QT_RESOURCE_BUNDLE_SHA256 };

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto text(const std::string_view aValue) -> QString
	{
		return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
	}

	auto file_bytes(const QString& aPath) -> QByteArray
	{
		QFile file{ aPath };
		expect(file.open(QIODevice::ReadOnly), "a source or bundled file opens");
		return file.readAll();
	}

	void write_file(const QString& aPath, const QByteArray& aBytes)
	{
		QFile file{ aPath };
		expect(file.open(QIODevice::WriteOnly) && file.write(aBytes) == aBytes.size(), "a scratch file is written");
	}

	auto sha256(const QByteArray& aBytes) -> QByteArray
	{
		return QCryptographicHash::hash(aBytes, QCryptographicHash::Sha256).toHex();
	}

	// Under the runner's temporary directory, whose inherited entries the
	// container can use; a QTemporaryDir's owner-only entries it cannot.
	class scratch_directory final
	{
	public:
		scratch_directory()
		{
			Path = std::filesystem::temp_directory_path()
				/ ("bpr-0974-installed-resources-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
			expect(std::filesystem::create_directories(Path), "a scratch directory is created");
		}
		scratch_directory(const scratch_directory&)                    = delete;
		auto operator=(const scratch_directory&) -> scratch_directory& = delete;
		~scratch_directory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}

		[[nodiscard]] auto file_path(const std::string_view aName) const -> QString
		{
			return QString::fromStdWString((Path / aName).wstring());
		}

	private:
		std::filesystem::path Path;
	};

	auto refusal_of(const QString& aPath, const std::string_view aDigest) -> std::optional<installed_resources_refusal>
	{
		const auto loaded = bpr::app::load_installed_resources(aPath, aDigest);
		if (loaded) {
			return std::nullopt;
		}
		return loaded.error();
	}

	void the_image_compiles_in_the_bundles_digest()
	{
		const auto bundle = file_bytes(QStringLiteral(BPR_TEST_RESOURCE_BUNDLE));
		expect(bundle.startsWith("qres"), "the bundle is a binary Qt resource file");
		expect(sha256(bundle) == QByteArray{ expected_digest.data(), static_cast<qsizetype>(expected_digest.size()) },
			   "the compiled digest is the built bundle's SHA-256");
		expect(file_bytes(QStringLiteral(BPR_TEST_IMAGE_BUNDLE)) == bundle, "the bundle beside the executable is the built one");
	}

	void anything_but_the_built_bundle_is_refused()
	{
		const scratch_directory scratch;
		const auto              bundle = file_bytes(QStringLiteral(BPR_TEST_RESOURCE_BUNDLE));

		expect(refusal_of(scratch.file_path("absent.rcc"), expected_digest) == installed_resources_refusal::missing,
			   "a missing bundle is refused as missing");

		expect(QDir{ }.mkpath(scratch.file_path("directory.rcc")), "a directory stands where the bundle belongs");
		expect(refusal_of(scratch.file_path("directory.rcc"), expected_digest) == installed_resources_refusal::unreadable,
			   "a directory in the bundle's place is refused as unreadable");

		write_file(scratch.file_path("empty.rcc"), QByteArray{ });
		expect(refusal_of(scratch.file_path("empty.rcc"), expected_digest) == installed_resources_refusal::unreadable,
			   "an empty bundle is refused as unreadable");

		auto altered                = bundle;
		altered[altered.size() / 2] = static_cast<char>(altered[altered.size() / 2] ^ 0x01);
		write_file(scratch.file_path("altered.rcc"), altered);
		expect(refusal_of(scratch.file_path("altered.rcc"), expected_digest) == installed_resources_refusal::digest_mismatch,
			   "a bundle altered by one bit is refused as a digest mismatch");

		write_file(scratch.file_path("truncated.rcc"), bundle.first(bundle.size() - 1));
		expect(refusal_of(scratch.file_path("truncated.rcc"), expected_digest) == installed_resources_refusal::digest_mismatch,
			   "a truncated bundle is refused as a digest mismatch");

		// Named by its own digest, so only Qt's resource system can refuse it.
		const auto foreign = QByteArray{ "not a Qt resource file, though its digest is the expected one" };
		write_file(scratch.file_path("foreign.rcc"), foreign);
		const auto foreignDigest = sha256(foreign).toStdString();
		expect(refusal_of(scratch.file_path("foreign.rcc"), foreignDigest) == installed_resources_refusal::not_a_resource_file,
			   "a file that is not a Qt resource file is refused even when its digest is expected");

		expect(!QFile::exists(text(icon_resource)), "no refusal registered anything");
	}

	void the_bundle_beside_the_executable_carries_every_resource()
	{
		{
			const auto loaded = bpr::app::load_image_resources();
			expect(loaded.has_value(), "the bundle beside the executable registers");
			expect(loaded->registered(), "the owner holds the registration");

			for (const auto size : { 16, 24, 32, 40, 48, 64, 128, 256, 512 }) {
				const auto name = QStringLiteral("%1x%1/better_pull_review.png").arg(size);
				expect(file_bytes(QStringLiteral(":/icons/") + name) == file_bytes(QStringLiteral(BPR_TEST_SOURCE_ROOT "/packaging/icons/png/") + name),
					   "every icon size is the packaged PNG");
			}

			constexpr auto strips = std::array{ "rest", "calm-idle", "greet", "blink", "compare-glance", "consider", "comment-ready", "attention", "acknowledge", "blocked" };
			for (const auto* const strip : strips) {
				const auto name     = QStringLiteral("dif-%1.png").arg(QLatin1StringView{ strip });
				const auto resource = QResource{ QStringLiteral(":/bpr/dif/") + name };
				expect(resource.isValid(), "every accepted Dif strip is bundled");
				expect(resource.compressionAlgorithm() == QResource::NoCompression, "every Dif strip is stored uncompressed, as the runtime requires");
				expect(QByteArray::fromRawData(reinterpret_cast<const char*>(resource.data()), resource.size())
						   == file_bytes(QStringLiteral(BPR_TEST_SOURCE_ROOT "/src/adapters/qt/assets/dif/generated/") + name),
					   "every Dif strip is the accepted generated product");
			}
			expect(QDir{ QStringLiteral(":/bpr/dif") }.entryList(QDir::Files).size() == static_cast<qsizetype>(strips.size()),
				   "the bundle holds exactly the ten accepted strips");

			const auto collection = file_bytes(QStringLiteral(BPR_TEST_SYNTAX_COLLECTION));
			expect(file_bytes(text(syntax_root) + QStringLiteral("/index.katesyntax")) == file_bytes(QStringLiteral(BPR_TEST_SYNTAX_INDEX)),
				   "the syntax index is KSyntaxHighlighting's own");
			const auto definitions = QDir{ text(syntax_root) }.entryList(QDir::Files);
			expect(definitions.size() > 100 && definitions.size() == collection.count("<file "),
				   "every definition KSyntaxHighlighting's collection names is bundled");
			expect(definitions.contains(QStringLiteral("cpp.xml")) && definitions.contains(QStringLiteral("python.xml")),
				   "the definitions include the common languages");

			// [[BPR-1270]]. The bundled code font's four verified faces.
			constexpr auto faces = std::array{ "Regular", "Bold", "Italic", "BoldItalic" };
			for (const auto* const face : faces) {
				const auto name     = QStringLiteral("JetBrainsMonoNLNerdFont-%1.ttf").arg(QLatin1StringView{ face });
				const auto resource = QResource{ QStringLiteral(":/bpr/fonts/") + name };
				expect(resource.isValid(), "every code font face is bundled");
				expect(resource.compressionAlgorithm() != QResource::NoCompression, "every code font face is stored compressed");
				expect(file_bytes(QStringLiteral(":/bpr/fonts/") + name) == file_bytes(QStringLiteral(BPR_TEST_CODE_FONT_DIRECTORY "/") + name),
					   "every code font face is the provider's verified face");
			}
			expect(QDir{ QStringLiteral(":/bpr/fonts") }.entryList(QDir::Files).size() == static_cast<qsizetype>(faces.size()),
				   "the bundle holds exactly the four faces");
		}
		expect(!QFile::exists(text(icon_resource)), "releasing the owner unregisters the bundle");
	}
}

auto main() -> int
{
	the_image_compiles_in_the_bundles_digest();
	anything_but_the_built_bundle_is_refused();
	the_bundle_beside_the_executable_carries_every_resource();
	std::cout << "installed-resources-contract:pass\n";
	return EXIT_SUCCESS;
}
