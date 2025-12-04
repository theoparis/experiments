#include "stage-manager.h"
#include "include/core/SkColor.h"
#include "include/core/SkImage.h"
#include "include/core/SkM44.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkImageFilters.h"
#include "include/gpu/MutableTextureState.h"
#include "include/gpu/graphite/BackendSemaphore.h"
#include "include/gpu/graphite/ContextOptions.h"
#include "include/gpu/graphite/Surface.h"
#include "include/gpu/graphite/vk/VulkanGraphiteTypes.h"
#include "include/gpu/vk/VulkanBackendContext.h"
#include "include/gpu/vk/VulkanMutableTextureState.h"
#include "src/gpu/graphite/ContextOptionsPriv.h"
#include "vulkan/vulkan_core.h"
#include <cassert>
#include <cstdlib>
#include <memory>
#include <vector>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_renderutil.h>
#include <xcb/xproto.h>

namespace StageManager {

uint32_t findGraphicsQueueFamilyIndex(
    std::vector<VkQueueFamilyProperties> const &queueFamilyProperties) {
  std::vector<VkQueueFamilyProperties>::const_iterator
      graphicsQueueFamilyProperty = std::find_if(
          queueFamilyProperties.begin(), queueFamilyProperties.end(),
          [](VkQueueFamilyProperties const &qfp) {
            return qfp.queueFlags & VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT;
          });
  assert(graphicsQueueFamilyProperty != queueFamilyProperties.end());
  return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(),
                                             graphicsQueueFamilyProperty));
}

StageManager::StageManager(xcb_connection_t *connection, xcb_screen_t *screen)
    : connection(connection), screen(screen),
      width(screen->width_in_pixels / 10), height(screen->height_in_pixels),
      window(neora::Window::Builder(connection, screen, width, height)
                 .set_disable_resize(true)
                 .set_decorations(false)
                 .build()),
      instance(nullptr), physicalDevice(nullptr), device(nullptr),
      graphicsQueue(nullptr), presentQueue(nullptr), surface(nullptr),
      swapchain(nullptr), images(), currentImageIndex(0),
      acquireSemaphore(nullptr) {

  std::vector<const char *> layers{"VK_LAYER_KHRONOS_validation"};
  std::vector<const char *> instance_extensions{"VK_KHR_surface",
                                                "VK_KHR_xcb_surface"};
  std::vector<const char *> device_extensions{"VK_KHR_swapchain",
                                              "VK_KHR_dynamic_rendering"};

  VkApplicationInfo applicationInfo = {
      .pApplicationName = "Stage Manager",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_4,
  };

  VkInstanceCreateInfo instanceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &applicationInfo,
      .enabledLayerCount = static_cast<uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(instance_extensions.size()),
      .ppEnabledExtensionNames = instance_extensions.data(),
  };
  vkCreateInstance(&instanceCreateInfo, nullptr, &instance);

  VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
      .connection = connection,
      .window = window.getHandle(),
  };
  if (vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create surface");
  }

  uint32_t physicalDeviceCount;
  vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
  std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
  vkEnumeratePhysicalDevices(instance, &physicalDeviceCount,
                             physicalDevices.data());

  uint32_t selectedQueueFamilyIndex = -1;
  for (const auto &device : physicalDevices) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    uint32_t queueFamilyPropertyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyPropertyCount,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(
        queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyPropertyCount,
                                             queueFamilyProperties.data());

    for (uint32_t i = 0; i < queueFamilyPropertyCount; i++) {
      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

      bool hasGraphics =
          queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
      if (hasGraphics && presentSupport) {
        physicalDevice = device;
        selectedQueueFamilyIndex = i;
        break;
      }
    }
    if (physicalDevice) {
      break;
    }
  }

  if (!physicalDevice) {
    throw std::runtime_error(
        "Failed to find a suitable physical device and queue family");
  }

  float queuePriority = 1.0;
  VkDeviceQueueCreateInfo queueCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = selectedQueueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority,
  };
  VkDeviceCreateInfo deviceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueCreateInfo,
      .enabledLayerCount = static_cast<uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
      .ppEnabledExtensionNames = device_extensions.data(),
  };
  vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);

  backendContext.fGetProc = [](const char *name, VkInstance instance,
                               VkDevice device) {
    if (device != nullptr) {
      return vkGetDeviceProcAddr(device, name);
    }
    return vkGetInstanceProcAddr(instance, name);
  };

  vkGetDeviceQueue(device, selectedQueueFamilyIndex, 0, &graphicsQueue);
  vkGetDeviceQueue(device, selectedQueueFamilyIndex, 0, &presentQueue);

  backendContext.fDevice = device;
  backendContext.fGraphicsQueueIndex = selectedQueueFamilyIndex;
  backendContext.fMaxAPIVersion = VK_API_VERSION_1_4;
  backendContext.fVkExtensions = &skia_vk_extensions;
  backendContext.fQueue = graphicsQueue;
  backendContext.fInstance = instance;
  backendContext.fPhysicalDevice = physicalDevice;

  skia_vk_extensions.init(backendContext.fGetProc, instance, physicalDevice,
                          instance_extensions.size(),
                          instance_extensions.data(), device_extensions.size(),
                          device_extensions.data());

  skgpu::graphite::ContextOptions options;
  skgpu::graphite::ContextOptionsPriv optionsPriv;
  optionsPriv.fStoreContextRefInRecorder = true;
  options.fOptionsPriv = &optionsPriv;

  skiaContext =
      skgpu::graphite::ContextFactory::MakeVulkan(backendContext, options);
  if (!skiaContext) {
    throw std::runtime_error("Failed to create a skia context");
  }

  recorder = skiaContext->makeRecorder();

  if (!recorder) {
    throw std::runtime_error("Failed to make recorder");
  }

  createSwapchain();
  initSwapchainImages();
}

StageManager::~StageManager() {
  for (auto &image : images) {
    vkDestroySemaphore(device, image.renderSemaphore, nullptr);
  }

  vkDestroySemaphore(device, acquireSemaphore, nullptr);

  images.clear();
  recorder.reset();
  skiaContext.reset();

  vkDestroySwapchainKHR(device, swapchain, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroyInstance(instance, nullptr);
}

void StageManager::run() {
  while (true) {
    xcb_generic_event_t *event = window.getEvent();
    if (!event)
      break;
    if (event->response_type == XCB_EXPOSE) {
      draw();
    }
    free(event);
  }
}

void StageManager::createSwapchain() {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       nullptr);
  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       formats.data());

  VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                             static_cast<uint32_t>(height)};

  actualExtent.width =
      std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                 capabilities.maxImageExtent.width);
  actualExtent.height =
      std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                 capabilities.maxImageExtent.height);

  uint32_t imageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 &&
      imageCount > capabilities.maxImageCount) {
    imageCount = capabilities.maxImageCount;
  }

  VkCompositeAlphaFlagBitsKHR compositeAlpha =
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  if (capabilities.supportedCompositeAlpha &
      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
    compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
  } else if (capabilities.supportedCompositeAlpha &
             VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
    compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
  }

  VkSwapchainCreateInfoKHR swapchainCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .minImageCount = imageCount,
      .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
      .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
      .imageExtent = actualExtent,
      .imageArrayLayers = 1,
      .imageUsage =
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform = capabilities.currentTransform,
      .compositeAlpha = compositeAlpha,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create swapchain");
  }
}

void StageManager::initSwapchainImages() {
  uint32_t imageCount;
  vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
  std::vector<VkImage> vkImages(imageCount);
  vkGetSwapchainImagesKHR(device, swapchain, &imageCount, vkImages.data());

  images.resize(imageCount);

  VkSemaphoreCreateInfo semaphoreInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &acquireSemaphore) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create acquire semaphore");
  }

  for (size_t i = 0; i < imageCount; i++) {
    images[i].image = vkImages[i];
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                          &images[i].renderSemaphore) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create render semaphore");
    }

    skgpu::graphite::VulkanTextureInfo info;
    info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
    info.fFormat = VK_FORMAT_B8G8R8A8_UNORM;
    info.fImageUsageFlags =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    info.fSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.fFlags = 0;

    auto backendTex = skgpu::graphite::BackendTextures::MakeVulkan(
        {width, height}, info, VK_IMAGE_LAYOUT_UNDEFINED,
        backendContext.fGraphicsQueueIndex, images[i].image,
        skgpu::VulkanAlloc());

    images[i].surface = SkSurfaces::WrapBackendTexture(
        recorder.get(), backendTex, kBGRA_8888_SkColorType,
        SkColorSpace::MakeSRGB(), nullptr);

    if (!images[i].surface) {
      throw std::runtime_error(
          "Failed to create SkSurface for swapchain image");
    }
  }
}

void StageManager::draw() {
  if (vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquireSemaphore,
                            VK_NULL_HANDLE, &currentImageIndex) != VK_SUCCESS) {
    // Handle swapchain recreation if needed, for now just throw or return
    return;
  }

  SkCanvas *canvas = images[currentImageIndex].surface->getCanvas();
  canvas->clear(SkColors::kTransparent); // Clear before drawing
  drawStageStrip(canvas);

  std::unique_ptr<skgpu::graphite::Recording> recording = recorder->snap();
  if (!recording) {
    return;
  }

  skgpu::graphite::InsertRecordingInfo info = {};
  info.fRecording = recording.get();
  info.fTargetSurface = images[currentImageIndex].surface.get();

  skgpu::MutableTextureState presentState =
      skgpu::MutableTextureStates::MakeVulkan(
          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, backendContext.fGraphicsQueueIndex);
  info.fTargetTextureState = &presentState;

  skgpu::graphite::BackendSemaphore waitSemaphore =
      skgpu::graphite::BackendSemaphores::MakeVulkan(acquireSemaphore);
  info.fNumWaitSemaphores = 1;
  info.fWaitSemaphores = &waitSemaphore;

  skgpu::graphite::BackendSemaphore signalSemaphore =
      skgpu::graphite::BackendSemaphores::MakeVulkan(
          images[currentImageIndex].renderSemaphore);
  info.fNumSignalSemaphores = 1;
  info.fSignalSemaphores = &signalSemaphore;

  skiaContext->insertRecording(info);
  skiaContext->submit(skgpu::graphite::SyncToCpu::kNo);

  VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &images[currentImageIndex].renderSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &currentImageIndex,
  };

  vkQueuePresentKHR(presentQueue, &presentInfo);
  vkQueueWaitIdle(presentQueue); // Simple sync for now
}

void StageManager::drawStageStrip(SkCanvas *canvas) {
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

    canvas->save();

    // 3D Rotation
    SkM44 matrix;
    matrix.setIdentity();

    float centerX = x + stageWidth / 2.0f;
    float centerY = y + stageHeight / 2.0f;

    // Perspective
    SkM44 perspective;
    perspective.setIdentity();
    perspective.setRow(3, {0, 0, -1.0f / 800.0f, 1}); // Camera at z=800

    matrix.postConcat(perspective);
    matrix.preTranslate(centerX, centerY, 0);
    matrix.preConcat(
        SkM44::Rotate({0, 1, 0}, 0.3f)); // Rotate ~23 degrees around Y
    matrix.preTranslate(-centerX, -centerY, 0);

    canvas->concat(matrix);

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
    float iconX = x - 10;
    float iconY = y + stageHeight - iconSize / 2;

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

    canvas->restore();
  }
}
} // namespace StageManager
