#pragma once

#include "marker/CompletionMarkerRenderer.h"

struct IStream;

namespace Gdiplus {
class Bitmap;
}

namespace campaign_completion {

class DirectDrawMarkerSurface final : public IMarkerDrawingSurface {
public:
    ~DirectDrawMarkerSurface();
    bool Initialize() noexcept;
    void Shutdown() noexcept;
    bool Describe(LPDIRECTDRAWSURFACE7 surface,
                  MarkerSurfaceExtent& extent) noexcept override;
    bool Begin(LPDIRECTDRAWSURFACE7 surface) noexcept override;
    bool DrawOutlinedCheck(const MarkerCheckGeometry& geometry) noexcept override;
    bool End() noexcept override;

private:
    LPDIRECTDRAWSURFACE7 activeSurface_ = nullptr;
    HDC dc_ = nullptr;
    ULONG_PTR gdiplusToken_ = 0u;
    IStream* imageStream_ = nullptr;
    Gdiplus::Bitmap* checkImage_ = nullptr;
};

}  // namespace campaign_completion
