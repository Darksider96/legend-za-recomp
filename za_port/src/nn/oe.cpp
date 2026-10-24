#include "nn/oe.h"
#include "nn/diag.h"
#include <queue>
#include <mutex>

namespace nn::oe {

namespace {
    std::queue<uint32_t> g_message_queue;
    std::mutex g_oe_mutex;
}

bool TryPopNotificationMessage(uint32_t* outMessage) {
    if (!outMessage) return false;
    std::lock_guard<std::mutex> lock(g_oe_mutex);
    if (g_message_queue.empty()) {
        return false;
    }
    *outMessage = g_message_queue.front();
    g_message_queue.pop();
    NN_LOG("[nn::oe] TryPopNotificationMessage -> 0x%x\n", *outMessage);
    return true;
}

void PushNotificationMessage(uint32_t message) {
    std::lock_guard<std::mutex> lock(g_oe_mutex);
    g_message_queue.push(message);
    NN_LOG("[nn::oe] PushNotificationMessage(0x%x)\n", message);
}

} // namespace nn::oe
