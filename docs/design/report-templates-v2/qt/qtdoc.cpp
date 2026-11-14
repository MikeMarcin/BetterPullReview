// Throwaway: lays out one HTML file with QTextDocument (what the report tab's body uses) and saves it as a PNG.
// Offscreen only (QT_QPA_PLATFORM=offscreen). Not product code.
#include <QColor>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTextDocument>
int main(int argc, char** argv)
{
	QGuiApplication app(argc, argv);
	if (argc < 5) { return 2; }
	QFile file(QString::fromLocal8Bit(argv[1]));
	if (!file.open(QIODevice::ReadOnly)) { return 3; }
	QTextDocument doc;
	doc.setDocumentMargin(0);
	QFont font(QStringLiteral("Charter"));
	font.setPixelSize(17);
	doc.setDefaultFont(font);
	if (argc > 5) { doc.setDefaultStyleSheet(QStringLiteral("body{color:%1}").arg(QString::fromLocal8Bit(argv[5]))); }
	doc.setHtml(QString::fromUtf8(file.readAll()));
	const int width = QString::fromLocal8Bit(argv[3]).toInt();
	doc.setTextWidth(width);
	const qreal dpr = 2.0;
	QImage image(QSize(int(width * dpr), int(doc.size().height() * dpr)), QImage::Format_ARGB32_Premultiplied);
	image.setDevicePixelRatio(dpr);
	image.fill(QColor(QString::fromLocal8Bit(argv[4])));
	QPainter painter(&image);
	doc.drawContents(&painter);
	painter.end();
	return image.save(QString::fromLocal8Bit(argv[2])) ? 0 : 4;
}
