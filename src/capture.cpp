#include "capture.hpp"

#include <dwmapi.h>
#include <dxgi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.DirectX.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

using namespace winrt;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

namespace windowpane
{
RECT SourceBounds(HWND source)
{
    RECT bounds{};
    const HRESULT result =
        DwmGetWindowAttribute(source, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds));
    if (FAILED(result) && !GetWindowRect(source, &bounds))
    {
        throw std::runtime_error("Cannot determine the selected window bounds.");
    }
    return bounds;
}

Capture::~Capture()
{
    try
    {
        Stop();
    }
    catch (const hresult_error& error)
    {
        OutputDebugStringW((L"WindowPane capture cleanup: " + error.message() + L"\n").c_str());
    }
}

void Capture::Start(HWND source, Rect crop, int framesPerSecond)
{
    Stop();
    if (!GraphicsCaptureSession::IsSupported())
    {
        throw std::runtime_error("Windows Graphics Capture is unavailable on this device.");
    }
    if (!IsWindow(source) || IsIconic(source) || !IsWindowVisible(source))
    {
        throw std::runtime_error("Select an open, visible, non-minimized source window.");
    }

    check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                    D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                                    device_.put(), nullptr, context_.put()));
    const auto dxgiDevice = device_.as<IDXGIDevice>();
    com_ptr<IInspectable> inspectable;
    check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
    runtimeDevice_ = inspectable.as<IDirect3DDevice>();

    const auto interop = get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    check_hresult(interop->CreateForWindow(source, guid_of<GraphicsCaptureItem>(), put_abi(item_)));
    size_ = item_.Size();
    if (!ValidCrop(crop, size_.Width, size_.Height))
    {
        throw std::invalid_argument("Crop must fit inside the source and be 8 to 4096 pixels per side.");
    }
    crop_ = crop;
    source_ = source;
    GetWindowThreadProcessId(source_, &processId_);
    RECT window{};
    if (!GetWindowRect(source_, &window))
    {
        throw std::runtime_error("Cannot read the selected window geometry.");
    }
    windowSize_ = {window.right - window.left, window.bottom - window.top};

    D3D11_TEXTURE2D_DESC description{};
    description.Width = static_cast<UINT>(crop.width);
    description.Height = static_cast<UINT>(crop.height);
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_STAGING;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    check_hresult(device_->CreateTexture2D(&description, nullptr, staging_.put()));
    pool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(
        runtimeDevice_, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size_);
    session_ = pool_.CreateCaptureSession(item_);
    if (winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
            L"Windows.Graphics.Capture.GraphicsCaptureSession", L"IsCursorCaptureEnabled"))
    {
        session_.IsCursorCaptureEnabled(false);
    }
    SetRate(framesPerSecond);
    session_.StartCapture();
}

void Capture::SetRate(int framesPerSecond)
{
    if (std::find(CaptureRates.begin(), CaptureRates.end(), framesPerSecond) == CaptureRates.end())
    {
        throw std::invalid_argument("Capture rate must be 1, 5, 10, 30 or 60 frames per second.");
    }
    if (session_ && winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
                        L"Windows.Graphics.Capture.GraphicsCaptureSession", L"MinUpdateInterval"))
    {
        session_.MinUpdateInterval(winrt::Windows::Foundation::TimeSpan{10'000'000 / framesPerSecond});
    }
}

void Capture::Stop()
{
    if (session_)
    {
        session_.Close();
        session_ = nullptr;
    }
    if (pool_)
    {
        pool_.Close();
        pool_ = nullptr;
    }
    item_ = nullptr;
    runtimeDevice_ = nullptr;
    staging_ = nullptr;
    context_ = nullptr;
    device_ = nullptr;
    source_ = nullptr;
}

CapturedFrame Capture::Poll()
{
    if (!session_)
    {
        return {};
    }
    DWORD processId{};
    GetWindowThreadProcessId(source_, &processId);
    if (!IsWindow(source_) || processId != processId_)
    {
        return {CaptureState::Closed, {}};
    }
    if (IsIconic(source_))
    {
        return {CaptureState::Minimized, {}};
    }
    if (!IsWindowVisible(source_))
    {
        return {CaptureState::Hidden, {}};
    }
    RECT window{};
    if (!GetWindowRect(source_, &window))
    {
        throw std::runtime_error("Cannot read the current source geometry.");
    }
    if (window.right - window.left != windowSize_.cx || window.bottom - window.top != windowSize_.cy)
    {
        return {CaptureState::Resized, {}};
    }
    Direct3D11CaptureFrame latest{nullptr};
    while (auto frame = pool_.TryGetNextFrame())
    {
        if (latest)
        {
            latest.Close();
        }
        latest = std::move(frame);
    }
    if (!latest)
    {
        return {};
    }
    const auto contentSize = latest.ContentSize();
    if (contentSize.Width != size_.Width || contentSize.Height != size_.Height)
    {
        latest.Close();
        // Minimize/restore animations can leave an old, differently sized frame in the pool.
        return {};
    }
    const auto access =
        latest.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    com_ptr<ID3D11Texture2D> texture;
    check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), texture.put_void()));
    D3D11_BOX box{static_cast<UINT>(crop_.x),
                  static_cast<UINT>(crop_.y),
                  0,
                  static_cast<UINT>(crop_.x + crop_.width),
                  static_cast<UINT>(crop_.y + crop_.height),
                  1};
    context_->CopySubresourceRegion(staging_.get(), 0, 0, 0, 0, texture.get(), 0, &box);
    std::vector<Pixel> pixels(static_cast<std::size_t>(crop_.width) * crop_.height);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check_hresult(context_->Map(staging_.get(), 0, D3D11_MAP_READ, 0, &mapped));
    for (int y = 0; y < crop_.height; ++y)
    {
        std::memcpy(pixels.data() + static_cast<std::size_t>(y) * crop_.width,
                    static_cast<const std::uint8_t*>(mapped.pData) +
                        static_cast<std::size_t>(y) * mapped.RowPitch,
                    static_cast<std::size_t>(crop_.width) * sizeof(Pixel));
    }
    context_->Unmap(staging_.get(), 0);
    latest.Close();
    return {CaptureState::Frame, std::move(pixels)};
}
} // namespace windowpane
