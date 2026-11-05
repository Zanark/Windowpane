#include "capture.hpp"

#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <psapi.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace windowpane
{
namespace
{
constexpr wchar_t ManagerClass[] = L"WindowPane.Controls";
constexpr wchar_t PaneClass[] = L"WindowPane.LivePane";
constexpr wchar_t GripClass[] = L"WindowPane.CornerGrip";
constexpr wchar_t SnipClass[] = L"WindowPane.Selection";
constexpr int IconResource = 101;
constexpr UINT TrayMessage = WM_APP + 1;
constexpr UINT ShowControlsMessage = WM_APP + 2;
constexpr int NewPane = 100;
constexpr int HidePane = 101;
constexpr int Reset = 102;
constexpr int PickColor = 103;
constexpr int EnableKey = 104;
constexpr int ClickThrough = 105;
constexpr int Opacity = 106;
constexpr int Tolerance = 107;
constexpr int Softness = 108;
constexpr int Rate = 109;
constexpr int OpenControls = 110;
constexpr int ExitApp = 111;
constexpr COLORREF Background = RGB(17, 24, 37);
constexpr COLORREF Foreground = RGB(230, 238, 249);
constexpr COLORREF Muted = RGB(164, 183, 205);
constexpr COLORREF Accent = RGB(80, 225, 194);

std::wstring ErrorText(const std::exception& error)
{
    const std::string text = error.what();
    return std::wstring(text.begin(), text.end());
}

void CheckWin32(BOOL success, const char* message)
{
    if (!success)
    {
        throw std::runtime_error(std::string(message) + " (Win32 " + std::to_string(GetLastError()) + ").");
    }
}

class Surface
{
  public:
    Surface()
    {
        dc_ = CreateCompatibleDC(nullptr);
        CheckWin32(dc_ != nullptr, "Cannot create composition DC");
    }
    ~Surface()
    {
        if (bitmap_)
        {
            SelectObject(dc_, original_);
            DeleteObject(bitmap_);
        }
        DeleteDC(dc_);
    }
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    void Resize(int width, int height)
    {
        if (width == width_ && height == height_)
        {
            return;
        }
        if (width < 1 || height < 1 || width > 4096 || height > 4096)
        {
            throw std::invalid_argument("Pane dimensions must be 1 to 4096 pixels.");
        }
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* data{};
        HBITMAP replacement = CreateDIBSection(dc_, &info, DIB_RGB_COLORS, &data, nullptr, 0);
        CheckWin32(replacement != nullptr, "Cannot create composition bitmap");
        HGDIOBJ previous = SelectObject(dc_, replacement);
        if (!bitmap_)
        {
            original_ = previous;
        }
        else
        {
            DeleteObject(bitmap_);
        }
        bitmap_ = replacement;
        pixels_ = static_cast<Pixel*>(data);
        width_ = width;
        height_ = height;
    }

    void Present(HWND window)
    {
        RECT bounds{};
        CheckWin32(GetWindowRect(window, &bounds), "Cannot read pane bounds");
        POINT position{bounds.left, bounds.top};
        POINT source{};
        SIZE size{width_, height_};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        CheckWin32(UpdateLayeredWindow(window, nullptr, &position, &size, dc_, &source, 0, &blend, ULW_ALPHA),
                   "Cannot compose the transparent pane");
    }

    void SaveSyntheticBitmap(const std::filesystem::path& path) const
    {
        BITMAPFILEHEADER file{};
        file.bfType = 0x4d42;
        file.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        file.bfSize = file.bfOffBits + static_cast<DWORD>(width_ * height_ * sizeof(Pixel));
        BITMAPINFOHEADER info{};
        info.biSize = sizeof(info);
        info.biWidth = width_;
        info.biHeight = -height_;
        info.biPlanes = 1;
        info.biBitCount = 32;
        info.biCompression = BI_RGB;
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(&file), sizeof(file));
        output.write(reinterpret_cast<const char*>(&info), sizeof(info));
        output.write(reinterpret_cast<const char*>(pixels_),
                     static_cast<std::streamsize>(width_) * height_ * sizeof(Pixel));
        if (!output)
        {
            throw std::runtime_error("Cannot save the synthetic composition evidence.");
        }
    }

    HDC Dc() const
    {
        return dc_;
    }
    Pixel* Pixels() const
    {
        return pixels_;
    }
    int Width() const
    {
        return width_;
    }
    int Height() const
    {
        return height_;
    }

  private:
    HDC dc_{};
    HBITMAP bitmap_{};
    HGDIOBJ original_{};
    Pixel* pixels_{};
    int width_{};
    int height_{};
};

struct Fixture
{
    bool changed{};
    static LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        auto* fixture = reinterpret_cast<Fixture*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            fixture = static_cast<Fixture*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(fixture));
        }
        if (message == WM_PAINT)
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT client{};
            GetClientRect(hwnd, &client);
            HBRUSH blue = CreateSolidBrush(RGB(45, 126, 230));
            FillRect(dc, &client, blue);
            DeleteObject(blue);
            RECT white{24, 24, 120, 76};
            FillRect(dc, &white, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            RECT changing{140, 10, 180, 50};
            HBRUSH color =
                CreateSolidBrush(fixture && fixture->changed ? RGB(225, 30, 45) : RGB(20, 200, 80));
            FillRect(dc, &changing, color);
            DeleteObject(color);
            EndPaint(hwnd, &paint);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
};

class App
{
  public:
    explicit App(HINSTANCE instance) : instance_(instance)
    {
        brush_ = CreateSolidBrush(Background);
        CheckWin32(brush_ != nullptr, "Cannot create control background");
    }
    ~App()
    {
        capture_.reset();
        if (trayReady_)
        {
            Shell_NotifyIconW(NIM_DELETE, &tray_);
        }
        for (int id = 1; id <= 3; ++id)
        {
            UnregisterHotKey(manager_, id);
        }
        if (snipper_)
        {
            DestroyWindow(snipper_);
        }
        if (IsWindow(grip_))
        {
            DestroyWindow(grip_);
        }
        if (pane_)
        {
            DestroyWindow(pane_);
        }
        if (IsWindow(manager_))
        {
            DestroyWindow(manager_);
        }
        if (font_)
        {
            DeleteObject(font_);
        }
        if (heading_)
        {
            DeleteObject(heading_);
        }
        DeleteObject(brush_);
    }

    void Initialize(bool testMode)
    {
        testMode_ = testMode;
        WNDCLASSEXW type{sizeof(type)};
        type.hInstance = instance_;
        type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        type.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IconResource));
        CheckWin32(type.hIcon != nullptr, "Cannot load the WindowPane icon");
        smallIcon_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IconResource), IMAGE_ICON,
                                                   GetSystemMetrics(SM_CXSMICON),
                                                   GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        CheckWin32(smallIcon_ != nullptr, "Cannot load the small WindowPane icon");
        type.hIconSm = smallIcon_;
        type.hbrBackground = brush_;
        type.lpszClassName = ManagerClass;
        type.lpfnWndProc = ManagerProc;
        CheckWin32(RegisterClassExW(&type), "Cannot register controls window");
        type.hbrBackground = nullptr;
        type.lpszClassName = PaneClass;
        type.lpfnWndProc = PaneProc;
        CheckWin32(RegisterClassExW(&type), "Cannot register pane window");
        type.lpszClassName = GripClass;
        type.lpfnWndProc = GripProc;
        type.hCursor = LoadCursorW(nullptr, IDC_SIZEALL);
        CheckWin32(RegisterClassExW(&type), "Cannot register corner grip");
        type.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        type.hCursor = LoadCursorW(nullptr, IDC_CROSS);
        type.lpszClassName = SnipClass;
        type.lpfnWndProc = SnipProc;
        CheckWin32(RegisterClassExW(&type), "Cannot register selection window");

        manager_ = CreateWindowExW(0, ManagerClass, L"WindowPane",
                                   WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
                                   CW_USEDEFAULT, 450, 730, nullptr, nullptr, instance_, this);
        CheckWin32(manager_ != nullptr, "Cannot create controls");
        pane_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, PaneClass,
                                L"WindowPane live view", WS_POPUP | WS_THICKFRAME, 100, 100, 320, 140,
                                nullptr, nullptr, instance_, this);
        CheckWin32(pane_ != nullptr, "Cannot create live pane");
        grip_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, GripClass,
                                L"WindowPane grip - drag to move; right-click for options", WS_POPUP, 100,
                                100, 28, 28, pane_, nullptr, instance_, this);
        CheckWin32(grip_ != nullptr, "Cannot create corner grip");
        CheckWin32(SetTimer(manager_, 1, static_cast<UINT>(1000 / rate_), nullptr) != 0,
                   "Cannot start update timer");
        if (!testMode)
        {
            AddTray();
            const bool hotkeys = RegisterHotKey(manager_, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'N') &&
                                 RegisterHotKey(manager_, 2, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'W') &&
                                 RegisterHotKey(manager_, 3, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'O');
            if (!hotkeys)
            {
                SetStatus(L"A shortcut is already in use. Use the tray or these controls instead.");
            }
            ShowControls();
        }
    }

    int Run()
    {
        MSG message{};
        BOOL result{};
        while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0)
        {
            if (!IsDialogMessageW(manager_, &message))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        CheckWin32(result != -1, "Window message loop failed");
        return static_cast<int>(message.wParam);
    }

    int SelfTest(const std::filesystem::path& report, bool exportDemo = false);

  private:
    static App* Bind(HWND hwnd, UINT message, LPARAM lparam)
    {
        if (message == WM_NCCREATE)
        {
            auto* self = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        return reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    template <typename Callback> static LRESULT Dispatch(App* self, Callback callback)
    {
        try
        {
            return callback();
        }
        catch (const winrt::hresult_error& error)
        {
            self->Fail(L"Windows capture/composition error: " + std::wstring(error.message()) +
                       L" (HRESULT " + std::to_wstring(static_cast<long>(error.code())) + L").");
        }
        catch (const std::exception& error)
        {
            self->Fail(ErrorText(error));
        }
        return 0;
    }

    static LRESULT CALLBACK ManagerProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        App* self = Bind(hwnd, message, lparam);
        if (!self)
        {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }
        if (message == WM_NCCREATE)
        {
            self->manager_ = hwnd;
        }
        return Dispatch(self, [&] { return self->OnManager(hwnd, message, wparam, lparam); });
    }
    static LRESULT CALLBACK PaneProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        App* self = Bind(hwnd, message, lparam);
        if (!self)
        {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }
        return Dispatch(self, [&] { return self->OnPane(hwnd, message, wparam, lparam); });
    }
    static LRESULT CALLBACK SnipProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        App* self = Bind(hwnd, message, lparam);
        if (!self)
        {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }
        return Dispatch(self, [&] { return self->OnSnip(hwnd, message, wparam, lparam); });
    }
    static LRESULT CALLBACK GripProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        App* self = Bind(hwnd, message, lparam);
        if (!self)
        {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }
        return Dispatch(self, [&] { return self->OnGrip(hwnd, message, wparam, lparam); });
    }

    int Dip(int value) const
    {
        return MulDiv(value, static_cast<int>(dpi_), 96);
    }

    HWND Control(const wchar_t* type, const wchar_t* text, DWORD style, int id, int x, int y, int width,
                 int height)
    {
        HWND child = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, Dip(x), Dip(y), Dip(width),
                                     Dip(height), manager_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                     instance_, nullptr);
        CheckWin32(child != nullptr, "Cannot create a control");
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        if (id == EnableKey || id == ClickThrough)
        {
            winrt::check_hresult(SetWindowTheme(child, L"", L""));
        }
        return child;
    }

    HWND Slider(int id, int value, int y, const wchar_t* name)
    {
        HWND slider = Control(TRACKBAR_CLASSW, name, WS_TABSTOP | TBS_HORZ, id, 18, y, 384, 30);
        SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(slider, TBM_SETPOS, TRUE, value);
        SendMessageW(slider, TBM_SETPAGESIZE, 0, 10);
        return slider;
    }

    void CreateControls()
    {
        dpi_ = GetDpiForWindow(manager_);
        font_ = CreateFontW(-Dip(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                            L"Segoe UI");
        heading_ = CreateFontW(-Dip(27), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                               L"Segoe UI");
        Control(L"STATIC", L"WINDOWPANE  /  LOCAL LIVE VIEW", 0, 200, 22, 18, 380, 20);
        HWND title = Control(L"STATIC", L"A little window into any window.", 0, 201, 22, 44, 380, 70);
        SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(heading_), TRUE);
        Control(L"BUTTON", L"+  Snip a window region", WS_TABSTOP | BS_OWNERDRAW, NewPane, 22, 122, 376, 42);
        statusControl_ =
            Control(L"STATIC", L"Choose a region. Nothing is captured until you select a source.", 0, 202, 22,
                    178, 376, 56);
        opacityLabel_ = Control(L"STATIC", L"Opacity  100%", 0, 203, 22, 244, 376, 22);
        Slider(Opacity, appearance_.opacity, 267, L"Pane opacity");
        Control(L"BUTTON", L"Remove a selected color", WS_TABSTOP | BS_AUTOCHECKBOX, EnableKey, 22, 310, 376,
                24);
        Control(L"BUTTON", L"Pick from live pane", WS_TABSTOP | BS_OWNERDRAW, PickColor, 22, 345, 192, 34);
        keyLabel_ = Control(L"STATIC", L"No color selected", 0, 204, 230, 351, 168, 24);
        toleranceLabel_ = Control(L"STATIC", L"Color tolerance  8", 0, 205, 22, 395, 376, 22);
        Slider(Tolerance, appearance_.tolerance, 418, L"Color tolerance");
        softnessLabel_ = Control(L"STATIC", L"Soft edge  5", 0, 206, 22, 458, 376, 22);
        Slider(Softness, appearance_.softness, 481, L"Color edge softness");
        Control(L"STATIC", L"Update rate", 0, 207, 22, 526, 128, 24);
        HWND rate =
            Control(L"COMBOBOX", L"Update rate", WS_TABSTOP | CBS_DROPDOWNLIST, Rate, 230, 520, 168, 120);
        for (int framesPerSecond : CaptureRates)
        {
            const std::wstring label = std::to_wstring(framesPerSecond) + L" fps" +
                                       (framesPerSecond == CaptureRates.back() ? L"  /  maximum" : L"");
            const LRESULT index =
                SendMessageW(rate, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            CheckWin32(index >= 0, "Cannot add update rate");
            if (framesPerSecond == rate_)
            {
                SendMessageW(rate, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
            }
        }
        Control(L"BUTTON", L"Pass mouse clicks through the pane", WS_TABSTOP | BS_AUTOCHECKBOX, ClickThrough,
                22, 566, 376, 26);
        Control(L"BUTTON", L"Hide / show", WS_TABSTOP | BS_OWNERDRAW, HidePane, 22, 610, 176, 36);
        Control(L"BUTTON", L"Reset appearance", WS_TABSTOP | BS_OWNERDRAW, Reset, 214, 610, 184, 36);
        Control(L"STATIC", L"Ctrl+Alt+N  Snip     W  Hide/show     O  Controls", 0, 208, 22, 663, 376, 26);
        CheckDlgButton(manager_, ClickThrough, clickThrough_ ? BST_CHECKED : BST_UNCHECKED);
        UpdateAppearanceLabels();
        RECT size{0, 0, Dip(420), Dip(705)};
        CheckWin32(AdjustWindowRectExForDpi(&size, static_cast<DWORD>(GetWindowLongPtrW(manager_, GWL_STYLE)),
                                            FALSE, 0, dpi_),
                   "Cannot size controls for this monitor");
        SetWindowPos(manager_, nullptr, 0, 0, size.right - size.left, size.bottom - size.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        const BOOL dark = TRUE;
        DwmSetWindowAttribute(manager_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    }

    void AddTray()
    {
        tray_.cbSize = sizeof(tray_);
        tray_.hWnd = manager_;
        tray_.uID = 1;
        tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        tray_.uCallbackMessage = TrayMessage;
        tray_.hIcon = smallIcon_;
        wcscpy_s(tray_.szTip, L"WindowPane - double-click for controls");
        trayReady_ = Shell_NotifyIconW(NIM_ADD, &tray_) != FALSE;
        if (!trayReady_)
        {
            SetStatus(L"Tray icon unavailable. Keep these controls open for recovery.");
        }
    }

    void SetStatus(const std::wstring& text)
    {
        if (status_ != text)
        {
            status_ = text;
            SetWindowTextW(statusControl_, text.c_str());
        }
    }

    void Fail(const std::wstring& text)
    {
        OutputDebugStringW((L"WindowPane: " + text + L"\n").c_str());
        if (snipper_)
        {
            CloseSnip();
        }
        capture_.reset();
        lastFrame_.clear();
        picking_ = false;
        SetStatus(text);
        if (pane_)
        {
            ShowWindow(pane_, SW_HIDE);
        }
        userHidden_ = true;
        if (manager_)
        {
            ShowWindow(manager_, SW_SHOWNORMAL);
        }
        error_ = text;
    }

    void ShowControls()
    {
        if (snipper_)
        {
            CancelSnip();
            return;
        }
        ShowWindow(manager_, SW_SHOWNORMAL);
        SetForegroundWindow(manager_);
    }

    void UpdateAppearanceLabels()
    {
        SetWindowTextW(opacityLabel_, (L"Opacity  " + std::to_wstring(appearance_.opacity) + L"%").c_str());
        SetWindowTextW(toleranceLabel_,
                       (L"Color tolerance  " + std::to_wstring(appearance_.tolerance)).c_str());
        SetWindowTextW(softnessLabel_, (L"Soft edge  " + std::to_wstring(appearance_.softness)).c_str());
        CheckDlgButton(manager_, EnableKey, appearance_.keyEnabled ? BST_CHECKED : BST_UNCHECKED);
        std::wostringstream color;
        color << L"#" << std::uppercase << std::hex << std::setfill(L'0') << std::setw(2)
              << static_cast<int>(appearance_.key.r) << std::setw(2) << static_cast<int>(appearance_.key.g)
              << std::setw(2) << static_cast<int>(appearance_.key.b);
        SetWindowTextW(keyLabel_, hasKeyColor_ ? color.str().c_str() : L"No color selected");
    }

    bool NeedsResnip() const
    {
        return lastFrame_.empty() &&
               (sourcePhase_ == CaptureState::Resized || sourcePhase_ == CaptureState::Closed);
    }

    void SyncGrip()
    {
        if (!IsWindow(grip_))
        {
            return;
        }
        if (!IsWindowVisible(pane_) || snipper_ || picking_)
        {
            ShowWindow(grip_, SW_HIDE);
            return;
        }
        RECT pane{};
        CheckWin32(GetWindowRect(pane_, &pane), "Cannot locate the pane for its grip");
        const int dpi = static_cast<int>(GetDpiForWindow(pane_));
        const int inset = MulDiv(6, dpi, 96);
        const int side = std::max(1, std::min({MulDiv(28, dpi, 96), static_cast<int>(pane.right - pane.left),
                                               static_cast<int>(pane.bottom - pane.top)}));
        const int x = std::max(static_cast<int>(pane.left), static_cast<int>(pane.right) - inset - side);
        const int y = std::min(static_cast<int>(pane.bottom) - side, static_cast<int>(pane.top) + inset);
        const RECT button = ResnipButton();
        const RECT localGrip{x - pane.left, y - pane.top, x - pane.left + side, y - pane.top + side};
        RECT overlap{};
        if (IntersectRect(&overlap, &button, &localGrip))
        {
            ShowWindow(grip_, SW_HIDE);
            return;
        }
        const bool redraw = gripSurface_.Width() != side;
        if (redraw)
        {
            gripSurface_.Resize(side, side);
            const double scale = side / 28.0;
            const double radius = 7 * scale;
            for (int row = 0; row < side; ++row)
            {
                for (int column = 0; column < side; ++column)
                {
                    const double qx = std::abs(column + 0.5 - side / 2.0) - (side / 2.0 - radius);
                    const double qy = std::abs(row + 0.5 - side / 2.0) - (side / 2.0 - radius);
                    const double distance = std::hypot(std::max(qx, 0.0), std::max(qy, 0.0)) +
                                            std::min(std::max(qx, qy), 0.0) - radius;
                    const double coverage = std::clamp(0.5 - distance, 0.0, 1.0);
                    const bool border = distance > -1.2 * scale;
                    const double gradient = static_cast<double>(row) / std::max(1, side - 1);
                    double red = border ? 184 : 63 - 25 * gradient;
                    double green = border ? 211 : 83 - 29 * gradient;
                    double blue = border ? 224 : 103 - 32 * gradient;
                    double alpha = border ? 175 : 218;
                    double dot{};
                    for (int dotY : {8, 14, 20})
                    {
                        for (int dotX : {10, 18})
                        {
                            dot = std::max(dot, std::clamp(1.3 * scale + 0.5 -
                                                               std::hypot(column + 0.5 - dotX * scale,
                                                                          row + 0.5 - dotY * scale),
                                                           0.0, 1.0));
                        }
                    }
                    red += (222 - red) * dot;
                    green += (251 - green) * dot;
                    blue += (245 - blue) * dot;
                    alpha = (alpha + (245 - alpha) * dot) * coverage;
                    gripSurface_.Pixels()[row * side + column] = {
                        static_cast<std::uint8_t>(std::lround(blue * alpha / 255)),
                        static_cast<std::uint8_t>(std::lround(green * alpha / 255)),
                        static_cast<std::uint8_t>(std::lround(red * alpha / 255)),
                        static_cast<std::uint8_t>(std::lround(alpha))};
                }
            }
        }
        RECT previous{};
        CheckWin32(GetWindowRect(grip_, &previous), "Cannot read corner-grip bounds");
        const bool moved = previous.left != x || previous.top != y ||
                           previous.right - previous.left != side || previous.bottom - previous.top != side;
        if (moved)
        {
            CheckWin32(
                SetWindowPos(grip_, HWND_TOPMOST, x, y, side, side, SWP_NOACTIVATE | SWP_NOOWNERZORDER),
                "Cannot align corner grip");
        }
        if (redraw || moved || !IsWindowVisible(grip_))
        {
            gripSurface_.Present(grip_);
            ShowWindow(grip_, SW_SHOWNOACTIVATE);
        }
    }

    RECT ResnipButton() const
    {
        if (!pane_ || !NeedsResnip())
        {
            return {};
        }
        RECT client{};
        GetClientRect(pane_, &client);
        const int dpi = static_cast<int>(GetDpiForWindow(pane_));
        const int padding =
            std::min(MulDiv(12, dpi, 96), static_cast<int>(std::min(client.right, client.bottom)) / 8);
        const int width = std::min(MulDiv(132, dpi, 96), static_cast<int>(client.right) - 2 * padding);
        const int height = std::min(MulDiv(34, dpi, 96), static_cast<int>(client.bottom) - 2 * padding);
        return {padding, client.bottom - padding - height, padding + width, client.bottom - padding};
    }

    bool OnResnipButton(POINT point) const
    {
        const RECT button = ResnipButton();
        return PtInRect(&button, point) != FALSE;
    }

    void Render()
    {
        if (!pane_ || !IsWindowVisible(pane_))
        {
            return;
        }
        RECT bounds{};
        GetClientRect(pane_, &bounds);
        const int width = bounds.right;
        const int height = bounds.bottom;
        if (width < 1 || height < 1)
        {
            return;
        }
        ApplyClickThrough();
        surface_.Resize(width, height);
        if (!lastFrame_.empty())
        {
            Appearance appearance = appearance_;
            if (picking_)
            {
                appearance.keyEnabled = false;
                appearance.opacity = 100;
            }
            const auto output = Compose(lastFrame_, crop_.width, crop_.height, width, height, appearance);
            std::memcpy(surface_.Pixels(), output.data(), output.size() * sizeof(Pixel));
        }
        else
        {
            std::fill_n(surface_.Pixels(), static_cast<std::size_t>(width) * height, Pixel{37, 24, 17, 255});
            SetBkMode(surface_.Dc(), TRANSPARENT);
            SetTextColor(surface_.Dc(), Foreground);
            HGDIOBJ oldFont = SelectObject(surface_.Dc(), font_);
            RECT text{12, 8, width - 12, height - 8};
            const RECT button = ResnipButton();
            if (button.right > button.left)
            {
                text.bottom = button.top - 8;
            }
            const std::wstring message = sourceState_.empty() ? L"Waiting for source..." : sourceState_;
            DrawTextW(surface_.Dc(), message.c_str(), -1, &text, DT_WORDBREAK | DT_NOPREFIX);
            if (button.right > button.left)
            {
                HBRUSH fill = CreateSolidBrush(resnipPressed_ ? RGB(55, 185, 158) : Accent);
                CheckWin32(fill != nullptr, "Cannot draw the snip-again button");
                FillRect(surface_.Dc(), &button, fill);
                DeleteObject(fill);
                SetTextColor(surface_.Dc(), Background);
                RECT label = button;
                const wchar_t* caption =
                    button.right - button.left >= MulDiv(100, static_cast<int>(GetDpiForWindow(pane_)), 96)
                        ? L"Snip again"
                        : L"Snip";
                DrawTextW(surface_.Dc(), caption, -1, &label,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                if (GetFocus() == pane_)
                {
                    InflateRect(&label, -3, -3);
                    DrawFocusRect(surface_.Dc(), &label);
                }
            }
            SelectObject(surface_.Dc(), oldFont);
            for (int i = 0; i < width * height; ++i)
            {
                surface_.Pixels()[i].a = 255;
            }
        }
        surface_.Present(pane_);
        SyncGrip();
    }

    void StartCurrent()
    {
        if (!source_)
        {
            return;
        }
        auto replacement = std::make_unique<Capture>();
        replacement->Start(source_, crop_, rate_);
        capture_ = std::move(replacement);
        lastFrame_.clear();
        sourcePhase_ = CaptureState::Waiting;
        sourceState_ = L"Waiting for source...";
        started_ = GetTickCount64();
        error_.clear();
        SetStatus(L"Waiting for the selected window to produce a frame.");
        ShowWindow(pane_, SW_SHOWNOACTIVATE);
        Render();
    }

    void Tick()
    {
        if (!capture_ || snipper_)
        {
            return;
        }
        auto frame = capture_->Poll();
        if (frame.state == CaptureState::Frame)
        {
            lastFrame_ = std::move(frame.pixels);
            sourcePhase_ = CaptureState::Frame;
            sourceState_.clear();
            ++frames_;
            SetStatus(picking_ ? L"Click a background color inside the live pane. Esc cancels."
                               : L"Live mirror. Drag to move; resize at an edge; right-click for controls.");
            Render();
        }
        else if (frame.state != CaptureState::Waiting)
        {
            lastFrame_.clear();
            sourcePhase_ = frame.state;
            if (frame.state == CaptureState::Minimized)
            {
                sourceState_ = L"Source minimized\nRestore it to resume.";
            }
            if (frame.state == CaptureState::Hidden)
            {
                sourceState_ = L"Source hidden\nShow it to resume.";
            }
            if (frame.state == CaptureState::Closed)
            {
                sourceState_ = L"Source closed\nSnip a new window.";
            }
            if (frame.state == CaptureState::Resized)
            {
                sourceState_ = L"Source resized\nSnip the region again.";
            }
            SetStatus(sourceState_);
            Render();
            if (frame.state == CaptureState::Closed || frame.state == CaptureState::Resized)
            {
                capture_.reset();
            }
        }
        else if (lastFrame_.empty() && GetTickCount64() - started_ > 5000 &&
                 sourceState_ == L"Waiting for source...")
        {
            sourceState_ = L"No frames received\nCheck source or capture policy.";
            SetStatus(sourceState_);
            Render();
        }
    }

    void TogglePane()
    {
        if (!source_)
        {
            SetStatus(L"Snip a window region first.");
            ShowControls();
            return;
        }
        userHidden_ = !userHidden_;
        picking_ = false;
        ApplyClickThrough();
        if (userHidden_)
        {
            ShowWindow(pane_, SW_HIDE);
            capture_.reset();
            lastFrame_.clear();
            SetStatus(L"Pane hidden. Capture is stopped. Show it to reconnect.");
        }
        else
        {
            StartCurrent();
        }
    }

    void BeginPick()
    {
        if (lastFrame_.empty() || userHidden_)
        {
            SetStatus(L"Show a live pane before picking a color.");
            return;
        }
        picking_ = true;
        ApplyClickThrough();
        SetStatus(L"Click a background color inside the live pane. Esc cancels.");
        ShowWindow(pane_, SW_SHOW);
        SetForegroundWindow(pane_);
        Render();
    }

    void EndPick(int x, int y)
    {
        RECT rect{};
        GetClientRect(pane_, &rect);
        const Rect content = FitContent(crop_.width, crop_.height, rect.right, rect.bottom);
        if (x < content.x || y < content.y || x >= content.x + content.width ||
            y >= content.y + content.height)
        {
            SetStatus(L"Click inside the mirrored content, not the transparent resize margin.");
            return;
        }
        appearance_.key = Sample(lastFrame_, crop_.width, crop_.height, rect.right, rect.bottom, x, y);
        hasKeyColor_ = true;
        appearance_.keyEnabled = true;
        picking_ = false;
        ApplyClickThrough();
        UpdateAppearanceLabels();
        SetStatus(L"Color key applied. Tolerance widens the match; soft edge feathers it.");
        Render();
    }

    void ApplyClickThrough()
    {
        const LONG_PTR style = GetWindowLongPtrW(pane_, GWL_EXSTYLE);
        const LONG_PTR desired = clickThrough_ && !picking_ && !NeedsResnip() ? style | WS_EX_TRANSPARENT
                                                                              : style & ~WS_EX_TRANSPARENT;
        if (style == desired)
        {
            return;
        }
        SetLastError(0);
        const LONG_PTR result = SetWindowLongPtrW(pane_, GWL_EXSTYLE, desired);
        CheckWin32(result != 0 || GetLastError() == 0, "Cannot change pane input mode");
    }

    void ResetAppearance()
    {
        appearance_ = {};
        hasKeyColor_ = false;
        picking_ = false;
        clickThrough_ = false;
        ApplyClickThrough();
        CheckDlgButton(manager_, ClickThrough, BST_UNCHECKED);
        SendDlgItemMessageW(manager_, Opacity, TBM_SETPOS, TRUE, appearance_.opacity);
        SendDlgItemMessageW(manager_, Tolerance, TBM_SETPOS, TRUE, appearance_.tolerance);
        SendDlgItemMessageW(manager_, Softness, TBM_SETPOS, TRUE, appearance_.softness);
        UpdateAppearanceLabels();
        if (source_ && userHidden_)
        {
            userHidden_ = false;
            StartCurrent();
        }
        Render();
    }

    void BeginSnip()
    {
        if (snipper_)
        {
            return;
        }
        hadVisiblePane_ = source_ && !userHidden_;
        capture_.reset();
        picking_ = false;
        ApplyClickThrough();
        ShowWindow(pane_, SW_HIDE);
        ShowWindow(manager_, SW_HIDE);
        selectingSource_ = nullptr;
        dragging_ = false;
        desktop_ = {GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                    GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)};
        snipper_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, SnipClass,
                                   L"WindowPane - select a region", WS_POPUP, desktop_.x, desktop_.y,
                                   desktop_.width, desktop_.height, nullptr, nullptr, instance_, this);
        CheckWin32(snipper_ != nullptr, "Cannot create region selector");
        CheckWin32(SetLayeredWindowAttributes(snipper_, 0, 95, LWA_ALPHA), "Cannot dim selection overlay");
        ShowWindow(snipper_, SW_SHOW);
        SetForegroundWindow(snipper_);
    }

    void CloseSnip()
    {
        HWND old = snipper_;
        snipper_ = nullptr;
        dragging_ = false;
        if (GetCapture() == old)
        {
            ReleaseCapture();
        }
        if (old)
        {
            DestroyWindow(old);
        }
    }

    void CancelSnip()
    {
        CloseSnip();
        RestorePreviousPane();
        SetStatus(L"Selection cancelled. Previous pane preserved.");
        if (!testMode_)
        {
            ShowControls();
        }
    }

    void RestorePreviousPane()
    {
        if (hadVisiblePane_)
        {
            if (NeedsResnip())
            {
                ShowWindow(pane_, SW_SHOWNOACTIVATE);
                Render();
            }
            else
            {
                StartCurrent();
            }
        }
    }

    void FinishSnip()
    {
        const HWND selected = selectingSource_;
        const POINT start = selectionStart_;
        const POINT end = selectionEnd_;
        CloseSnip();
        if (!selected)
        {
            throw std::runtime_error("Start your selection inside an application window.");
        }
        if (testMode_ && selected != allowedTestSource_)
        {
            throw std::runtime_error(
                "Test selection was obscured. Refusing to capture anything except its fixture.");
        }
        const RECT bounds = SourceBounds(selected);
        const Rect region =
            SelectRect(start.x, start.y, end.x, end.y,
                       {bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top});
        if (region.width < 8 || region.height < 8 || region.width > 4096 || region.height > 4096)
        {
            RestorePreviousPane();
            SetStatus(L"Select a region between 8 and 4096 pixels per side. Previous pane preserved.");
            if (!testMode_)
            {
                ShowControls();
            }
            return;
        }
        const Rect crop{region.x - bounds.left, region.y - bounds.top, region.width, region.height};
        auto replacement = std::make_unique<Capture>();
        replacement->Start(selected, crop, rate_);
        source_ = selected;
        crop_ = crop;
        capture_ = std::move(replacement);
        lastFrame_.clear();
        sourcePhase_ = CaptureState::Waiting;
        userHidden_ = false;
        sourceState_ = L"Waiting for source...";
        started_ = GetTickCount64();
        appearance_ = {};
        ResetAppearance();
        const double scale = std::min({1.0, 640.0 / crop.width, 480.0 / crop.height});
        const int width = std::max(64, static_cast<int>(crop.width * scale));
        const int height = std::max(48, static_cast<int>(crop.height * scale));
        MONITORINFO monitor{sizeof(monitor)};
        CheckWin32(GetMonitorInfoW(MonitorFromPoint(end, MONITOR_DEFAULTTONEAREST), &monitor),
                   "Cannot locate selection monitor");
        const int x = std::clamp<LONG>(region.x + 20, monitor.rcWork.left,
                                       std::max(monitor.rcWork.left, monitor.rcWork.right - width));
        const int y = std::clamp<LONG>(region.y + 20, monitor.rcWork.top,
                                       std::max(monitor.rcWork.top, monitor.rcWork.bottom - height));
        SetWindowPos(pane_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        Render();
        SetStatus(L"Live pane created. Right-click it or use the tray for controls.");
    }

    void Menu(HWND owner, bool tray)
    {
        HMENU menu = CreatePopupMenu();
        CheckWin32(menu != nullptr, "Cannot create pane menu");
        AppendMenuW(menu, MF_STRING, OpenControls, L"WindowPane controls");
        AppendMenuW(menu, MF_STRING, NewPane, L"Snip a new region\tCtrl+Alt+N");
        AppendMenuW(menu, MF_STRING, HidePane, L"Hide / show pane\tCtrl+Alt+W");
        AppendMenuW(menu, MF_STRING, PickColor, L"Pick transparent color");
        AppendMenuW(menu, MF_STRING, Reset, L"Reset appearance");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ExitApp, L"Exit WindowPane");
        POINT cursor{};
        GetCursorPos(&cursor);
        SetForegroundWindow(owner);
        const int command =
            TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, owner, nullptr);
        DestroyMenu(menu);
        if (tray)
        {
            PostMessageW(owner, WM_NULL, 0, 0);
        }
        if (command)
        {
            Command(command, 0);
        }
    }

    void Command(int id, int notification)
    {
        switch (id)
        {
        case NewPane:
            BeginSnip();
            break;
        case HidePane:
            TogglePane();
            break;
        case PickColor:
            BeginPick();
            break;
        case Reset:
            ResetAppearance();
            break;
        case OpenControls:
            ShowControls();
            break;
        case EnableKey:
            appearance_.keyEnabled = hasKeyColor_ && IsDlgButtonChecked(manager_, EnableKey) == BST_CHECKED;
            if (!hasKeyColor_)
            {
                CheckDlgButton(manager_, EnableKey, BST_UNCHECKED);
                BeginPick();
            }
            Render();
            break;
        case ClickThrough:
            clickThrough_ = IsDlgButtonChecked(manager_, ClickThrough) == BST_CHECKED;
            ApplyClickThrough();
            break;
        case Rate:
            if (notification == CBN_SELCHANGE)
            {
                const LRESULT index = SendDlgItemMessageW(manager_, Rate, CB_GETCURSEL, 0, 0);
                CheckWin32(index >= 0 && index < static_cast<LRESULT>(CaptureRates.size()),
                           "Cannot read selected update rate");
                rate_ = CaptureRates[static_cast<std::size_t>(index)];
                if (capture_)
                {
                    capture_->SetRate(rate_);
                }
                CheckWin32(SetTimer(manager_, 1, static_cast<UINT>(1000 / rate_), nullptr) != 0,
                           "Cannot set update timer");
            }
            break;
        case ExitApp:
            DestroyWindow(manager_);
            break;
        }
    }

    LRESULT OnManager(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_CREATE)
        {
            CreateControls();
            return 0;
        }
        if (message == WM_COMMAND)
        {
            Command(LOWORD(wparam), HIWORD(wparam));
            return 0;
        }
        if (message == WM_TIMER)
        {
            Tick();
            return 0;
        }
        if (message == ShowControlsMessage)
        {
            ShowControls();
            return 0;
        }
        if (message == WM_HOTKEY)
        {
            if (wparam == 1)
            {
                BeginSnip();
            }
            if (wparam == 2)
            {
                TogglePane();
            }
            if (wparam == 3)
            {
                ShowControls();
            }
            return 0;
        }
        if (message == WM_HSCROLL)
        {
            appearance_.opacity = static_cast<int>(SendDlgItemMessageW(hwnd, Opacity, TBM_GETPOS, 0, 0));
            appearance_.tolerance = static_cast<int>(SendDlgItemMessageW(hwnd, Tolerance, TBM_GETPOS, 0, 0));
            appearance_.softness = static_cast<int>(SendDlgItemMessageW(hwnd, Softness, TBM_GETPOS, 0, 0));
            UpdateAppearanceLabels();
            Render();
            return 0;
        }
        if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLORBTN)
        {
            HDC dc = reinterpret_cast<HDC>(wparam);
            SetTextColor(dc, reinterpret_cast<HWND>(lparam) == statusControl_ ? Muted : Foreground);
            SetBkColor(dc, Background);
            return reinterpret_cast<LRESULT>(brush_);
        }
        if (message == WM_DRAWITEM)
        {
            auto& item = *reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
            HBRUSH fill = CreateSolidBrush(item.CtlID == NewPane ? Accent : RGB(39, 54, 75));
            FillRect(item.hDC, &item.rcItem, fill);
            DeleteObject(fill);
            SetBkMode(item.hDC, TRANSPARENT);
            SetTextColor(item.hDC, item.CtlID == NewPane ? Background : Foreground);
            wchar_t label[128]{};
            GetWindowTextW(item.hwndItem, label, 128);
            RECT text = item.rcItem;
            DrawTextW(item.hDC, label, -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (item.itemState & ODS_FOCUS)
            {
                InflateRect(&text, -4, -4);
                DrawFocusRect(item.hDC, &text);
            }
            return TRUE;
        }
        if (message == TrayMessage)
        {
            if (lparam == WM_LBUTTONDBLCLK)
            {
                ShowControls();
            }
            if (lparam == WM_RBUTTONUP)
            {
                Menu(hwnd, true);
            }
            return 0;
        }
        if (message == taskbarCreated_)
        {
            if (!testMode_)
            {
                AddTray();
            }
            return 0;
        }
        if (message == WM_DPICHANGED)
        {
            const RECT suggested = *reinterpret_cast<RECT*>(lparam);
            HWND child = GetWindow(hwnd, GW_CHILD);
            while (child)
            {
                HWND next = GetWindow(child, GW_HWNDNEXT);
                DestroyWindow(child);
                child = next;
            }
            DeleteObject(font_);
            DeleteObject(heading_);
            font_ = nullptr;
            heading_ = nullptr;
            CreateControls();
            SetWindowPos(hwnd, nullptr, suggested.left, suggested.top, suggested.right - suggested.left,
                         suggested.bottom - suggested.top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        if (message == WM_CLOSE)
        {
            if (trayReady_)
            {
                ShowWindow(hwnd, SW_HIDE);
            }
            else
            {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        if (message == WM_DESTROY)
        {
            KillTimer(hwnd, 1);
            capture_.reset();
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    LRESULT OnPane(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_WINDOWPOSCHANGED)
        {
            const LRESULT result = DefWindowProcW(hwnd, message, wparam, lparam);
            SyncGrip();
            return result;
        }
        if (message == WM_NCCALCSIZE)
        {
            return 0;
        }
        if (message == WM_NCHITTEST)
        {
            if (picking_)
            {
                return HTCLIENT;
            }
            RECT bounds{};
            GetWindowRect(hwnd, &bounds);
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            POINT client{x, y};
            ScreenToClient(hwnd, &client);
            if (OnResnipButton(client))
            {
                return HTCLIENT;
            }
            const int edge = MulDiv(8, static_cast<int>(GetDpiForWindow(hwnd)), 96);
            const bool left = x < bounds.left + edge;
            const bool right = x >= bounds.right - edge;
            const bool top = y < bounds.top + edge;
            const bool bottom = y >= bounds.bottom - edge;
            if (top && left)
            {
                return HTTOPLEFT;
            }
            if (top && right)
            {
                return HTTOPRIGHT;
            }
            if (bottom && left)
            {
                return HTBOTTOMLEFT;
            }
            if (bottom && right)
            {
                return HTBOTTOMRIGHT;
            }
            if (left)
            {
                return HTLEFT;
            }
            if (right)
            {
                return HTRIGHT;
            }
            if (top)
            {
                return HTTOP;
            }
            if (bottom)
            {
                return HTBOTTOM;
            }
            return HTCAPTION;
        }
        if (message == WM_GETMINMAXINFO)
        {
            auto& info = *reinterpret_cast<MINMAXINFO*>(lparam);
            info.ptMinTrackSize = {64, 48};
            info.ptMaxTrackSize = {2048, 2048};
            return 0;
        }
        if (message == WM_SIZE)
        {
            Render();
            return 0;
        }
        if (message == WM_EXITSIZEMOVE)
        {
            RECT bounds{};
            GetWindowRect(hwnd, &bounds);
            MONITORINFO monitor{sizeof(monitor)};
            CheckWin32(GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor),
                       "Cannot locate pane monitor");
            const int snap = MulDiv(14, static_cast<int>(GetDpiForWindow(hwnd)), 96);
            int x = bounds.left;
            int y = bounds.top;
            if (std::abs(bounds.left - monitor.rcWork.left) <= snap)
            {
                x = monitor.rcWork.left;
            }
            if (std::abs(bounds.right - monitor.rcWork.right) <= snap)
            {
                x = monitor.rcWork.right - (bounds.right - bounds.left);
            }
            if (std::abs(bounds.top - monitor.rcWork.top) <= snap)
            {
                y = monitor.rcWork.top;
            }
            if (std::abs(bounds.bottom - monitor.rcWork.bottom) <= snap)
            {
                y = monitor.rcWork.bottom - (bounds.bottom - bounds.top);
            }
            SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        if (message == WM_CONTEXTMENU || message == WM_NCRBUTTONUP)
        {
            Menu(hwnd, false);
            return 0;
        }
        if (message == WM_LBUTTONDOWN && OnResnipButton({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}))
        {
            resnipPressed_ = true;
            SetCapture(hwnd);
            SetFocus(hwnd);
            Render();
            return 0;
        }
        if (message == WM_LBUTTONUP && resnipPressed_)
        {
            resnipPressed_ = false;
            const bool activate = OnResnipButton({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }
            if (activate)
            {
                BeginSnip();
            }
            else
            {
                Render();
            }
            return 0;
        }
        if ((message == WM_CAPTURECHANGED || message == WM_CANCELMODE) && resnipPressed_)
        {
            resnipPressed_ = false;
            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }
            Render();
            return 0;
        }
        if (message == WM_KEYDOWN && (wparam == VK_RETURN || wparam == VK_SPACE) && NeedsResnip())
        {
            BeginSnip();
            return 0;
        }
        if (message == WM_SETFOCUS || message == WM_KILLFOCUS)
        {
            Render();
        }
        if (message == WM_LBUTTONUP && picking_)
        {
            EndPick(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
        }
        if (message == WM_SETCURSOR && picking_)
        {
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
            return TRUE;
        }
        if (message == WM_SETCURSOR && LOWORD(lparam) == HTCLIENT)
        {
            POINT cursor{};
            if (GetCursorPos(&cursor) && ScreenToClient(hwnd, &cursor) && OnResnipButton(cursor))
            {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        if (message == WM_KEYDOWN && wparam == VK_ESCAPE && picking_)
        {
            picking_ = false;
            ApplyClickThrough();
            SetStatus(L"Color selection cancelled.");
            Render();
            return 0;
        }
        if (message == WM_DPICHANGED)
        {
            const RECT suggested = *reinterpret_cast<RECT*>(lparam);
            SetWindowPos(hwnd, nullptr, suggested.left, suggested.top, suggested.right - suggested.left,
                         suggested.bottom - suggested.top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        if (message == WM_CLOSE)
        {
            TogglePane();
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    void MoveFromGrip(HWND hwnd, LPARAM position)
    {
        POINT cursor{GET_X_LPARAM(position), GET_Y_LPARAM(position)};
        CheckWin32(ClientToScreen(hwnd, &cursor), "Cannot map corner-grip drag");
        CheckWin32(SetWindowPos(pane_, nullptr, cursor.x - gripOffset_.x, cursor.y - gripOffset_.y, 0, 0,
                                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE),
                   "Cannot move pane from its corner grip");
    }

    void EndGripDrag(HWND hwnd)
    {
        gripDragging_ = false;
        if (GetCapture() == hwnd)
        {
            ReleaseCapture();
        }
    }

    LRESULT OnGrip(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_LBUTTONDOWN)
        {
            POINT cursor{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            RECT pane{};
            CheckWin32(ClientToScreen(hwnd, &cursor) && GetWindowRect(pane_, &pane),
                       "Cannot start corner-grip drag");
            gripOffset_ = {cursor.x - pane.left, cursor.y - pane.top};
            gripDragging_ = true;
            SetFocus(hwnd);
            SetCapture(hwnd);
            CheckWin32(GetCapture() == hwnd, "Cannot capture corner-grip drag");
            return 0;
        }
        if (message == WM_MOUSEMOVE && gripDragging_)
        {
            MoveFromGrip(hwnd, lparam);
            return 0;
        }
        if (message == WM_LBUTTONUP && gripDragging_)
        {
            MoveFromGrip(hwnd, lparam);
            EndGripDrag(hwnd);
            SendMessageW(pane_, WM_EXITSIZEMOVE, 0, 0);
            return 0;
        }
        if ((message == WM_CAPTURECHANGED || message == WM_CANCELMODE ||
             (message == WM_KEYDOWN && wparam == VK_ESCAPE)) &&
            gripDragging_)
        {
            EndGripDrag(hwnd);
            return 0;
        }
        if (message == WM_CONTEXTMENU)
        {
            EndGripDrag(hwnd);
            Menu(hwnd, false);
            return 0;
        }
        if (message == WM_KEYDOWN && (wparam == VK_RETURN || wparam == VK_SPACE))
        {
            ShowControls();
            return 0;
        }
        if (message == WM_COMMAND)
        {
            Command(LOWORD(wparam), HIWORD(wparam));
            return 0;
        }
        if (message == WM_CLOSE)
        {
            EndGripDrag(hwnd);
            TogglePane();
            return 0;
        }
        if (message == WM_DPICHANGED)
        {
            // The owned grip follows the pane's DPI and bounds, not its own suggested rectangle.
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    LRESULT OnSnip(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_KEYDOWN && wparam == VK_ESCAPE)
        {
            CancelSnip();
            return 0;
        }
        if (message == WM_RBUTTONUP)
        {
            CancelSnip();
            return 0;
        }
        if (message == WM_LBUTTONDOWN)
        {
            selectionStart_ = {GET_X_LPARAM(lparam) + desktop_.x, GET_Y_LPARAM(lparam) + desktop_.y};
            selectionEnd_ = selectionStart_;
            ShowWindow(hwnd, SW_HIDE);
            selectingSource_ = GetAncestor(WindowFromPoint(selectionStart_), GA_ROOT);
            ShowWindow(hwnd, SW_SHOW);
            if (selectingSource_ == manager_ || selectingSource_ == pane_ || selectingSource_ == grip_ ||
                selectingSource_ == hwnd || selectingSource_ == GetDesktopWindow() ||
                selectingSource_ == GetShellWindow())
            {
                selectingSource_ = nullptr;
            }
            dragging_ = true;
            SetCapture(hwnd);
            return 0;
        }
        if (message == WM_MOUSEMOVE && dragging_)
        {
            selectionEnd_ = {GET_X_LPARAM(lparam) + desktop_.x, GET_Y_LPARAM(lparam) + desktop_.y};
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (message == WM_LBUTTONUP && dragging_)
        {
            selectionEnd_ = {GET_X_LPARAM(lparam) + desktop_.x, GET_Y_LPARAM(lparam) + desktop_.y};
            FinishSnip();
            return 0;
        }
        if (message == WM_CAPTURECHANGED && dragging_ && reinterpret_cast<HWND>(lparam) != hwnd)
        {
            CancelSnip();
            return 0;
        }
        if (message == WM_PAINT)
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT client{};
            GetClientRect(hwnd, &client);
            FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 255, 255));
            SelectObject(dc, heading_);
            RECT text{30, 30, client.right - 30, 130};
            DrawTextW(dc, L"Drag inside a window to snip a live region.\nEsc or right-click cancels.", -1,
                      &text, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
            if (dragging_)
            {
                const Rect selected =
                    SelectRect(selectionStart_.x - desktop_.x, selectionStart_.y - desktop_.y,
                               selectionEnd_.x - desktop_.x, selectionEnd_.y - desktop_.y,
                               {0, 0, desktop_.width, desktop_.height});
                HPEN pen = CreatePen(PS_SOLID, 4, Accent);
                HGDIOBJ oldPen = SelectObject(dc, pen);
                HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                Rectangle(dc, selected.x, selected.y, selected.x + selected.width,
                          selected.y + selected.height);
                SelectObject(dc, oldBrush);
                SelectObject(dc, oldPen);
                DeleteObject(pen);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    HINSTANCE instance_{};
    HWND manager_{};
    HWND pane_{};
    HWND grip_{};
    HWND snipper_{};
    HWND source_{};
    HWND allowedTestSource_{};
    HWND selectingSource_{};
    HWND statusControl_{};
    HWND opacityLabel_{};
    HWND toleranceLabel_{};
    HWND softnessLabel_{};
    HWND keyLabel_{};
    HBRUSH brush_{};
    HFONT font_{};
    HFONT heading_{};
    HICON smallIcon_{};
    UINT dpi_{96};
    UINT taskbarCreated_{RegisterWindowMessageW(L"TaskbarCreated")};
    NOTIFYICONDATAW tray_{};
    bool trayReady_{};
    bool testMode_{};
    bool userHidden_{true};
    bool hadVisiblePane_{};
    bool picking_{};
    bool clickThrough_{};
    bool hasKeyColor_{};
    bool dragging_{};
    bool resnipPressed_{};
    bool gripDragging_{};
    int rate_{CaptureRates.back()};
    std::wstring status_;
    std::wstring sourceState_;
    CaptureState sourcePhase_{CaptureState::Waiting};
    std::wstring error_;
    POINT selectionStart_{};
    POINT selectionEnd_{};
    POINT gripOffset_{};
    Rect desktop_{};
    Rect crop_{};
    Appearance appearance_;
    std::unique_ptr<Capture> capture_;
    std::vector<Pixel> lastFrame_;
    Surface surface_;
    Surface gripSurface_;
    ULONGLONG started_{};
    std::uint64_t frames_{};
};

void PumpUntil(const std::function<bool()>& condition, int timeoutMilliseconds)
{
    const ULONGLONG deadline = GetTickCount64() + timeoutMilliseconds;
    do
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                throw std::runtime_error("Unexpected application exit.");
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (condition())
        {
            return;
        }
        Sleep(20);
    } while (GetTickCount64() < deadline);
    throw std::runtime_error("Native integration check timed out.");
}

int App::SelfTest(const std::filesystem::path& report, bool exportDemo)
{
    std::ofstream log(report);
    if (!log)
    {
        return 2;
    }
    HWND fixtureWindow{};
    try
    {
        int checks{};
        const auto check = [&](bool passed, const char* description) {
            log << (passed ? "PASS " : "FAIL ") << description << '\n';
            log.flush();
            if (!passed)
            {
                throw std::runtime_error(description);
            }
            ++checks;
        };
        wchar_t executable[MAX_PATH]{};
        CheckWin32(GetModuleFileNameW(nullptr, executable, MAX_PATH) != 0, "Cannot locate icon resources");
        HICON extractedLarge{};
        HICON extractedSmall{};
        const UINT extracted = ExtractIconExW(executable, 0, &extractedLarge, &extractedSmall, 1);
        const bool brandedIcons =
            extracted > 0 && extractedLarge && extractedSmall && smallIcon_ &&
            GetClassLongPtrW(manager_, GCLP_HICONSM) == reinterpret_cast<ULONG_PTR>(smallIcon_);
        if (extractedLarge)
        {
            DestroyIcon(extractedLarge);
        }
        if (extractedSmall)
        {
            DestroyIcon(extractedSmall);
        }
        check(brandedIcons, "Executable and window class expose the embedded large and small branded icons");
        check(rate_ == 60 && SendDlgItemMessageW(manager_, Rate, CB_GETCURSEL, 0, 0) ==
                                 static_cast<LRESULT>(CaptureRates.size() - 1),
              "New sessions default to the maximum selectable 60 fps update rate");
        const auto selectRate = [&](int framesPerSecond) {
            const auto index =
                std::find(CaptureRates.begin(), CaptureRates.end(), framesPerSecond) - CaptureRates.begin();
            SendDlgItemMessageW(manager_, Rate, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
            SendMessageW(manager_, WM_COMMAND, MAKEWPARAM(Rate, CBN_SELCHANGE), 0);
        };
        WNDCLASSW fixtureClass{};
        fixtureClass.hInstance = instance_;
        fixtureClass.lpfnWndProc = Fixture::Proc;
        fixtureClass.lpszClassName = L"WindowPane.SyntheticFixture";
        CheckWin32(RegisterClassW(&fixtureClass), "Cannot register synthetic fixture");
        Fixture fixture;
        MONITORINFO monitor{sizeof(monitor)};
        CheckWin32(GetMonitorInfoW(MonitorFromWindow(manager_, MONITOR_DEFAULTTOPRIMARY), &monitor),
                   "Cannot locate test monitor");
        fixtureWindow = CreateWindowExW(
            WS_EX_TOPMOST, fixtureClass.lpszClassName, L"WindowPane - original synthetic test source",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, monitor.rcWork.left + 40, monitor.rcWork.top + 40, 680, 420,
            nullptr, nullptr, instance_, &fixture);
        CheckWin32(fixtureWindow != nullptr, "Cannot create synthetic fixture");
        allowedTestSource_ = fixtureWindow;
        UpdateWindow(fixtureWindow);
        POINT origin{8, 8};
        ClientToScreen(fixtureWindow, &origin);
        BeginSnip();
        HWND selector = snipper_;
        const auto at = [&](int x, int y) {
            return MAKELPARAM(origin.x + x - desktop_.x, origin.y + y - desktop_.y);
        };
        SendMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, at(0, 0));
        SendMessageW(selector, WM_MOUSEMOVE, MK_LBUTTON, at(180, 110));
        SendMessageW(selector, WM_LBUTTONUP, 0, at(180, 110));
        check(source_ == fixtureWindow && crop_.width == 180 && crop_.height == 110,
              "Snipping mouse path selects the exact synthetic source and crop");
        PumpUntil([&] { return !lastFrame_.empty() || !error_.empty(); }, 8000);
        if (!error_.empty())
        {
            throw std::runtime_error("Capture failed; see debugger/status.");
        }
        check(lastFrame_.size() == 180 * 110 && lastFrame_[0].b == 230 && lastFrame_[0].r == 45 &&
                  lastFrame_[0].a == 255,
              "Actual WGC crop matches blue fixture pixels");
        check(lastFrame_[30 * 180 + 30].r == 255 && lastFrame_[30 * 180 + 30].g == 255,
              "Window/client coordinate mapping retains the white fixture patch");
        for (int framesPerSecond : {30, 60})
        {
            selectRate(framesPerSecond);
            fixture.changed = !fixture.changed;
            InvalidateRect(fixtureWindow, nullptr, FALSE);
            UpdateWindow(fixtureWindow);
            const int expectedRed = fixture.changed ? 225 : 20;
            PumpUntil([&] { return !lastFrame_.empty() && lastFrame_[12 * 180 + 142].r == expectedRed; },
                      6000);
            check(rate_ == framesPerSecond && capture_ && capture_->Active() && error_.empty(),
                  framesPerSecond == 30 ? "Selecting 30 fps during capture delivers updated source pixels"
                                        : "Selecting 60 fps during capture delivers updated source pixels");
        }
        check((GetWindowLongPtrW(pane_, GWL_STYLE) & WS_CAPTION) == 0 &&
                  (GetWindowLongPtrW(pane_, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0,
              "Live pane has no caption and is always on top");
        RECT gripBefore{};
        RECT paneBefore{};
        GetWindowRect(grip_, &gripBefore);
        GetWindowRect(pane_, &paneBefore);
        check(IsWindowVisible(grip_) && GetWindow(grip_, GW_OWNER) == pane_ &&
                  gripBefore.right <= paneBefore.right && gripBefore.top >= paneBefore.top &&
                  (GetWindowLongPtrW(grip_, GWL_EXSTYLE) & WS_EX_TRANSPARENT) == 0,
              "Independent corner grip is visible, interactive and owned by the live pane");
        const int gripCenter = gripSurface_.Width() / 2;
        SendMessageW(grip_, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(gripCenter, gripCenter));
        SendMessageW(grip_, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(gripCenter + 36, gripCenter + 22));
        SendMessageW(grip_, WM_LBUTTONUP, 0, MAKELPARAM(gripCenter, gripCenter));
        RECT paneMoved{};
        RECT gripMoved{};
        GetWindowRect(pane_, &paneMoved);
        GetWindowRect(grip_, &gripMoved);
        check(paneMoved.left == paneBefore.left + 36 && paneMoved.top == paneBefore.top + 22 &&
                  gripMoved.left - paneMoved.left == gripBefore.left - paneBefore.left &&
                  gripMoved.top - paneMoved.top == gripBefore.top - paneBefore.top && !gripDragging_ &&
                  GetCapture() != grip_,
              "Real grip mouse path moves the pane, keeps its corner anchor and releases capture");
        SendMessageW(grip_, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(gripCenter, gripCenter));
        SendMessageW(grip_, WM_KEYDOWN, VK_ESCAPE, 0);
        check(!gripDragging_ && GetCapture() != grip_, "Escape safely cancels a corner-grip drag");
        bool menuSeen{};
        CheckWin32(SetPropW(grip_, L"WindowPane.TestMenuProbe", &menuSeen), "Cannot set owned menu probe");
        CheckWin32(SetTimer(grip_, 2, 150,
                            [](HWND hwnd, UINT, UINT_PTR, DWORD) {
                                GUITHREADINFO state{sizeof(state)};
                                auto* seen = static_cast<bool*>(GetPropW(hwnd, L"WindowPane.TestMenuProbe"));
                                if (seen && GetGUIThreadInfo(GetCurrentThreadId(), &state))
                                {
                                    *seen = (state.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE)) != 0;
                                }
                                EndMenu();
                            }) != 0,
                   "Cannot start owned menu probe");
        SendMessageW(grip_, WM_RBUTTONUP, 0, MAKELPARAM(gripCenter, gripCenter));
        KillTimer(grip_, 2);
        RemovePropW(grip_, L"WindowPane.TestMenuProbe");
        check(menuSeen, "Right-clicking the corner grip opens the actual native options popup");
        surface_.SaveSyntheticBitmap(report.parent_path() / L"synthetic-original-pane.bmp");
        RECT paneBounds{};
        GetWindowRect(pane_, &paneBounds);
        check(SendMessageW(pane_, WM_NCHITTEST, 0, MAKELPARAM(paneBounds.left + 30, paneBounds.top + 30)) ==
                      HTCAPTION &&
                  SendMessageW(pane_, WM_NCHITTEST, 0,
                               MAKELPARAM(paneBounds.right - 2, paneBounds.bottom - 2)) == HTBOTTOMRIGHT,
              "Borderless pane exposes native drag and resize hit targets");

        fixture.changed = true;
        InvalidateRect(fixtureWindow, nullptr, FALSE);
        UpdateWindow(fixtureWindow);
        PumpUntil([&] { return !lastFrame_.empty() && lastFrame_[12 * 180 + 142].r == 225; }, 6000);
        check(true, "Source repaint arrives as new captured pixels");

        HWND opacity = GetDlgItem(manager_, Opacity);
        SendMessageW(opacity, TBM_SETPOS, TRUE, 50);
        SendMessageW(manager_, WM_HSCROLL, TB_THUMBPOSITION, reinterpret_cast<LPARAM>(opacity));
        check(appearance_.opacity == 50 && surface_.Pixels()[30 * surface_.Width() + 30].a == 128,
              "Actual opacity slider event changes premultiplied output alpha");
        SendMessageW(manager_, WM_COMMAND, PickColor, 0);
        check(picking_ && !IsWindowVisible(grip_),
              "Color-picker preview hides the grip to expose source pixels");
        SendMessageW(pane_, WM_LBUTTONUP, 0, MAKELPARAM(4, 4));
        check(!picking_ && appearance_.keyEnabled && appearance_.key.r == 45 && appearance_.key.g == 126 &&
                  appearance_.key.b == 230 && IsWindowVisible(grip_),
              "Picker mouse event samples the captured source color");
        check(surface_.Pixels()[0].a == 0 && surface_.Pixels()[30 * surface_.Width() + 30].a == 128,
              "Picked blue becomes transparent while white remains partially opaque");
        surface_.SaveSyntheticBitmap(report.parent_path() / L"synthetic-keyed-pane.bmp");
        ShowWindow(manager_, SW_SHOWNOACTIVATE);
        const ULONGLONG controlsReady = GetTickCount64() + 350;
        PumpUntil([&] { return GetTickCount64() >= controlsReady; }, 1000);
        CheckWin32(RedrawWindow(manager_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN),
                   "Cannot paint owned controls for documentation");
        RECT controlBounds{};
        GetWindowRect(manager_, &controlBounds);
        Surface controlImage;
        controlImage.Resize(controlBounds.right - controlBounds.left,
                            controlBounds.bottom - controlBounds.top);
        CheckWin32(PrintWindow(manager_, controlImage.Dc(), PW_RENDERFULLCONTENT),
                   "Cannot capture the owned synthetic controls surface");
        for (int i = 0; i < controlImage.Width() * controlImage.Height(); ++i)
        {
            controlImage.Pixels()[i].a = 255;
        }
        controlImage.SaveSyntheticBitmap(report.parent_path() / L"synthetic-controls.bmp");
        ShowWindow(manager_, SW_HIDE);

        POINT backing{300, 150};
        ClientToScreen(fixtureWindow, &backing);
        SetWindowPos(pane_, HWND_TOPMOST, backing.x, backing.y, 180, 110, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        Render();
        DwmFlush();
        Sleep(100);
        for (const POINT point : {POINT{backing.x + 4, backing.y + 4}, POINT{backing.x + 30, backing.y + 30}})
        {
            const HWND visible = GetAncestor(WindowFromPoint(point), GA_ROOT);
            CheckWin32(visible == pane_ || visible == fixtureWindow,
                       "Synthetic composition was obscured; refusing to inspect unrelated screen content");
        }
        HDC screen = GetDC(nullptr);
        CheckWin32(screen != nullptr, "Cannot sample fixture-only composition points");
        const COLORREF background = GetPixel(screen, backing.x + 4, backing.y + 4);
        const COLORREF foreground = GetPixel(screen, backing.x + 30, backing.y + 30);
        ReleaseDC(nullptr, screen);
        check(std::abs(static_cast<int>(GetRValue(background)) - 45) <= 2 &&
                  std::abs(static_cast<int>(GetBValue(background)) - 230) <= 2,
              "Actual layered composition exposes the synthetic backing through keyed pixels");
        check(std::abs(static_cast<int>(GetRValue(foreground)) - 150) <= 3 &&
                  std::abs(static_cast<int>(GetBValue(foreground)) - 243) <= 3,
              "Actual layered composition blends white at 50 percent over the synthetic backing");
        SetWindowPos(pane_, nullptr, 0, 0, 360, 220, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        SendMessageW(manager_, WM_COMMAND, PickColor, 0);
        SendMessageW(pane_, WM_LBUTTONUP, 0, MAKELPARAM(8, 8));
        check(appearance_.key.r == 45 && appearance_.key.b == 230,
              "Color picker maps a resized pane back to original source pixels");
        SendMessageW(manager_, WM_COMMAND, PickColor, 0);
        SendMessageW(pane_, WM_KEYDOWN, VK_ESCAPE, 0);
        check(!picking_ && appearance_.keyEnabled,
              "Escape cancels color picking without changing the saved key");
        SetWindowPos(pane_, HWND_TOPMOST, monitor.rcWork.left + 6, monitor.rcWork.top + 6, 180, 110,
                     SWP_NOACTIVATE);
        SendMessageW(pane_, WM_EXITSIZEMOVE, 0, 0);
        GetWindowRect(pane_, &paneBounds);
        check(paneBounds.left == monitor.rcWork.left && paneBounds.top == monitor.rcWork.top,
              "Native move completion snaps the pane to the monitor work area");

        CheckDlgButton(manager_, ClickThrough, BST_CHECKED);
        SendMessageW(manager_, WM_COMMAND, ClickThrough, 0);
        check((GetWindowLongPtrW(pane_, GWL_EXSTYLE) & WS_EX_TRANSPARENT) != 0,
              "Click-through checkbox changes the native pane style");
        SendMessageW(opacity, TBM_SETPOS, TRUE, 0);
        SendMessageW(manager_, WM_HSCROLL, TB_THUMBPOSITION, reinterpret_cast<LPARAM>(opacity));
        RECT gripBounds{};
        GetWindowRect(grip_, &gripBounds);
        const POINT visibleGrip{(gripBounds.left + gripBounds.right) / 2,
                                (gripBounds.top + gripBounds.bottom) / 2};
        check(surface_.Pixels()[30 * surface_.Width() + 30].a == 0 && IsWindowVisible(grip_) &&
                  WindowFromPoint(visibleGrip) == grip_ &&
                  (GetWindowLongPtrW(pane_, GWL_EXSTYLE) & WS_EX_TRANSPARENT) != 0,
              "Fully transparent click-through content retains a clickable corner grip");
        GetWindowRect(pane_, &paneBefore);
        SendMessageW(grip_, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(gripCenter, gripCenter));
        SendMessageW(grip_, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(gripCenter + 18, gripCenter + 16));
        SendMessageW(grip_, WM_LBUTTONUP, 0, MAKELPARAM(gripCenter, gripCenter));
        GetWindowRect(pane_, &paneMoved);
        check(paneMoved.left == paneBefore.left + 18 && paneMoved.top == paneBefore.top + 16 &&
                  (GetWindowLongPtrW(pane_, GWL_EXSTYLE) & WS_EX_TRANSPARENT) != 0 && !gripDragging_,
              "Grip drag moves a fully transparent click-through pane without disabling click-through");
        SendMessageW(opacity, TBM_SETPOS, TRUE, 50);
        SendMessageW(manager_, WM_HSCROLL, TB_THUMBPOSITION, reinterpret_cast<LPARAM>(opacity));
        SetWindowPos(pane_, nullptr, 0, 0, 360, 220, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        surface_.SaveSyntheticBitmap(report.parent_path() / L"synthetic-grip-pane.bmp");
        gripSurface_.SaveSyntheticBitmap(report.parent_path() / L"synthetic-grip.bmp");
        GetWindowRect(pane_, &paneBounds);
        GetWindowRect(grip_, &gripBounds);
        std::ofstream gripLayout(report.parent_path() / L"synthetic-grip.json");
        gripLayout << "{\"x\":" << gripBounds.left - paneBounds.left
                   << ",\"y\":" << gripBounds.top - paneBounds.top << "}\n";
        if (!gripLayout)
        {
            throw std::runtime_error("Cannot export owned grip geometry.");
        }
        gripLayout.close();
        SendMessageW(manager_, WM_COMMAND, Reset, 0);
        check(appearance_.opacity == 100 && !appearance_.keyEnabled && !clickThrough_,
              "Reset recovers transparent/click-through appearance");
        SendMessageW(grip_, WM_CLOSE, 0, 0);
        check(userHidden_ && !capture_ && !IsWindowVisible(pane_) && IsWindow(grip_) &&
                  !IsWindowVisible(grip_),
              "Closing the grip hides/releases the pane without destroying its recovery control");
        SendMessageW(manager_, WM_COMMAND, HidePane, 0);
        PumpUntil([&] { return !lastFrame_.empty(); }, 6000);
        check(capture_ && IsWindowVisible(pane_) && IsWindowVisible(grip_),
              "Show reconnects to the original source and restores its grip");
        BeginSnip();
        check(!IsWindowVisible(grip_), "Snipping hides the grip instead of leaving an orphan overlay");
        SendMessageW(snipper_, WM_KEYDOWN, VK_ESCAPE, 0);
        check(!snipper_ && source_ == fixtureWindow && capture_,
              "Escape cancels snipping and restores the previous pane");
        PumpUntil([&] { return !lastFrame_.empty(); }, 6000);

        ShowWindow(fixtureWindow, SW_MINIMIZE);
        PumpUntil([&] { return sourceState_.find(L"minimized") != std::wstring::npos; }, 4000);
        check(lastFrame_.empty(), "Minimized source clears old pixels instead of presenting stale data");
        ShowWindow(fixtureWindow, SW_RESTORE);
        PumpUntil([&] { return !lastFrame_.empty(); }, 6000);
        check(true, "Restoring the source resumes live frames");
        ShowWindow(fixtureWindow, SW_HIDE);
        PumpUntil([&] { return sourceState_.find(L"hidden") != std::wstring::npos; }, 4000);
        check(lastFrame_.empty(), "Hidden source clears the old frame");
        ShowWindow(fixtureWindow, SW_SHOWNOACTIVATE);
        PumpUntil([&] { return !lastFrame_.empty(); }, 6000);

        RECT resized{};
        GetWindowRect(fixtureWindow, &resized);
        CheckDlgButton(manager_, ClickThrough, BST_CHECKED);
        SendMessageW(manager_, WM_COMMAND, ClickThrough, 0);
        SetWindowPos(fixtureWindow, nullptr, 0, 0, resized.right - resized.left + 32,
                     resized.bottom - resized.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        PumpUntil([&] { return sourceState_.find(L"resized") != std::wstring::npos; }, 6000);
        check(!capture_ && lastFrame_.empty(), "Source resize stops the invalid geometric crop explicitly");
        SetWindowPos(pane_, nullptr, 0, 0, 560, 240, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        Render();
        const RECT button = ResnipButton();
        const POINT buttonCenter{(button.left + button.right) / 2, (button.top + button.bottom) / 2};
        POINT buttonScreen = buttonCenter;
        ClientToScreen(pane_, &buttonScreen);
        check(button.right > button.left &&
                  SendMessageW(pane_, WM_NCHITTEST, 0, MAKELPARAM(buttonScreen.x, buttonScreen.y)) ==
                      HTCLIENT &&
                  (GetWindowLongPtrW(pane_, GWL_EXSTYLE) & WS_EX_TRANSPARENT) == 0,
              "Resized-source recovery button is a client hit target even with click-through enabled");
        surface_.SaveSyntheticBitmap(report.parent_path() / L"synthetic-recovery-pane.bmp");
        const auto clickResnip = [&] {
            SendMessageW(pane_, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(buttonCenter.x, buttonCenter.y));
            SendMessageW(pane_, WM_LBUTTONUP, 0, MAKELPARAM(buttonCenter.x, buttonCenter.y));
        };
        SendMessageW(pane_, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(buttonCenter.x, buttonCenter.y));
        SendMessageW(pane_, WM_LBUTTONUP, 0, MAKELPARAM(button.right + 4, buttonCenter.y));
        check(!snipper_ && !resnipPressed_ && GetCapture() != pane_,
              "Releasing outside the recovery button cancels activation and releases mouse capture");
        clickResnip();
        check(snipper_ && !IsWindowVisible(pane_), "In-pane Snip again opens the actual selection overlay");
        SendMessageW(snipper_, WM_KEYDOWN, VK_ESCAPE, 0);
        check(!snipper_ && !capture_ && NeedsResnip() && IsWindowVisible(pane_) && error_.empty(),
              "Cancelling re-snipping preserves the resized-source recovery pane without restarting an "
              "invalid crop");
        for (const int key : {VK_RETURN, VK_SPACE})
        {
            SendMessageW(pane_, WM_KEYDOWN, key, 0);
            check(snipper_ != nullptr, "Recovery action supports Enter and Space");
            SendMessageW(snipper_, WM_KEYDOWN, VK_ESCAPE, 0);
        }
        SetWindowPos(pane_, nullptr, 0, 0, 64, 48, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        const RECT compact = ResnipButton();
        check(compact.left >= 0 && compact.top >= 0 && compact.right <= 64 && compact.bottom <= 48 &&
                  compact.right > compact.left && compact.bottom > compact.top && !IsWindowVisible(grip_),
              "Minimum-sized recovery remains usable without the grip covering its action");
        SetWindowPos(pane_, nullptr, 0, 0, 560, 240, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        clickResnip();
        selector = snipper_;
        SendMessageW(selector, WM_LBUTTONDOWN, MK_LBUTTON, at(0, 0));
        SendMessageW(selector, WM_MOUSEMOVE, MK_LBUTTON, at(180, 110));
        SendMessageW(selector, WM_LBUTTONUP, 0, at(180, 110));
        PumpUntil([&] { return !lastFrame_.empty(); }, 6000);
        check(source_ == fixtureWindow && capture_ && !NeedsResnip() && ResnipButton().right == 0,
              "Completing in-pane re-snipping restores live pixels and removes recovery chrome");

        selectRate(5);
        FILETIME creation{}, exit{}, kernelStart{}, userStart{}, kernelEnd{}, userEnd{};
        CheckWin32(GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernelStart, &userStart),
                   "Cannot measure fixture CPU time");
        const ULONGLONG measurementStart = GetTickCount64();
        const std::uint64_t initialFrames = frames_;
        ULONGLONG nextPaint{};
        PumpUntil(
            [&] {
                const ULONGLONG now = GetTickCount64();
                if (now >= nextPaint)
                {
                    fixture.changed = !fixture.changed;
                    InvalidateRect(fixtureWindow, nullptr, FALSE);
                    UpdateWindow(fixtureWindow);
                    nextPaint = now + 200;
                }
                return now - measurementStart >= 5000;
            },
            6000);
        const ULONGLONG measuredMilliseconds = GetTickCount64() - measurementStart;
        CheckWin32(GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernelEnd, &userEnd),
                   "Cannot finish fixture CPU observation");
        const auto ticks = [](FILETIME time) {
            return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
        };
        const double cpuOneCorePercent =
            static_cast<double>(ticks(kernelEnd) - ticks(kernelStart) + ticks(userEnd) - ticks(userStart)) /
            (measuredMilliseconds * 100.0);
        check(frames_ - initialFrames >= 10, "Controlled repaint workload continues producing fresh frames");
        PROCESS_MEMORY_COUNTERS_EX memory{};
        memory.cb = sizeof(memory);
        CheckWin32(GetProcessMemoryInfo(GetCurrentProcess(),
                                        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)),
                   "Cannot measure preview process memory");
        log << "OBSERVATION fixture=180x110 rate=" << rate_ << "fps duration_ms=" << measuredMilliseconds
            << " frames=" << frames_ - initialFrames << " cpu_percent_one_core=" << cpuOneCorePercent
            << " working_set_bytes=" << memory.WorkingSetSize << " private_bytes=" << memory.PrivateUsage
            << '\n';
        selectRate(CaptureRates.back());
        if (exportDemo)
        {
            const auto directory = report.parent_path() / L"demo-frames";
            std::filesystem::create_directories(directory);
            const auto saveFrame = [&](const wchar_t* family, int index) {
                Render();
                std::wostringstream name;
                name << family << L'-' << std::setfill(L'0') << std::setw(3) << index << L".bmp";
                surface_.SaveSyntheticBitmap(directory / name.str());
            };
            const auto slider = [&](int id, int value) {
                HWND control = GetDlgItem(manager_, id);
                SendMessageW(control, TBM_SETPOS, TRUE, value);
                SendMessageW(manager_, WM_HSCROLL, TB_THUMBPOSITION, reinterpret_cast<LPARAM>(control));
            };
            const auto saveControls = [&](const wchar_t* name) {
                ShowWindow(manager_, SW_SHOWNOACTIVATE);
                const ULONGLONG ready = GetTickCount64() + 350;
                PumpUntil([&] { return GetTickCount64() >= ready; }, 1000);
                CheckWin32(RedrawWindow(manager_, nullptr, nullptr,
                                        RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN),
                           "Cannot repaint synthetic demo controls");
                CheckWin32(PrintWindow(manager_, controlImage.Dc(), PW_RENDERFULLCONTENT),
                           "Cannot capture owned demo controls");
                for (int i = 0; i < controlImage.Width() * controlImage.Height(); ++i)
                {
                    controlImage.Pixels()[i].a = 255;
                }
                controlImage.SaveSyntheticBitmap(directory / name);
                ShowWindow(manager_, SW_HIDE);
            };
            SendMessageW(manager_, WM_COMMAND, Reset, 0);
            SetWindowPos(pane_, nullptr, 0, 0, 360, 220, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            saveControls(L"controls-default.bmp");
            for (int i = 0; i < 8; ++i)
            {
                fixture.changed = (i % 2) != 0;
                InvalidateRect(fixtureWindow, nullptr, FALSE);
                UpdateWindow(fixtureWindow);
                const auto expected = fixture.changed ? 225 : 20;
                PumpUntil([&] { return !lastFrame_.empty() && lastFrame_[12 * 180 + 142].r == expected; },
                          6000);
                saveFrame(L"live", i);
            }
            for (int i = 0; i <= 16; ++i)
            {
                slider(Opacity, 100 - 10 * std::min(i, 16 - i));
                saveFrame(L"opacity", i);
            }
            slider(Opacity, 100);
            saveFrame(L"key", 0);
            SendMessageW(manager_, WM_COMMAND, PickColor, 0);
            saveControls(L"controls-picker.bmp");
            SendMessageW(pane_, WM_LBUTTONUP, 0, MAKELPARAM(8, 8));
            saveFrame(L"key", 1);
            slider(Opacity, 50);
            saveFrame(L"key", 2);
            saveControls(L"controls-keyed.bmp");
            slider(Opacity, 100);
            for (int i = 0; i <= 12; ++i)
            {
                const int width = 240 + 30 * std::min(i, 12 - i);
                SetWindowPos(pane_, nullptr, 0, 0, width, 220, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                saveFrame(L"resize", i);
            }
            check(true, "Documentation frames exported only from the guarded fixture and owned controls");
            SendMessageW(manager_, WM_COMMAND, HidePane, 0);
            check(!capture_, "Demo teardown releases capture before destroying its owned source");
            DestroyWindow(fixtureWindow);
            fixtureWindow = nullptr;
            log << "RESULT " << checks << " native demo checks passed\n";
            return 0;
        }
        DestroyWindow(fixtureWindow);
        fixtureWindow = nullptr;
        PumpUntil([&] { return sourceState_.find(L"closed") != std::wstring::npos; }, 4000);
        check(!capture_ && lastFrame_.empty(), "Source closure stops capture without keeping old data");
        SendMessageW(pane_, WM_KEYDOWN, VK_RETURN, 0);
        check(snipper_ != nullptr, "Closed-source recovery offers the same in-pane selection action");
        SendMessageW(snipper_, WM_KEYDOWN, VK_ESCAPE, 0);
        check(!capture_ && NeedsResnip() && IsWindowVisible(pane_) && error_.empty(),
              "Cancelling after source closure keeps recovery visible without capturing a dead window");
        log << "RESULT " << checks << " native integration checks passed\n";
        return 0;
    }
    catch (const winrt::hresult_error& error)
    {
        log << "ERROR HRESULT " << static_cast<long>(error.code()) << '\n';
    }
    catch (const std::exception& error)
    {
        log << "ERROR " << error.what() << '\n';
        log << "STATUS " << winrt::to_string(status_) << '\n';
    }
    if (fixtureWindow)
    {
        DestroyWindow(fixtureWindow);
    }
    return 1;
}
} // namespace
} // namespace windowpane

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
        windowpane::CheckWin32(InitCommonControlsEx(&controls), "Cannot initialize Windows controls");
        int count{};
        LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        windowpane::CheckWin32(arguments != nullptr, "Cannot parse command line");
        const bool exportDemo = count == 3 && std::wstring(arguments[1]) == L"--demo-assets";
        const bool selfTest = count == 3 && (std::wstring(arguments[1]) == L"--self-test" || exportDemo);
        const std::filesystem::path report = selfTest ? arguments[2] : L"";
        LocalFree(arguments);
        HANDLE mutex{};
        if (!selfTest)
        {
            mutex = CreateMutexW(nullptr, FALSE, L"Local\\WindowPane.NativePreview");
            windowpane::CheckWin32(mutex != nullptr, "Cannot create application identity");
            if (GetLastError() == ERROR_ALREADY_EXISTS)
            {
                HWND existing = FindWindowW(windowpane::ManagerClass, nullptr);
                if (existing)
                {
                    PostMessageW(existing, windowpane::ShowControlsMessage, 0, 0);
                }
                CloseHandle(mutex);
                return 0;
            }
        }
        int result{};
        {
            windowpane::App app(instance);
            app.Initialize(selfTest);
            result = selfTest ? app.SelfTest(report, exportDemo) : app.Run();
        }
        if (mutex)
        {
            CloseHandle(mutex);
        }
        winrt::uninit_apartment();
        return result;
    }
    catch (const winrt::hresult_error& error)
    {
        MessageBoxW(nullptr, error.message().c_str(), L"WindowPane could not start", MB_OK | MB_ICONERROR);
    }
    catch (const std::exception& error)
    {
        MessageBoxW(nullptr, windowpane::ErrorText(error).c_str(), L"WindowPane could not start",
                    MB_OK | MB_ICONERROR);
    }
    return 1;
}
