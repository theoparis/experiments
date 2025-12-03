#include "stage-strip.h"
#include "include/core/SkColor.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkImageFilters.h"
#include <vector>

namespace StageManager {
void drawStageStrip(SkCanvas *canvas) {
  // Draw 4 stages on the left
  float startY = 100;
  float gap = 20;
  float stageHeight = 180;
  float stageWidth = 240;
  float x = 40; // Left margin

  struct StageInfo {
    SkColor color;
    SkColor iconColor;
  };

  std::vector<StageInfo> stages = {
      {SkColorSetRGB(200, 200, 220),
       SkColorSetRGB(100, 100, 100)},                           // Light window
      {SkColorSetRGB(50, 50, 60), SkColorSetRGB(80, 120, 200)}, // Dark IDE-like
      {SkColorSetRGB(40, 40, 40), SkColorSetRGB(200, 100, 100)}, // Media app
      {SkColorSetRGB(220, 220, 220), SkColorSetRGB(50, 150, 50)} // Browser
  };

  for (size_t i = 0; i < stages.size(); ++i) {
    float y = startY + i * (stageHeight + gap);

    // Draw the thumbnail
    SkPaint paint;
    paint.setAntiAlias(true);

    // Perspective/Skew effect (simplified as a scale for now, or just rect)
    // Stage Manager thumbnails are slightly 3D, but we'll do flat for minimal
    // recreation

    // Shadow
    paint.setColor(SkColorSetARGB(80, 0, 0, 0));
    paint.setImageFilter(SkImageFilters::Blur(10.0, 1.0, nullptr));
    SkRect rect = SkRect::MakeXYWH(x, y, stageWidth, stageHeight);
    canvas->drawRoundRect(rect, 10, 10, paint);
    paint.setImageFilter(nullptr);

    // Content
    paint.setColor(stages[i].color);
    canvas->drawRoundRect(rect, 8, 8, paint);

    // App Icon (floating on the left of the thumbnail)
    float iconSize = 32;
    float iconX = x - 20; // Overlapping or to the left
    float iconY = y + (stageHeight - iconSize) / 2;

    // In actual Stage Manager, icon is to the left of the thumbnail strip
    // usually, or overlaying. In the image, it looks like icons are on the
    // left.

    // Let's place icon at the bottom left of the group
    iconX = x - 10;
    iconY = y + stageHeight - iconSize / 2;

    // Icon shadow
    paint.setColor(SkColorSetARGB(60, 0, 0, 0));
    paint.setImageFilter(SkImageFilters::Blur(4.0, 1.0, nullptr));
    canvas->drawCircle(iconX + iconSize / 2, iconY + iconSize / 2, iconSize / 2,
                       paint);
    paint.setImageFilter(nullptr);

    // Icon body
    paint.setColor(stages[i].iconColor);
    canvas->drawCircle(iconX + iconSize / 2, iconY + iconSize / 2, iconSize / 2,
                       paint);
  }
}
} // namespace StageManager
