#include "../window.h"
#include "include/core/SkSurface.h"
#include "stage-strip.h"
#include <cstdlib>
#include <memory>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_renderutil.h>
#include <xcb/xproto.h>

using XcbConnection =
    std::unique_ptr<xcb_connection_t, decltype(&xcb_disconnect)>;

int main() {
  XcbConnection xcb_connection(xcb_connect(nullptr, nullptr), &xcb_disconnect);
  if (!xcb_connection) {
    return EXIT_FAILURE;
  }
  xcb_connection_t *connection = xcb_connection.get();

  xcb_screen_iterator_t screens =
      xcb_setup_roots_iterator(xcb_get_setup(connection));
  xcb_screen_t *screen = screens.data;

  int width = screen->width_in_pixels / 10;
  int height = screen->height_in_pixels;
  neora::Window window(connection, screen, width, height, true);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  if (!surface) {
    fprintf(stderr, "Couldn't create a skia surface!\n");
    return EXIT_FAILURE;
  }
  SkCanvas *canvas = surface->getCanvas();

  StageManager::drawStageStrip(canvas);

  sk_sp<SkImage> img(surface->makeImageSnapshot());
  if (!img) {
    fprintf(stderr, "Couldn't create a skia surface snapshot!\n");
    return EXIT_FAILURE;
  }

  SkPixmap pixmap;
  if (!img->peekPixels(&pixmap)) {
    fprintf(stderr, "Couldn't create a skia pixmap!\n");
    return EXIT_FAILURE;
  }

  xcb_visualtype_t *argb_visual = neora::find_argb32_visual(connection, screen);
  if (!argb_visual) {
    fprintf(stderr, "No ARGB visual found.\n");
    return EXIT_FAILURE;
  }

  xcb_render_pictformat_t pictformat = 0;
  {
    xcb_render_query_pict_formats_cookie_t pf_cookie =
        xcb_render_query_pict_formats(connection);
    xcb_render_query_pict_formats_reply_t *pf_reply =
        xcb_render_query_pict_formats_reply(connection, pf_cookie, nullptr);

    xcb_render_pictscreen_iterator_t ps_iter =
        xcb_render_query_pict_formats_screens_iterator(pf_reply);

    for (; ps_iter.rem; xcb_render_pictscreen_next(&ps_iter)) {
      xcb_render_pictdepth_iterator_t pd_iter =
          xcb_render_pictscreen_depths_iterator(ps_iter.data);

      for (; pd_iter.rem; xcb_render_pictdepth_next(&pd_iter)) {
        xcb_render_pictvisual_iterator_t pv_iter =
            xcb_render_pictdepth_visuals_iterator(pd_iter.data);

        for (; pv_iter.rem; xcb_render_pictvisual_next(&pv_iter)) {
          if (pv_iter.data->visual == argb_visual->visual_id) {
            pictformat = pv_iter.data->format;
            break;
          }
        }
      }
    }
    free(pf_reply);
  }

  xcb_pixmap_t xcb_pixmap = xcb_generate_id(connection);
  xcb_create_pixmap(connection, 32, xcb_pixmap, window.getHandle(), width,
                    height);

  xcb_gcontext_t gc = xcb_generate_id(connection);
  uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
  uint32_t values[2];
  values[0] = 0;
  values[1] = 0;
  xcb_create_gc(connection, gc, xcb_pixmap, mask, values);
  xcb_image_t *xcb_image = xcb_image_create_native(
      connection, width, height, XCB_IMAGE_FORMAT_Z_PIXMAP, screen->root_depth,
      nullptr, pixmap.rowBytes() * height, nullptr);
  if (!xcb_image) {
    return EXIT_FAILURE;
  }

  xcb_image->data = (uint8_t *)malloc(pixmap.rowBytes() * height);
  if (xcb_image->data) {
    memcpy(xcb_image->data, pixmap.addr(), pixmap.rowBytes() * height);
  } else {
    xcb_image_destroy(xcb_image);
    return EXIT_FAILURE;
  }

  xcb_image_put(connection, xcb_pixmap, gc, xcb_image, 0, 0, 0);
  xcb_image_destroy(xcb_image);

  xcb_render_picture_t source_picture = xcb_generate_id(connection);
  xcb_render_create_picture(connection, source_picture, window.getHandle(),
                            pictformat, 0, nullptr);

  xcb_render_picture_t dest_picture = xcb_generate_id(connection);
  xcb_render_create_picture(connection, dest_picture, window.getHandle(),
                            pictformat, 0, nullptr);

  xcb_generic_event_t *event;
  while ((event = xcb_wait_for_event(connection))) {
    switch (event->response_type & ~0x80) {
    case XCB_EXPOSE: {
      xcb_expose_event_t *expose_event = (xcb_expose_event_t *)event;

      xcb_render_composite(connection, XCB_RENDER_PICT_OP_OVER, source_picture,
                           XCB_RENDER_PICTURE_NONE, dest_picture, 0, 0, 0, 0,
                           expose_event->x, expose_event->y,
                           expose_event->width, expose_event->height);
      xcb_flush(connection);
    } break;
    case XCB_BUTTON_PRESS:
      goto end;
    default:
      break;
    }
    free(event);
  }

end:
  xcb_free_pixmap(connection, xcb_pixmap);

  return 0;
}
