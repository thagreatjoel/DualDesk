#include "dualdesk/input/raw_input_handler.h"
#include "dualdesk/core/logger.h"

namespace dualdesk {

RawInputHandler::RawInputHandler() {
    LOG_DEBUG("RawInputHandler created");
}

RawInputHandler::~RawInputHandler() {
    Shutdown();
}

bool RawInputHandler::Initialize(HWND hwnd, InputManager* manager) {
    if (initialized_) {
        LOG_WARN("RawInputHandler already initialized");
        return true;
    }
    
    targetWindow_ = hwnd;
    manager_ = manager;
    
    if (!manager_->Initialize(hwnd)) {
        LOG_ERROR("Failed to initialize InputManager");
        return false;
    }
    
    initialized_ = true;
    LOG_INFO("RawInputHandler initialized");
    return true;
}

void RawInputHandler::Shutdown() {
    if (!initialized_) return;
    
    if (manager_) {
        manager_->Shutdown();
        manager_ = nullptr;
    }
    
    initialized_ = false;
    LOG_INFO("RawInputHandler shutdown");
}

bool RawInputHandler::HandleInputMessage(HRAWINPUT handle) {
    if (!manager_) return false;
    
    manager_->ProcessRawInput(handle);
    return true;
}

} // namespace dualdesk