#!/usr/bin/env bash
# Runs in the station's scratch folder. Never shows a window: every widget is
# WA_DontShowOnScreen and is read back with QWidget::grab().
set -u
cd "$(dirname "$0")"
export PATH="/c/code/git/BetterPullReview/build/.cpm-cache/bprqt/6.12.0/6.12.0/msvc2022_64/bin:$PATH"
A='Bahnschrift SemiCondensed|9'
for scale in 1 1.25 1.5; do
  QT_SCALE_FACTOR=$scale QT_FONT_DPI=96 ./build/Release/title_fit.exe --mode render --out rows --ui 'Segoe UI|9' --a-row "$A" --a-name "$A" --a-key "$A" 2>&1 | tail -1
  QT_SCALE_FACTOR=$scale QT_FONT_DPI=96 ./build/Release/title_fit.exe --mode render --out rows --ui 'Segoe UI|9' --a-row "$A" --a-name "$A" --a-key "$A" --title 'Search results' --suffix -short 2>&1 | tail -1
done
ls rows | wc -l
