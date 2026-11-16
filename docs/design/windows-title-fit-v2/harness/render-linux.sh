#!/usr/bin/env bash
# Runs in the Linux station's scratch folder, with Qt's offscreen platform:
# no display is opened. Text is FreeType and fontconfig, as on a Linux desktop.
set -u
cd "$(dirname "$0")"
QT=/home/mmarcin/BetterPullReview/build/.cpm-cache/bprqt/6.12.0/6.12.0/gcc_64
export LD_LIBRARY_PATH="$QT/lib"
export QT_QPA_PLATFORM=offscreen
for scale in 1 1.5; do
  QT_SCALE_FACTOR=$scale QT_FONT_DPI=96 ./build/title_fit2 --out rows --ui 'Ubuntu Sans|11' 2>&1 | grep -v propagateSizeHints | tail -1
done
ls rows | wc -l
