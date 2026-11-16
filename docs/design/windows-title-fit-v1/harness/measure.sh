#!/usr/bin/env bash
# Runs in the station's scratch folder. Never shows a window: every widget is
# WA_DontShowOnScreen and is read back with QWidget::grab().
set -u
cd "$(dirname "$0")"
export PATH="/c/code/git/BetterPullReview/build/.cpm-cache/bprqt/6.12.0/6.12.0/msvc2022_64/bin:$PATH"
FACES='Segoe UI 9=Segoe UI|9;Segoe UI 8.5=Segoe UI|8.5;Segoe UI 8=Segoe UI|8;Segoe UI Semibold 9=Segoe UI|9|600;Segoe UI Semibold 8.5=Segoe UI|8.5|600;Segoe UI Semibold 8=Segoe UI|8|600;Segoe UI Variable Text 9=Segoe UI Variable Text|9;Segoe UI Variable Small 9=Segoe UI Variable Small|9;Segoe UI Variable Display 9=Segoe UI Variable Display|9;Segoe UI Variable Display Semibold 9=Segoe UI Variable Display|9|600;Segoe UI Variable Display 8.5=Segoe UI Variable Display|8.5;Segoe UI Variable Text 8.5=Segoe UI Variable Text|8.5;Segoe UI Variable opsz36 9=Segoe UI Variable|9||36;Segoe UI Variable opsz10.5 9=Segoe UI Variable|9||10.5;Segoe UI Variable opsz5 9=Segoe UI Variable|9||5;Bahnschrift SemiCondensed 9=Bahnschrift SemiCondensed|9;Bahnschrift SemiCondensed 9.5=Bahnschrift SemiCondensed|9.5;Bahnschrift Condensed 9.5=Bahnschrift Condensed|9.5;Bahnschrift 9=Bahnschrift|9;Tahoma 8.25=Tahoma|8.25;Segoe UI 9 tracked -0.3=Segoe UI|9|||-0.3'
for scale in 1 1.25 1.5; do
  QT_SCALE_FACTOR=$scale QT_FONT_DPI=96 ./build/Release/title_fit.exe --mode measure --out faces --faces "$FACES" 2>&1 | tail -3
done
ls faces | wc -l
