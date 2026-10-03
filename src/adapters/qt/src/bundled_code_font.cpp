#include "bpr/qt/bundled_code_font.hpp"

#include <QFontDatabase>

#include <vector>

namespace bpr::qt {
	auto bundled_code_font_family() -> QString
	{
		return QStringLiteral("JetBrains Mono NL");
	}

	auto bundled_code_font_files() -> QStringList
	{
		return {
			QStringLiteral(":/bpr/fonts/JetBrainsMonoNL-Regular.ttf"),
			QStringLiteral(":/bpr/fonts/JetBrainsMonoNL-Bold.ttf"),
			QStringLiteral(":/bpr/fonts/JetBrainsMonoNL-Italic.ttf"),
			QStringLiteral(":/bpr/fonts/JetBrainsMonoNL-BoldItalic.ttf"),
		};
	}

	auto register_code_font_files(const QStringList& aFiles, const QString& aFamily) -> bool
	{
		auto added = std::vector<int>{ };
		auto whole = !aFiles.isEmpty();
		for (const auto& file : aFiles) {
			const auto id = QFontDatabase::addApplicationFont(file);
			if (id < 0) {
				whole = false;
				break;
			}
			added.push_back(id);
			if (!QFontDatabase::applicationFontFamilies(id).contains(aFamily)) {
				whole = false;
				break;
			}
		}
		if (!whole) {
			for (const auto id : added) {
				QFontDatabase::removeApplicationFont(id);
			}
		}
		return whole;
	}

	auto register_bundled_code_font() -> bool
	{
		return register_code_font_files(bundled_code_font_files(), bundled_code_font_family());
	}

	auto bundled_code_font_available() -> bool
	{
		return QFontDatabase::hasFamily(bundled_code_font_family());
	}
} // namespace bpr::qt
