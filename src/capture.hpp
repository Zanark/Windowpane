#pragma once

#include "pixels.hpp"

#include <windows.h>
#include <d3d11.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

namespace windowpane
{
enum class CaptureState
{
    Waiting,
    Frame,
    Minimized,
    Hidden,
    Closed,
    Resized
};

struct CapturedFrame
{
    CaptureState state{CaptureState::Waiting};
    std::vector<Pixel> pixels;
};

class Capture
{
  public:
    Capture() = default;
    ~Capture();
    Capture(const Capture&) = delete;
    Capture& operator=(const Capture&) = delete;
    void Start(HWND source, Rect crop, int framesPerSecond);
    void Stop();
    void SetRate(int framesPerSecond);
    CapturedFrame Poll();
    bool Active() const
    {
        return session_ != nullptr;
    }

  private:
    HWND source_{};
    DWORD processId_{};
    Rect crop_{};
    SIZE windowSize_{};
    winrt::Windows::Graphics::SizeInt32 size_{};
    winrt::com_ptr<ID3D11Device> device_;
    winrt::com_ptr<ID3D11DeviceContext> context_;
    winrt::com_ptr<ID3D11Texture2D> staging_;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice runtimeDevice_{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item_{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool pool_{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session_{nullptr};
};

RECT SourceBounds(HWND source);
} // namespace windowpane
