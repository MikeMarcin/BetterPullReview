#ifndef BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_DOCUMENT_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_DOCUMENT_HPP

#include <QByteArray>
#include <QTextDocument>
#include <QVariant>

namespace bpr::qt::detail {
	// No resource lookup reaches the filesystem, a parent browser, or either
	// resource provider. An invalid QVariant would permit provider fallback.
	class markdown_document final : public QTextDocument
	{
	public:
		using QTextDocument::QTextDocument;

	protected:
		auto loadResource(int, const QUrl&) -> QVariant override
		{
			return QByteArray{ };
		}
	};
} // namespace bpr::qt::detail

#endif
