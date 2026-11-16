#!/usr/bin/env bash
# macOS: Cocoa, so the text is Core Text's, with every widget
# WA_DontShowOnScreen and read back with QWidget::grab(). Nothing is shown.
set -u
cd "$(dirname "$0")"
QT_QPA_PLATFORM=cocoa QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM=1 ./build/title_fit2 --out rows
