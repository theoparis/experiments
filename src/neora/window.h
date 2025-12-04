#pragma once

#include "include/core/SkRect.h"
#include <xcb/xcb.h>

namespace neora {
class Window {
public:
  struct Builder {
    xcb_connection_t *connection;
    xcb_screen_t *screen;
    int width;
    int height;
    bool disable_resize = false;
    bool decorations = true;

    Builder(xcb_connection_t *connection, xcb_screen_t *screen, int width,
            int height)
        : connection(connection), screen(screen), width(width), height(height) {
    }

    Builder &set_disable_resize(bool disable) {
      disable_resize = disable;
      return *this;
    }

    Builder &set_decorations(bool enable) {
      decorations = enable;
      return *this;
    }

    Window build();
  };

  Window(const Builder &builder);
  ~Window();
  SkRect getRect();
  xcb_window_t getHandle();

  xcb_generic_event_t *getEvent();

private:
  xcb_connection_t *connection;
  xcb_window_t handle;
};

xcb_visualtype_t *find_argb32_visual(xcb_connection_t *c, xcb_screen_t *screen);
} // namespace neora
