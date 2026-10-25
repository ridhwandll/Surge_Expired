// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include <glm/glm.hpp>

namespace Surge::Platform
{
    void Initialize();
    void Shutdown();

    String GetPersistantStoragePath();
    void RequestExit();
    void ErrorMessageBox(const char* text);

    // Enters the folder in the system's file explorer. If the path is a file, it will fail to open, use OpenInExplorer() instead.
    void OpenFolderInExplorer(const String& path);

    // Opens the file or folder in the system's file explorer. If the path is a folder, it will open it. If it's a file, it will open the containing folder and select the file.
    void OpenInExplorer(const String& path);

    // If VSCode is installed and available in the system's PATH, this will open the specified workspace and file in VSCode. If the workspacePath is empty, it will only open the file.
    void OpenInVSCode(const String& workspacePath, const String& path);

    // Moves a file or folder (with its contents) to the OS recycle bin so it can be restored. False when it could not be moved
    bool MoveToRecycleBin(const String& path);

    glm::vec2 GetScreenSize();

    // Sets/Gets it for the current process only. No registry.
    bool SetEnvVariableForCurrentProcess(const String& key, const String& value);
    String GetEnvVariableForCurrentProcess(const String& key);

    bool SetEnvVariable(const String& key, const String& value);
    bool HasEnvVariable(const String& key);
    String GetEnvVariable(const String& key);

    void* LoadSharedLibrary(const String& path);
    void* GetFunction(void* library, const String& procAddress);
    void UnloadSharedLibrary(void* library);
    String GetCurrentExecutablePath();

    // Text input (UI::InputField). Android: soft keyboard + IME through GameTextInput, the IME owns the text while it is open
    // Windows: no keyboard to show, text arrives as KeyTyped/KeyPressed events, so these are no-ops (except the clipboard)
    enum class SoftKeyboardType : uint8_t { TEXT, NUMBER, DECIMAL, EMAIL, PHONE, PASSWORD };
    struct TextInputState
    {
        String Text;            // UTF-8
        int SelectionStart = 0; // Byte offsets into Text
        int SelectionEnd = 0;
    };

    void ShowSoftKeyboard(SoftKeyboardType type, bool multiLine, const TextInputState& state);
    void HideSoftKeyboard();
    void SetTextInputState(const TextInputState& state); // App side edits (caret moves, hardware keys) pushed to the IME
    bool PollTextInputState(TextInputState& outState);   // True when the IME changed the text since the last poll
    bool PollTextInputSubmit();                          // True once after the IME action key (Done)
    bool IsSoftKeyboardVisible();
    float GetSoftKeyboardHeight();                       // Window pixels covered at the bottom, 0 when hidden

    glm::vec4 GetSafeAreaInsets(); // Window pixels (left, top, right, bottom): display cutout, status and navigation bars

    // Device thermal state, Android's AThermalStatus: 0 none, 1 light, 2 moderate, 3 severe, 4 critical, 5 emergency, 6 shutdown.
    // -1 when unknown (Windows, Android < 11). A binder call on Android: poll it about once a second, never per frame
    int GetThermalStatus();

    // CPU model as the system reports it ("AMD Ryzen 7 5800H with Radeon Graphics"; Android 12+: "QTI SM6115", older: the board platform).
    // "" when unknown. Read it once
    String GetCPUName();

    // CPU time this process has used so far, every thread summed (user + kernel), in seconds. -1 when unknown
    double GetProcessCPUSeconds();

    String GetClipboardText();
    void SetClipboardText(const String& text);

} // namespace Surge::Platform