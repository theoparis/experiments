#pragma once

#define VK_USE_PLATFORM_XCB_KHR
#include "include/core/SkCanvas.h"
#include "include/gpu/graphite/Context.h"
#include "include/gpu/graphite/Recorder.h"
#include "include/gpu/graphite/vk/VulkanGraphiteContext.h"
#include "include/gpu/vk/VulkanBackendContext.h"
#include "include/gpu/vk/VulkanExtensions.h"
#include "neora/window.h"
#include "vulkan/vulkan.h"

namespace StageManager {
// TODO: replace with display server agnostic connection
using XcbConnection =
    std::unique_ptr<xcb_connection_t, decltype(&xcb_disconnect)>;

class StageManager {
public:
  StageManager(xcb_connection_t *connection, xcb_screen_t *screen);
  ~StageManager();

  StageManager(const StageManager &) = delete;
  StageManager &operator=(const StageManager &) = delete;
  StageManager(StageManager &&) = delete;
  StageManager &operator=(StageManager &&) = delete;

  void run();

private:
  int width;
  int height;
  xcb_connection_t *connection;
  xcb_screen_t *screen;
  neora::Window window;
  VkInstance instance;
  VkDevice device;
  VkPhysicalDevice physicalDevice;
  VkQueue graphicsQueue;
  VkQueue presentQueue;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;

  struct SwapchainImage {
    VkImage image;
    VkSemaphore renderSemaphore;
    sk_sp<SkSurface> surface;
  };
  std::vector<SwapchainImage> images;
  uint32_t currentImageIndex;
  VkSemaphore acquireSemaphore;

  skgpu::VulkanExtensions skia_vk_extensions;
  skgpu::VulkanBackendContext backendContext;
  std::unique_ptr<skgpu::graphite::Context> skiaContext;
  std::unique_ptr<skgpu::graphite::Recorder> recorder;

  void createSwapchain();
  void initSwapchainImages();
  void draw();
  void drawStageStrip(SkCanvas *canvas);
};
} // namespace StageManager
