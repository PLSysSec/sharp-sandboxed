#include "rlbox_mgr.h"
#include <mutex>


rlbox_sandbox_vips* GetVipsSandbox() {
    static rlbox_sandbox_vips sandbox;
    static std::once_flag sandbox_create_flag;

    std::call_once(sandbox_create_flag, [&](){
        sandbox.create_sandbox();
    });
    return &sandbox;
}