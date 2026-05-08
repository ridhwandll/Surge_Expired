// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Platform/Windows/WindowsWindow.hpp"
#include "Surge/Core/Core.hpp"
#include <imgui.h>
#include <dwmapi.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Surge
{
    WindowsWindow::WindowsWindow(const WindowDesc& windowData)
    {
        mWindowData = windowData;
        HINSTANCE hInstance = GetModuleHandle(nullptr);

        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);

        const ClientOptions& options = Surge::Core::GetClient()->GetClientOptions();
        if (options.WindowDescription.Flags & WindowFlags::EditorAcceleration)
            wc.lpfnWndProc = WindowProcWithImgui;
        else
            wc.lpfnWndProc = WindowProcWithoutImGui;

        wc.style = CS_CLASSDC;
        wc.hInstance = hInstance;
        wc.lpszClassName = "Surge Win32Window";
        wc.hbrBackground = (HBRUSH)GetStockObject(DKGRAY_BRUSH);
        wc.hCursor = NULL;
        wc.hIcon = nullptr;
        wc.hIconSm = wc.hIcon;
        wc.cbClsExtra = 0;

        RegisterClassEx(&wc);
        SURGE_GET_WIN32_LAST_ERROR

        glm::ivec2 screenSize = Platform::GetScreenSize();
        mWin32Window = CreateWindow(wc.lpszClassName, mWindowData.Title.c_str(),
                                    WS_OVERLAPPEDWINDOW, (screenSize.x - mWindowData.Width) / 2, (screenSize.y - mWindowData.Height) / 2, mWindowData.Width,
                                    mWindowData.Height, nullptr, NULL, wc.hInstance, this);
        SURGE_GET_WIN32_LAST_ERROR
        ApplyFlags();
        SG_ASSERT(mWin32Window, "WindowsWindow creation failure!");

        // Fix missing drop shadow and Windows 11 Rounded corners
        MARGINS ShadowMargins;
        ShadowMargins = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(mWin32Window, &ShadowMargins);
        SURGE_GET_WIN32_LAST_ERROR
        PostQuitMessage(0);
    }

    WindowsWindow::~WindowsWindow()
    {
        DestroyWindow(mWin32Window);
    }

    void WindowsWindow::Update()
    {
        SURGE_PROFILE_FUNC("WindowsWindow::Update()");
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
            mWindowState = WindowState::Minimized;
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

        if (!((flags & WindowFlags::Maximized) && (flags & WindowFlags::Minimized)))
        {
            if (flags & WindowFlags::Maximized)
                ShowWindow(mWin32Window, SW_SHOWMAXIMIZED);
            if (flags & WindowFlags::Minimized)
                ShowWindow(mWin32Window, SW_SHOWMINIMIZED);
            if (flags & WindowFlags::CreateDefault)
                ShowWindow(mWin32Window, SW_SHOWDEFAULT);
        }
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
            case WM_QUIT:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                AppClosedEvent event;
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
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                int repeatCount = (lParam & 0xffff);
                KeyPressedEvent event(static_cast<KeyCode>(wParam), repeatCount);
                data->mEventCallback(event);
                break;
            }
            case WM_MOUSEMOVE:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseMovedEvent event((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam));
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
                MouseButtonPressedEvent event(static_cast<MouseCode>(VK_LBUTTON));
                data->mEventCallback(event);
                break;
            }
            case WM_LBUTTONUP:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseButtonReleasedEvent event(static_cast<MouseCode>(VK_LBUTTON));
                data->mEventCallback(event);
                break;
            }
            case WM_MBUTTONDOWN:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseButtonPressedEvent event(static_cast<MouseCode>(VK_MBUTTON));
                data->mEventCallback(event);
                break;
            }
            case WM_MBUTTONUP:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseButtonReleasedEvent event(static_cast<MouseCode>(VK_MBUTTON));
                data->mEventCallback(event);
                break;
            }
            case WM_RBUTTONDOWN:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseButtonPressedEvent event(static_cast<MouseCode>(VK_RBUTTON));
                data->mEventCallback(event);
                break;
            }
            case WM_RBUTTONUP:
            {
                WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                MouseButtonReleasedEvent event(static_cast<MouseCode>(VK_RBUTTON));
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
                        data->mWindowState = WindowState::Minimized;
                        break;
                    }
                    case SC_RESTORE:
                    {
                        WindowsWindow* data = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                        data->mWindowState = WindowState::Normal;
                        break;
                    }
                }
                return DefWindowProc(hWnd, msg, wParam, lParam);
                break;
            }
            default:
                return DefWindowProc(hWnd, msg, wParam, lParam);
        }

        return 0;
    }

    LRESULT WindowsWindow::WindowProcWithImgui(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // Custom Titlebar and ImGui
        {
            switch (msg)
            {
                case WM_NCHITTEST: // Handle WM_NCHITTEST manually, this signals the default resize
                {
                    POINT mousePos;
                    RECT windowRect;

                    GetCursorPos(&mousePos);
                    GetWindowRect(hWnd, &windowRect);

                    if (PtInRect(&windowRect, mousePos))
                    {
                        const int borderX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                        const int borderY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

                        if (mousePos.y < (windowRect.top + borderY))
                        {
                            if (mousePos.x < (windowRect.left + borderX))
                            {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                                return HTTOPLEFT;
                            }
                            else if (mousePos.x >= (windowRect.right - borderX))
                            {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
                                return HTTOPRIGHT;
                            }
                            else
                            {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                                return HTTOP;
                            }
                        }
                        else if (mousePos.y >= (windowRect.bottom - borderY))
                        {
                            if (mousePos.x < (windowRect.left + borderX))
                            {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
                                return HTBOTTOMLEFT;
                            }
                            else if (mousePos.x >= (windowRect.right - borderX))
                            {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                                return HTBOTTOMRIGHT;
                            }
                            else
                            {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                                return HTBOTTOM;
                            }
                        }
                        else if (mousePos.x < (windowRect.left + borderX))
                        {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                            return HTLEFT;
                        }
                        else if (mousePos.x >= (windowRect.right - borderX))
                        {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                            return HTRIGHT;
                        }
                        else
                        {
                            // Drag the menu bar to move the window
                            if (!ImGui::IsAnyItemHovered() && (mousePos.y < (windowRect.top + 35)))
                                return HTCAPTION;
                        }
                    }
                    break;
                }
                case WM_NCCALCSIZE:
                {
                    // Preserve the old client area and align it with the upper-left corner of the new client area
                    // Starting with Windows Vista, removing the standard frame by simply returning 0 when the wParam is TRUE does not affect frames that area
                    // extended into the client area using the DwmExtendFrameIntoClientArea function.Only the standard frame will be removed
                    return 0;
                    break;
                }
            }
        }

        return WindowProcWithoutImGui(hWnd, msg, wParam, lParam);
    }
} // namespace Surge