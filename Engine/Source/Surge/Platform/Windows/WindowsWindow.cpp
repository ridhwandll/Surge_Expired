// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Platform/Windows/WindowsWindow.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include <imgui.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <shellapi.h>
#include <algorithm>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Surge
{
    // Windows synthesizes mouse messages from touch/pen contacts (for apps that only understand the mouse), those carry this
    // signature in their extra info. The contact itself is reported as Touch*Event (WM_POINTER*), so these are flagged
    static bool IsMouseMessageFromTouch()
    {
        constexpr LPARAM SIGNATURE_MASK = 0xFFFFFF00;
        constexpr LPARAM MI_WP_SIGNATURE = 0xFF515700;
        return (GetMessageExtraInfo() & SIGNATURE_MASK) == MI_WP_SIGNATURE;
    }

    // WM_POINTER* for touch and pen contacts -> Touch*Event. Returns false for the mouse (it keeps using WM_*BUTTON*)
    static bool HandlePointerMessage(const std::function<void(Event&)>& callback, HWND hWnd, UINT msg, WPARAM wParam)
    {
        const UINT32 pointerId = GET_POINTERID_WPARAM(wParam);

        // Capture lost: pointer info may already be gone, cancel the contact (no-op if this id isn't a touch)
        if (msg == WM_POINTERCAPTURECHANGED)
        {
            POINT cursor = {};
            GetCursorPos(&cursor);
            ScreenToClient(hWnd, &cursor);
            if (callback)
            {
                TouchReleasedEvent event(static_cast<int64_t>(pointerId), static_cast<float>(cursor.x), static_cast<float>(cursor.y), true);
                callback(event);
            }
            return true;
        }

        POINTER_INPUT_TYPE type = PT_POINTER;
        if (!GetPointerType(pointerId, &type) || (type != PT_TOUCH && type != PT_PEN))
            return false;

        POINTER_INFO info = {};
        if (!GetPointerInfo(pointerId, &info))
            return false;

        POINT p = info.ptPixelLocation;
        ScreenToClient(hWnd, &p);
        const float x = static_cast<float>(p.x);
        const float y = static_cast<float>(p.y);
        const int64_t id = static_cast<int64_t>(pointerId);

        float pressure = 1.0f;
        if (type == PT_TOUCH)
        {
            POINTER_TOUCH_INFO touchInfo = {};
            if (GetPointerTouchInfo(pointerId, &touchInfo) && (touchInfo.touchMask & TOUCH_MASK_PRESSURE) && touchInfo.pressure > 0)
                pressure = static_cast<float>(touchInfo.pressure) / 1024.0f;
        }

        if (!callback)
            return true;

        switch (msg)
        {
            case WM_POINTERDOWN:
            {
                TouchPressedEvent event(id, x, y, pressure);
                callback(event);
                break;
            }
            case WM_POINTERUPDATE:
            {
                if (!IS_POINTER_INCONTACT_WPARAM(wParam)) // Pen hovering above the screen
                    break;
                TouchMovedEvent event(id, x, y, pressure);
                callback(event);
                break;
            }
            case WM_POINTERUP:
            {
                TouchReleasedEvent event(id, x, y, (info.pointerFlags & POINTER_FLAG_CANCELED) != 0);
                callback(event);
                break;
            }
        }
        return true;
    }

    WindowsWindow::WindowsWindow(const WindowDesc& windowData)
    {
        mWindowData = windowData;
        HINSTANCE hInstance = GetModuleHandle(nullptr); 

        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);

        const ClientOptions& options = Surge::Core::GetClient()->GetClientOptions();

        if(options.WindowDescription.Flags & WindowFlags::NO_TITLEBAR)
            wc.lpfnWndProc = WindowProcBorderless;
        else
            wc.lpfnWndProc = WindowProcWithoutImGui;

        wc.style = CS_CLASSDC;
        wc.hInstance = hInstance;
        wc.lpszClassName = "Surge Win32Window";
        wc.hbrBackground = (HBRUSH)GetStockObject(DKGRAY_BRUSH);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

        // Icon resource ID 1 of the executable
        wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
        wc.hIconSm = static_cast<HICON>(LoadImage(hInstance, MAKEINTRESOURCE(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        if(!wc.hIconSm)
            wc.hIconSm = wc.hIcon;
        wc.cbClsExtra = 0;

        RegisterClassEx(&wc);
        SURGE_GET_WIN32_LAST_ERROR;

        String windowTitle = GetWindowTitle();
        glm::ivec2 screenSize = Platform::GetScreenSize();
        mWin32Window = CreateWindow(wc.lpszClassName, windowTitle.c_str(),
                                    WS_OVERLAPPEDWINDOW | WS_VISIBLE, (screenSize.x - mWindowData.Width) / 2, (screenSize.y - mWindowData.Height) / 2, mWindowData.Width,
                                    mWindowData.Height, nullptr, NULL, wc.hInstance, this);

        // Set the title bar & border color
        #define DWMWA_CAPTION_COLOR_ 35
        COLORREF titlebarColor = RGB(20, 20, 20);
        DwmSetWindowAttribute(mWin32Window, DWMWA_CAPTION_COLOR_, &titlebarColor, sizeof(titlebarColor));
        #define DWMWA_BORDER_COLOR_ 34
        DwmSetWindowAttribute(mWin32Window, DWMWA_BORDER_COLOR_, &titlebarColor, sizeof(titlebarColor));

        SURGE_GET_WIN32_LAST_ERROR
        ApplyFlags();
        SG_ASSERT(mWin32Window, "WindowsWindow creation failure!");

        // Fix missing drop shadow and Windows 11 Rounded corners
        MARGINS ShadowMargins;
        ShadowMargins = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(mWin32Window, &ShadowMargins);
        SURGE_GET_WIN32_LAST_ERROR
    }

    WindowsWindow::~WindowsWindow()
    {
        DestroyWindow(mWin32Window);
    }

    void WindowsWindow::Update()
    {
        SURGE_PROFILE_FUNC("WindowsWindow::Update()");

        constexpr Uint updateTitleEveryNFrames = 60;
        static Uint frameCount = updateTitleEveryNFrames;
        frameCount++;
        if(frameCount >= updateTitleEveryNFrames) // Update every 300 frames
        {
            SetWindowText(mWin32Window, GetWindowTitle().c_str());
            frameCount = 0;
        }

        // Nothing to draw while minimized
        if (IsIconic(mWin32Window))
            WaitMessage();

        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    void WindowsWindow::Minimize()
    {
        Core::AddFrameEndCallback([this]() {
            ShowWindow(mWin32Window, SW_MINIMIZE);
            mWindowState = WindowState::MINIMIZED;
        });
    }

    void WindowsWindow::Maximize()
    {
        Core::AddFrameEndCallback([this]() { ShowWindow(mWin32Window, SW_MAXIMIZE); });
    }

    void WindowsWindow::RestoreFromMaximize()
    {
        Core::AddFrameEndCallback([this]() { ShowWindow(mWin32Window, SW_SHOWNORMAL); });
    }

    bool WindowsWindow::IsWindowMaximized() const
    {
        return IsMaximized(mWin32Window);
    }

    bool WindowsWindow::IsWindowMinimized() const
    {
        return IsIconic(mWin32Window);
    }

    void WindowsWindow::SetTitle(const String& name)
    {
        mWindowData.Title = name;
        SetWindowText(mWin32Window, mWindowData.Title.c_str());
    }

    glm::vec2 WindowsWindow::GetPos() const
    {
        POINT pos = {0, 0};
        ClientToScreen(mWin32Window, &pos);
        return {static_cast<float>(pos.x), static_cast<float>(pos.y)};
    }

    void WindowsWindow::SetPos(const glm::vec2& pos) const
    {
        Surge::Core::AddFrameEndCallback([&, pos] {
            SetWindowPos(mWin32Window, nullptr, static_cast<int>(pos.x), static_cast<int>(pos.y), 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
        });
    }

    glm::vec2 WindowsWindow::GetSize() const
    {
        RECT rect;
        glm::vec2 result;
        if (GetClientRect(mWin32Window, &rect))
        {
            result.x = static_cast<float>(rect.right - rect.left);
            result.y = static_cast<float>(rect.bottom - rect.top);
        }
        return result;
    }

    void WindowsWindow::SetSize(const glm::vec2& size) const
    {
        Surge::Core::AddFrameEndCallback([&, size] {
            RECT rect = {0, 0, (LONG)size.x, (LONG)size.y};
            SetWindowPos(mWin32Window, HWND_TOP, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER);
        });
    }

    void WindowsWindow::ShowConsole(bool show) const
    {
        if (show)
            ShowWindow(GetConsoleWindow(), SW_SHOW);
        else
            ShowWindow(GetConsoleWindow(), SW_HIDE);
    }

    void WindowsWindow::ApplyFlags()
    {
        const WindowFlags& flags = mWindowData.Flags;

        if (!((flags & WindowFlags::MAXIMIZED) && (flags & WindowFlags::MINIMIZED)))
        {
            if (flags & WindowFlags::MAXIMIZED)
                ShowWindow(mWin32Window, SW_SHOWMAXIMIZED);
            if (flags & WindowFlags::MINIMIZED)
                ShowWindow(mWin32Window, SW_SHOWMINIMIZED);
            if (flags & WindowFlags::DEFAULT)
                ShowWindow(mWin32Window, SW_SHOWDEFAULT);
        }
    }

    String WindowsWindow::GetWindowTitle() const
    {
#ifdef SURGE_DEBUG
        String config = "DEBUG";
#elif defined(SURGE_RELEASE)
        String config = "RELEASE";
#endif
        String windowTitle = std::format("{} <{}> {:.2f}ms", mWindowData.Title, config, Core::GetClock().GetMilliseconds());
        return windowTitle;
    }

    void WindowsWindow::SetFileDropEnabled(bool enabled)
    {
        DragAcceptFiles(mWin32Window, enabled ? TRUE : FALSE);
    }

    LRESULT WindowsWindow::WindowProcWithoutImGui(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg)
        {
            case WM_CREATE:
            {
                LPCREATESTRUCT const params = reinterpret_cast<LPCREATESTRUCT>(lParam);
                WindowsWindow* const wnd = reinterpret_cast<WindowsWindow* const>(params->lpCreateParams);

                SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(wnd));
                break;
            }
            case WM_CLOSE:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                WindowClosedEvent event;
                data->mEventCallback(event);
                break;
            }
            case WM_SIZE:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                data->mWindowData.Width = (UINT)LOWORD(lParam);
                data->mWindowData.Height = (UINT)HIWORD(lParam);

                WindowResizeEvent event((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
                if (data->mEventCallback != nullptr)
                    data->mEventCallback(event);
                break;
            }
            case WM_KEYUP:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                KeyReleasedEvent event(static_cast<KeyCode>(wParam));
                data->mEventCallback(event);
                break;
            }
            case WM_CHAR:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                KeyTypedEvent event(static_cast<KeyCode>(wParam));
                data->mEventCallback(event);
                break;
            }
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: // For Alt key
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                WPARAM virtualKeyCode = wParam;
                Uint scancode = (lParam & 0x00ff0000) >> 16;
                bool isExtended = (lParam & 0x01000000) != 0;

                if (virtualKeyCode == VK_SHIFT)
                {
                    // MapVirtualKey translates the scancode to the specific L/R version
                    virtualKeyCode = MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
                }
                else if (virtualKeyCode == VK_CONTROL)
                {
                    virtualKeyCode = isExtended ? VK_RCONTROL : VK_LCONTROL;
                }
                else if (virtualKeyCode == VK_MENU) // Alt key
                {
                    virtualKeyCode = isExtended ? VK_RMENU : VK_LMENU;
                }

                int repeatCount = (lParam & 0xffff);
                KeyPressedEvent event(static_cast<Key>(virtualKeyCode), repeatCount);
                data->mEventCallback(event);
                break;
            }
            case WM_POINTERDOWN:
            case WM_POINTERUPDATE:
            case WM_POINTERUP:
            case WM_POINTERCAPTURECHANGED:
            {
                // Touch/pen contacts become touches, DefWindowProc still synthesizes the mouse messages (ImGui/Editor need them)
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                if (data)
                    HandlePointerMessage(data->mEventCallback, hWnd, msg, wParam);
                return DefWindowProc(hWnd, msg, wParam, lParam);
            }
            case WM_DROPFILES:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                HDROP drop = reinterpret_cast<HDROP>(wParam);
                if(data && data->mEventCallback)
                {
                    const UINT count = DragQueryFileA(drop, 0xFFFFFFFF, nullptr, 0);
                    Vector<String> paths;
                    paths.reserve(count);
                    for(UINT i = 0; i < count; i++)
                    {
                        String path(DragQueryFileA(drop, i, nullptr, 0), '\0');
                        DragQueryFileA(drop, i, path.data(), static_cast<UINT>(path.size() + 1));
                        std::replace(path.begin(), path.end(), '\\', '/');
                        paths.push_back(std::move(path));
                    }

                    POINT point = {};
                    DragQueryPoint(drop, &point);
                    FilesDroppedEvent event(std::move(paths), static_cast<float>(point.x), static_cast<float>(point.y));
                    data->mEventCallback(event);
                }
                DragFinish(drop);
                return 0;
            }
            case WM_KILLFOCUS:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                if (data && data->mEventCallback)
                {
                    WindowLostFocusEvent event;
                    data->mEventCallback(event);
                }
                return DefWindowProc(hWnd, msg, wParam, lParam);
            }
            case WM_MOUSEMOVE:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseMovedEvent event((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), IsMouseMessageFromTouch());
                data->mEventCallback(event);
                break;
            }
            case WM_MOUSEWHEEL:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseScrolledEvent event((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
                data->mEventCallback(event);
                break;
            }
            case WM_LBUTTONDOWN:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseButtonPressedEvent event((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), static_cast<MouseCode>(VK_LBUTTON), IsMouseMessageFromTouch());
                data->mEventCallback(event);
                break;
            }
            case WM_LBUTTONUP:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseButtonReleasedEvent event((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), static_cast<MouseCode>(VK_LBUTTON), IsMouseMessageFromTouch());
                data->mEventCallback(event);
                break;
            }
            case WM_MBUTTONDOWN:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseButtonPressedEvent event((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), static_cast<MouseCode>(VK_MBUTTON), IsMouseMessageFromTouch());
                data->mEventCallback(event);
                break;
            }
            case WM_MBUTTONUP:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseButtonReleasedEvent event((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), static_cast<MouseCode>(VK_MBUTTON), IsMouseMessageFromTouch());
                data->mEventCallback(event);
                break;
            }
            case WM_RBUTTONDOWN:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseButtonPressedEvent event((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), static_cast<MouseCode>(VK_RBUTTON), IsMouseMessageFromTouch());
                data->mEventCallback(event);
                break;
            }
            case WM_RBUTTONUP:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseButtonReleasedEvent event((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), static_cast<MouseCode>(VK_RBUTTON), IsMouseMessageFromTouch());
                data->mEventCallback(event);
                break;
            }
            case WM_SYSCOMMAND:
            {
                switch (wParam)
                {
                    case SC_MINIMIZE:
                    {
                        WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                        data->mWindowState = WindowState::MINIMIZED;
                        break;
                    }
                    case SC_RESTORE:
                    {
                        WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                        data->mWindowState = WindowState::NORMAL;
                        break;
                    }
                }
                return DefWindowProc(hWnd, msg, wParam, lParam);
                break;
            }
            case WM_QUIT:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                WindowClosedEvent event;
                data->mEventCallback(event);
                break;
            }
            case WM_SETCURSOR:
            {
                if(LOWORD(lParam) == HTCLIENT)
                {
                    break;
                }
                return DefWindowProc(hWnd, msg, wParam, lParam);
            }
            default:
                return DefWindowProc(hWnd, msg, wParam, lParam);
        }

        return 0;
    }

    LRESULT WindowsWindow::WindowProcBorderless(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
            case WM_SETCURSOR:
            {
                if(LOWORD(lParam) == HTCLIENT)
                    break;

                return DefWindowProc(hWnd, msg, wParam, lParam);
            }
            case WM_NCHITTEST: // Handle WM_NCHITTEST manually, this signals the default resize
            {
                POINT mousePos;
                RECT windowRect;

                GetCursorPos(&mousePos);
                GetWindowRect(hWnd, &windowRect);

                if(PtInRect(&windowRect, mousePos))
                {
                    bool imGuiEnabled = Surge::Core::GetClient()->GetClientOptions().EnableImGui;

                    const int borderX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                    const int borderY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

                    if(mousePos.y < (windowRect.top + borderY))
                    {
                        if(mousePos.x < (windowRect.left + borderX))
                        {
                            if(imGuiEnabled) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                            return HTTOPLEFT;
                        }
                        else if(mousePos.x >= (windowRect.right - borderX))
                        {
                            if(imGuiEnabled) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
                            return HTTOPRIGHT;
                        }
                        else
                        {
                            if(imGuiEnabled) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                            return HTTOP;
                        }
                    }
                    else if(mousePos.y >= (windowRect.bottom - borderY))
                    {
                        if(mousePos.x < (windowRect.left + borderX))
                        {
                            if(imGuiEnabled) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
                            return HTBOTTOMLEFT;
                        }
                        else if(mousePos.x >= (windowRect.right - borderX))
                        {
                            if(imGuiEnabled) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                            return HTBOTTOMRIGHT;
                        }
                        else
                        {
                            if(imGuiEnabled) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                            return HTBOTTOM;
                        }
                    }
                    else if(mousePos.x < (windowRect.left + borderX))
                    {
                        if(imGuiEnabled) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        return HTLEFT;
                    }
                    else if(mousePos.x >= (windowRect.right - borderX))
                    {
                        if(imGuiEnabled) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        return HTRIGHT;
                    }
                    else
                    {
                        // Drag the menu bar to move the window
                        // Only check ImGui hover states if ImGui is actually enabled! An open popup keeps the clicks: a click outside closes it
                        bool isHoveringImGui = imGuiEnabled ? (ImGui::IsAnyItemHovered() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) : false;

                        // Measured from the client areas top: a maximized windows rect overhangs the monitor
                        const WindowsWindow* window = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                        const float captionHeight = window ? window->mCaptionHeight : 0.0f;
                        POINT clientTop = { 0, 0 };
                        ClientToScreen(hWnd, &clientTop);
                        if(!isHoveringImGui && (mousePos.y < (clientTop.y + static_cast<LONG>(captionHeight))))
                            return HTCAPTION;
                    }
                    break;
                }
            }
            case WM_NCCALCSIZE:
            {
                // (Rid) When handeling custom titlebars(by handeling WM_NCCALCSIZE) the client area overflows beyond the monitor size when maximized(Fuckass Win32).
                // This Preserve the old client area when maximized and align it with the upper-left corner of the new client area
                if(wParam == TRUE && IsZoomed(hWnd)) // IsZoomed means IsMaximized (Fuckass Win32(2))
                {
                    NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                    const int frameX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                    const int frameY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

                    //(Rid) Insets all four edges of the proposed client rectangle by the border thickness,
                    // pulling the edges back inside the monitor's visible workspace (preventing the overflow)
                    params->rgrc[0].left += frameX;
                    params->rgrc[0].top += frameY;
                    params->rgrc[0].right -= frameX;
                    params->rgrc[0].bottom -= frameY;
                }
                return 0;
                break;
            }
        }
        return WindowProcWithoutImGui(hWnd, msg, wParam, lParam);
    }

} // namespace Surge
