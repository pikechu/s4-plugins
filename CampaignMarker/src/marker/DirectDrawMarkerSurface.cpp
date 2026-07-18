#include "marker/DirectDrawMarkerSurface.h"

#include "resources/CompletionMarkerResources.h"

#include <gdiplus.h>
#include <objidl.h>

#include <cstring>

namespace campaign_completion {
namespace {

HMODULE CurrentModule() noexcept {
    HMODULE module = nullptr;
    const auto address = reinterpret_cast<LPCWSTR>(&CurrentModule);
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            address, &module) == FALSE) {
        return nullptr;
    }
    return module;
}

IStream* OpenCheckImageResource() noexcept {
    const auto module = CurrentModule();
    if (module == nullptr) return nullptr;
    const auto resource = FindResourceW(
        module, MAKEINTRESOURCEW(IDR_COMPLETION_CHECK_PNG), RT_RCDATA);
    if (resource == nullptr) return nullptr;
    const auto size = SizeofResource(module, resource);
    const auto loaded = LoadResource(module, resource);
    const auto* bytes = loaded != nullptr ? LockResource(loaded) : nullptr;
    if (size == 0u || bytes == nullptr) return nullptr;

    const auto memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (memory == nullptr) return nullptr;
    auto* destination = GlobalLock(memory);
    if (destination == nullptr) {
        GlobalFree(memory);
        return nullptr;
    }
    std::memcpy(destination, bytes, size);
    GlobalUnlock(memory);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
        GlobalFree(memory);
        return nullptr;
    }
    return stream;
}

}  // namespace

DirectDrawMarkerSurface::~DirectDrawMarkerSurface() { Shutdown(); }

bool DirectDrawMarkerSurface::Initialize() noexcept {
    try {
        if (checkImage_ != nullptr) return true;
        Gdiplus::GdiplusStartupInput input{};
        if (Gdiplus::GdiplusStartup(&gdiplusToken_, &input, nullptr) !=
            Gdiplus::Ok) {
            gdiplusToken_ = 0u;
            return false;
        }
        imageStream_ = OpenCheckImageResource();
        if (imageStream_ == nullptr) {
            Shutdown();
            return false;
        }
        checkImage_ = Gdiplus::Bitmap::FromStream(imageStream_, FALSE);
        if (checkImage_ == nullptr || checkImage_->GetLastStatus() != Gdiplus::Ok ||
            checkImage_->GetWidth() == 0u || checkImage_->GetHeight() == 0u) {
            Shutdown();
            return false;
        }
        return true;
    } catch (...) {
        Shutdown();
        return false;
    }
}

void DirectDrawMarkerSurface::Shutdown() noexcept {
    try {
        if (dc_ != nullptr) End();
        delete checkImage_;
        checkImage_ = nullptr;
        if (imageStream_ != nullptr) {
            imageStream_->Release();
            imageStream_ = nullptr;
        }
        if (gdiplusToken_ != 0u) {
            Gdiplus::GdiplusShutdown(gdiplusToken_);
            gdiplusToken_ = 0u;
        }
    } catch (...) {}
}

bool DirectDrawMarkerSurface::Describe(LPDIRECTDRAWSURFACE7 surface,
                                       MarkerSurfaceExtent& extent) noexcept {
    try {
        if (surface == nullptr) return false;
        DDSURFACEDESC2 description{};
        description.dwSize = sizeof(description);
        if (FAILED(surface->GetSurfaceDesc(&description))) return false;
        extent = {description.dwWidth, description.dwHeight};
        return extent.width != 0u && extent.height != 0u;
    } catch (...) { return false; }
}

bool DirectDrawMarkerSurface::Begin(LPDIRECTDRAWSURFACE7 surface) noexcept {
    try {
        if (surface == nullptr || dc_ != nullptr || checkImage_ == nullptr)
            return false;
        HDC dc = nullptr;
        if (FAILED(surface->GetDC(&dc)) || dc == nullptr) return false;
        activeSurface_ = surface;
        dc_ = dc;
        return true;
    } catch (...) { return false; }
}

bool DirectDrawMarkerSurface::DrawOutlinedCheck(
    const MarkerCheckGeometry& geometry) noexcept {
    try {
        if (dc_ == nullptr || checkImage_ == nullptr) return false;
        const int saved = SaveDC(dc_);
        if (saved == 0) return false;
        bool success = IntersectClipRect(dc_, geometry.clip.left, geometry.clip.top,
                                         geometry.clip.right, geometry.clip.bottom) != ERROR;
        if (success) {
            const auto left = geometry.points[0].x;
            const auto top = geometry.points[2].y;
            const auto width = geometry.points[2].x - left + 1;
            const auto height = geometry.points[1].y - top + 1;
            if (width <= 0 || height <= 0) {
                success = false;
            } else {
                Gdiplus::Graphics graphics(dc_);
                success = graphics.GetLastStatus() == Gdiplus::Ok &&
                          graphics.SetCompositingMode(
                              Gdiplus::CompositingModeSourceOver) == Gdiplus::Ok &&
                          graphics.SetCompositingQuality(
                              Gdiplus::CompositingQualityHighQuality) == Gdiplus::Ok &&
                          graphics.SetInterpolationMode(
                              Gdiplus::InterpolationModeHighQualityBicubic) == Gdiplus::Ok &&
                          graphics.SetPixelOffsetMode(
                              Gdiplus::PixelOffsetModeHalf) == Gdiplus::Ok &&
                          graphics.DrawImage(
                              checkImage_, Gdiplus::Rect(
                                               left, top, width, height)) ==
                              Gdiplus::Ok;
            }
        }
        return RestoreDC(dc_, saved) != FALSE && success;
    } catch (...) { return false; }
}

bool DirectDrawMarkerSurface::End() noexcept {
    try {
        if (dc_ == nullptr || activeSurface_ == nullptr) return false;
        auto* surface = activeSurface_;
        HDC dc = dc_;
        activeSurface_ = nullptr;
        dc_ = nullptr;
        return SUCCEEDED(surface->ReleaseDC(dc));
    } catch (...) { activeSurface_ = nullptr; dc_ = nullptr; return false; }
}

}  // namespace campaign_completion
