#pragma once

#include "native/NativeEventAdmission.h"

namespace campaign_completion {

class INativeEventRegistration {
public:
    virtual ~INativeEventRegistration() = default;
    virtual bool Register(void* handle) noexcept = 0;
    virtual bool Unregister(void* handle) noexcept = 0;
};

class NativeEventRegistration final : public INativeEventRegistration {
public:
    explicit NativeEventRegistration(NativeEventAdmission admission) noexcept;

    bool Register(void* handle) noexcept override;
    bool Unregister(void* handle) noexcept override;

private:
    bool Invoke(std::uintptr_t operation, void* handle) noexcept;

    NativeEventAdmission admission_{};
};

}  // namespace campaign_completion
