#pragma once
#include <cstdint>
#include <cstddef>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace nvn {

struct Device {
    int id{1};
    bool initialized{false};
    uint32_t flags{0};
};

struct DeviceBuilder {
    uint32_t flags{0};
};

struct Queue {
    Device* device{nullptr};
    int priority{1000};
    size_t control_memory_size{0x80000};
    size_t command_memory_size{0x10000};
    size_t compute_memory_size{0x10000};
    size_t flush_threshold{0x10000};
    uint32_t flags{0};
    void* queue_memory{nullptr};
    size_t queue_memory_size{0};
    bool initialized{false};
};

struct QueueBuilder {
    Device* device{nullptr};
    int priority{1000};
    size_t control_memory_size{0x80000};
    size_t command_memory_size{0x10000};
    size_t compute_memory_size{0x10000};
    size_t flush_threshold{0x10000};
    uint32_t flags{0};
    void* queue_memory{nullptr};
    size_t queue_memory_size{0x80000};
};

struct Window {
    Device* device{nullptr};
    HWND hwnd{nullptr};
    int present_interval{1};
    int num_textures{2};
    int active_texture{0};
    uint64_t total_presents{0};
    std::chrono::high_resolution_clock::time_point last_present_time{};
    bool initialized{false};

    // Swapchain presentation surface
    HDC mem_dc{nullptr};
    HBITMAP dib_bitmap{nullptr};
    HBITMAP old_bitmap{nullptr};
    void* pixel_buffer{nullptr};
    int width{1280};
    int height{720};
};

struct WindowBuilder {
    Device* device{nullptr};
    void* native_window{nullptr};
    int num_textures{2};
    int present_interval{1};
    void* textures[8]{nullptr};
};

struct Texture {
    int width{1280};
    int height{720};
    int target{0};
    int format{0};
};

struct TextureBuilder {
    int width{1280};
    int height{720};
    int target{0};
    int format{0};
};

struct MemoryPool {
    size_t size{0};
    int flags{0};
    void* memory{nullptr};
};

struct MemoryPoolBuilder {
    size_t size{0};
    int flags{0};
    void* memory{nullptr};
};

struct Buffer {
    size_t size{0};
    uint64_t gpu_address{0};
};

struct BufferBuilder {
    size_t size{0};
    Device* device{nullptr};
};

struct CommandBuffer {
    size_t command_memory_size{0x10000};
    size_t control_memory_size{0x10000};
};

struct Program {
    int id{0};
};

struct Sync {
    int id{0};
};

// Funções do Device
void nvnDeviceBuilderSetDefaults(DeviceBuilder* builder);
void nvnDeviceBuilderSetFlags(DeviceBuilder* builder, uint32_t flags);
uint32_t nvnDeviceBuilderGetFlags(const DeviceBuilder* builder);
int nvnDeviceInitialize(Device* device, const DeviceBuilder* builder);
void nvnDeviceFinalize(Device* device);
int nvnDeviceGetInteger(Device* device, int property, int* out_value);
uint64_t nvnDeviceGetCurrentTimestampInNanoseconds(Device* device);

// Funções da Queue (Fila de comandos da GPU)
void nvnQueueBuilderSetDefaults(QueueBuilder* builder);
void nvnQueueBuilderSetDevice(QueueBuilder* builder, Device* device);
void nvnQueueBuilderSetFlags(QueueBuilder* builder, uint32_t flags);
void nvnQueueBuilderSetQueuePriority(QueueBuilder* builder, int priority);
void nvnQueueBuilderSetCommandMemorySize(QueueBuilder* builder, size_t size);
void nvnQueueBuilderSetComputeMemorySize(QueueBuilder* builder, size_t size);
void nvnQueueBuilderSetControlMemorySize(QueueBuilder* builder, size_t size);
void nvnQueueBuilderSetQueueMemory(QueueBuilder* builder, void* memory, size_t size);
void nvnQueueBuilderSetFlushThreshold(QueueBuilder* builder, size_t threshold);
int nvnQueueInitialize(Queue* queue, const QueueBuilder* builder);
void nvnQueueFinalize(Queue* queue);
void nvnQueueSubmitCommands(Queue* queue, int count, void* const* cmd_buffers);
void nvnQueueFinish(Queue* queue);
void nvnQueueFlush(Queue* queue);

// Funções da Window (Janela de Apresentação / Swapchain)
void nvnWindowBuilderSetDefaults(WindowBuilder* builder);
void nvnWindowBuilderSetDevice(WindowBuilder* builder, Device* device);
void nvnWindowBuilderSetNativeWindow(WindowBuilder* builder, void* native_window);
void nvnWindowBuilderSetTextures(WindowBuilder* builder, int count, void** textures);
void nvnWindowBuilderSetPresentInterval(WindowBuilder* builder, int interval);
int nvnWindowInitialize(Window* window, const WindowBuilder* builder);
void nvnWindowFinalize(Window* window);
void nvnWindowSetPresentInterval(Window* window, int interval);
int nvnWindowAcquireTexture(Window* window, Sync* sync, int* out_texture_idx);
void nvnQueuePresentTexture(Queue* queue, Window* window, int texture_idx);

// Bootstrap Loader da NVN
extern "C" void* nvnBootstrapLoader(const char* name);

} // namespace nvn

namespace nv {
int InitializeGraphics(void* address, size_t size);
void SetGraphicsAllocator(void* allocate, void* free, void* realloc, void* user_data);
}
