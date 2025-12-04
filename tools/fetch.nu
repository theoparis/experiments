#!/usr/bin/env nu

mkdir third-party
cd third-party
gix clone https://skia.googlesource.com/skia
gix clone https://github.com/KhronosGroup/Vulkan-Headers
gix clone https://github.com/KhronosGroup/Vulkan-Loader
gix clone https://github.com/KhronosGroup/SPIRV-Headers
gix clone https://github.com/KhronosGroup/SPIRV-Tools
gix clone https://gixlab.freedesktop.org/freetype/freetype
gix clone https://gixlab.freedesktop.org/xorg/lib/libxcb
gix clone https://github.com/aomediacodec/libavif
gix clone https://github.com/pnggroup/libpng
gix clone https://github.com/google/dawn
gix clone https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
cd -
