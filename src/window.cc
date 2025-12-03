#include "window.h"
#include <xcb/xcb_icccm.h>
#include <xcb/xproto.h>

namespace neora {
xcb_visualtype_t *find_argb32_visual(xcb_connection_t *c,
                                     xcb_screen_t *screen) {
  xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(screen);

  for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
    xcb_visualtype_iterator_t visual_iter =
        xcb_depth_visuals_iterator(depth_iter.data);

    for (; visual_iter.rem; xcb_visualtype_next(&visual_iter)) {
      if (depth_iter.data->depth == 32 &&
          visual_iter.data->_class == XCB_VISUAL_CLASS_TRUE_COLOR) {
        return visual_iter.data;
      }
    }
  }
  return NULL;
}

Window::Window(xcb_connection_t *connection, xcb_screen_t *screen, int width,
               int height, bool disable_resize)
    : connection(connection) {
  handle = xcb_generate_id(connection);

  xcb_visualtype_t *argb_visual = find_argb32_visual(connection, screen);

  xcb_colormap_t colormap = xcb_generate_id(connection);
  xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE, colormap,
                      screen->root, argb_visual->visual_id);

  uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_COLORMAP |
                  XCB_CW_EVENT_MASK,
           value_mask = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS,
           values[] = {0, 0, colormap, value_mask};
  xcb_create_window(connection, screen->root_depth, handle, screen->root, 0, 0,
                    width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                    argb_visual->visual_id, mask, values);
  xcb_map_window(connection, handle);
  xcb_flush(connection);

  if (disable_resize) {
    xcb_size_hints_t hints;
    hints.flags =
        XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | XCB_ICCCM_SIZE_HINT_P_MAX_SIZE;
    hints.min_width = width;
    hints.min_height = height;
    hints.max_width = width;
    hints.max_height = height;
    xcb_icccm_set_wm_normal_hints(connection, handle, &hints);
  }
}

SkRect Window::getRect() {
  {
    xcb_generic_error_t *error;
    xcb_get_geometry_cookie_t cookie = xcb_get_geometry(connection, handle);
    xcb_get_geometry_reply_t *reply =
        xcb_get_geometry_reply(connection, cookie, &error);

    if (error) {
      fprintf(stderr, "Error getting window geometry: %d\n", error->error_code);
      free(error);
    }

    SkRect rect = {static_cast<float>(reply->x), static_cast<float>(reply->y),
                   static_cast<float>(reply->width),
                   static_cast<float>(reply->height)};
    free(reply);

    return rect;
  }
}

xcb_window_t Window::getHandle() { return handle; }

} // namespace neora
