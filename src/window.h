#pragma once

#include "include/core/SkRect.h"
#include <xcb/xcb.h>

namespace neora {
class Window {
public:
  Window(xcb_connection_t *connection, xcb_screen_t *screen, int width,
         int height, bool disable_resize);
  SkRect getRect();
  xcb_window_t getHandle();

private:
  xcb_connection_t *connection;
  xcb_window_t handle;
};

xcb_visualtype_t *find_argb32_visual(xcb_connection_t *c, xcb_screen_t *screen);
} // namespace neora
