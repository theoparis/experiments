#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkRRect.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkGradientShader.h"
#include "include/effects/SkImageFilters.h"
#include "include/encode/SkPngEncoder.h"
#include <iostream>
#include <vector>

class StageManager {
public:
  StageManager(int width, int height) : width_(width), height_(height) {
    surface_ = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  }

  void draw() {
    if (!surface_)
      return;
    SkCanvas *canvas = surface_->getCanvas();

    // 1. Draw Background (Wallpaper-like gradient)
    drawBackground(canvas);

    // 2. Draw Main Window (Center)
    drawMainWindow(canvas);

    // 3. Draw Stage Manager Strip (Left)
    drawStageStrip(canvas);
  }

  bool save(const char *filename) {
    if (!surface_)
      return false;
    sk_sp<SkImage> image = surface_->makeImageSnapshot();
    if (!image)
      return false;

    SkFILEWStream stream(filename);
    if (!stream.isValid()) {
      std::cerr << "Could not open file for writing: " << filename << std::endl;
      return false;
    }

    SkPngEncoder::Options options;
    SkPixmap pixmap;
    if (!image->peekPixels(&pixmap)) {
      std::cerr << "Could not peek pixels from image\n";
      return false;
    }
    return SkPngEncoder::Encode(&stream, pixmap, options);
  }

private:
  int width_;
  int height_;
  sk_sp<SkSurface> surface_;

  void drawBackground(SkCanvas *canvas) {
    // Draw a nice sunset-like gradient to match the vibe
    SkPaint paint;
    SkPoint points[2] = {{0, 0}, {0, (float)height_}};
    SkColor colors[3] = {
        SkColorSetRGB(60, 40, 80),   // Deep purple top
        SkColorSetRGB(150, 80, 100), // Pinkish middle
        SkColorSetRGB(40, 30, 60)    // Dark bottom
    };
    paint.setShader(SkGradientShader::MakeLinear(points, colors, nullptr, 3,
                                                 SkTileMode::kClamp));
    canvas->drawPaint(paint);
  }

  void drawMainWindow(SkCanvas *canvas) {
    SkPaint paint;
    paint.setAntiAlias(true);

    // Shadow
    paint.setColor(SkColorSetARGB(100, 0, 0, 0));
    paint.setImageFilter(SkImageFilters::Blur(20.0, 2.0, nullptr));
    SkRect rect = SkRect::MakeXYWH(350, 100, width_ - 450, height_ - 200);
    canvas->drawRoundRect(rect, 20, 20, paint);
    paint.setImageFilter(nullptr);

    // Window Content
    paint.setColor(SkColorSetRGB(30, 30, 35)); // Dark theme window
    canvas->drawRoundRect(rect, 12, 12, paint);

    // Header bar
    SkRect header = SkRect::MakeXYWH(350, 100, width_ - 450, 40);
    SkRRect headerRRect;
    SkVector radii[4] = {{12, 12}, {12, 12}, {0, 0}, {0, 0}};
    headerRRect.setRectRadii(header, radii);
    paint.setColor(SkColorSetRGB(50, 50, 55));
    canvas->drawRRect(headerRRect, paint);

    // Window Controls (Traffic lights)
    paint.setColor(SkColorSetRGB(255, 95, 87)); // Red
    canvas->drawCircle(370, 120, 6, paint);
    paint.setColor(SkColorSetRGB(255, 189, 46)); // Yellow
    canvas->drawCircle(390, 120, 6, paint);
    paint.setColor(SkColorSetRGB(40, 200, 64)); // Green
    canvas->drawCircle(410, 120, 6, paint);
  }

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
         SkColorSetRGB(100, 100, 100)}, // Light window
        {SkColorSetRGB(50, 50, 60),
         SkColorSetRGB(80, 120, 200)}, // Dark IDE-like
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
      canvas->drawCircle(iconX + iconSize / 2, iconY + iconSize / 2,
                         iconSize / 2, paint);
      paint.setImageFilter(nullptr);

      // Icon body
      paint.setColor(stages[i].iconColor);
      canvas->drawCircle(iconX + iconSize / 2, iconY + iconSize / 2,
                         iconSize / 2, paint);
    }
  }
};

#include "include/effects/SkGradientShader.h"

int main() {
  StageManager stage_manager(1920, 1200);
  stage_manager.draw();

  if (stage_manager.save("stage_manager.png")) {
    std::cout << "Successfully rendered to stage_manager.png" << std::endl;
  } else {
    std::cerr << "Failed to render image." << std::endl;
    return 1;
  }

  return 0;
}
