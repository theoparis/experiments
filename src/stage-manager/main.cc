#include "stage-manager.h"

int main() {
  StageManager::XcbConnection xcbConnection(xcb_connect(nullptr, nullptr),
                                            xcb_disconnect);
  if (!xcbConnection) {
    throw std::runtime_error("Failed to connect to X11");
  }

  xcb_screen_iterator_t screens =
      xcb_setup_roots_iterator(xcb_get_setup(xcbConnection.get()));
  xcb_screen_t *screen = screens.data;

  StageManager::StageManager stageManager(xcbConnection.get(), screen);

  stageManager.run();

  return 0;
}
