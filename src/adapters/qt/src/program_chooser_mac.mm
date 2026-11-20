#include "program_chooser_mac.hpp"

#import <AppKit/AppKit.h>

#include <QObject>
#include <QWidget>

/// Refuses a plain folder at Open: only a file or an app bundle is a program.
/// The panel's own item metadata says which; nothing else is read.
@interface BPRProgramPanelDelegate : NSObject <NSOpenSavePanelDelegate>
@end

@implementation BPRProgramPanelDelegate
- (BOOL)panel:(id)aSender validateURL:(NSURL*)aURL error:(NSError**)aError
{
	static_cast<void>(aSender);
	NSNumber* folder = nil;
	[aURL getResourceValue:&folder forKey:NSURLIsDirectoryKey error:nil];
	if (!folder.boolValue || [aURL.pathExtension caseInsensitiveCompare:@"app"] == NSOrderedSame) {
		return YES;
	}
	if (aError != nullptr) {
		const auto words = QObject::tr("Choose a program or an app, not a folder.").toNSString();
		*aError          = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadInvalidFileNameError userInfo:@{ NSLocalizedDescriptionKey : words }];
	}
	return NO;
}
@end

namespace bpr::qt::detail {
	auto choose_program_mac(QWidget* aParent, const QString& aTitle, const QString& aStart) -> QString
	{
		static_cast<void>(aParent);
		@autoreleasepool {
			auto* const panel                     = [NSOpenPanel openPanel];
			auto* const delegate                  = [BPRProgramPanelDelegate new];
			panel.delegate                        = delegate;
			panel.title                           = aTitle.toNSString();
			panel.message                         = QObject::tr("Choose a program, or an app to use its main program. Apps open like folders, so a program inside one can be chosen too.").toNSString();
			panel.prompt                          = QObject::tr("Choose").toNSString();
			panel.canChooseFiles                  = YES;
			panel.canChooseDirectories            = YES;
			panel.treatsFilePackagesAsDirectories = YES;
			panel.allowsMultipleSelection         = NO;
			panel.resolvesAliases                 = YES;
			// The folder of the program already in the field, by its text alone.
			if (const auto start = aStart.trimmed(); start.startsWith(QLatin1Char{ '/' })) {
				panel.directoryURL = [NSURL fileURLWithPath:start.toNSString()].URLByDeletingLastPathComponent;
			}
			const auto  answer = [panel runModal];
			panel.delegate     = nil;
			NSURL* const chosen = panel.URLs.firstObject;
			if (answer != NSModalResponseOK || chosen == nil) {
				return { };
			}
			return QString::fromNSString(chosen.path);
		}
	}
} // namespace bpr::qt::detail
