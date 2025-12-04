#include "neora/window.h"
#include <format>
#include <xcb/xcb_aux.h>
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

Window Window::Builder::build() { return Window(*this); }

Window::Window(const Builder &builder) : connection(builder.connection) {
  handle = xcb_generate_id(connection);
  xcb_screen_t *screen = builder.screen;
  int width = builder.width;
  int height = builder.height;

  xcb_visualtype_t *argb_visual = find_argb32_visual(connection, screen);
  if (!argb_visual) {
    // Fallback to default if no ARGB visual found
    xcb_depth_iterator_t depth_iter =
        xcb_screen_allowed_depths_iterator(screen);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
      if (depth_iter.data->depth == screen->root_depth) {
        xcb_visualtype_iterator_t visual_iter =
            xcb_depth_visuals_iterator(depth_iter.data);
        for (; visual_iter.rem; xcb_visualtype_next(&visual_iter)) {
          if (visual_iter.data->visual_id == screen->root_visual) {
            argb_visual = visual_iter.data;
            break;
          }
        }
        if (argb_visual)
          break;
      }
    }
  }

  if (!argb_visual) {
    throw std::runtime_error("Failed to find suitable visual");
  }

  xcb_colormap_t colormap = xcb_generate_id(connection);
  xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE, colormap,
                      screen->root, argb_visual->visual_id);

  uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK |
                  XCB_CW_COLORMAP;
  uint32_t values[] = {
      0, 0, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS, colormap};

  xcb_create_window(connection, 32, handle, screen->root, 0, 0, width, height,
                    0, XCB_WINDOW_CLASS_INPUT_OUTPUT, argb_visual->visual_id,
                    mask, values);

  if (!builder.decorations) {
    struct MwmHints {
      uint32_t flags;
      uint32_t functions;
      uint32_t decorations;
      int32_t input_mode;
      uint32_t status;
    };
    MwmHints hints = {};
    hints.flags = (1L << 1);
    hints.decorations = 0;

    xcb_intern_atom_cookie_t cookie =
        xcb_intern_atom(connection, 0, 15, "_MOTIF_WM_HINTS");
    xcb_intern_atom_reply_t *reply =
        xcb_intern_atom_reply(connection, cookie, NULL);
    if (reply) {
      xcb_change_property(connection, XCB_PROP_MODE_REPLACE, handle,
                          reply->atom, reply->atom, 32, 5, &hints);
      free(reply);
    }
  }

  xcb_map_window(connection, handle);
  xcb_flush(connection);

  if (builder.disable_resize) {
    xcb_size_hints_t hints = {};
    hints.flags =
        XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | XCB_ICCCM_SIZE_HINT_P_MAX_SIZE;
    hints.min_width = width;
    hints.min_height = height;
    hints.max_width = width;
    hints.max_height = height;
    xcb_icccm_set_wm_normal_hints(connection, handle, &hints);
  }
}

Window::~Window() {
  xcb_destroy_window(connection, handle);
  xcb_flush(connection);
}

xcb_generic_event_t *Window::getEvent() {
  xcb_generic_event_t *event = xcb_wait_for_event(connection);
  return event;
}

SkRect Window::getRect() {
  {
    xcb_generic_error_t *error = nullptr;
    xcb_get_geometry_cookie_t cookie = xcb_get_geometry(connection, handle);
    xcb_get_geometry_reply_t *reply =
        xcb_get_geometry_reply(connection, cookie, &error);

    if (error) {
      throw std::runtime_error(
          std::format("Failed to get window geometry: {}", error->error_code));
    }

    if (!reply) {
      return SkRect::MakeEmpty();
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
