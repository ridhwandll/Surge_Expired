// Copyright (c) - SurgeTechnologies - All rights reserved
#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Utility/Platform.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Platform/Android/AndroidApp.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Platform/Android/AndroidWindow.hpp"
#include <android/log.h>
#include <dlfcn.h>
#include <sys/resource.h>
#include <sys/system_properties.h>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>

#define SURGE_ANDROID_LOG_TAG "SurgeEngine"

namespace Surge
{
    // AThermal_* is API 30, minSdk is 29: resolved from libandroid.so at runtime
    struct ThermalApi
    {
        using AcquireFn = void* (*)();
        using ReleaseFn = void (*)(void*);
        using GetStatusFn = int (*)(void*);

        void* Library = nullptr;
        void* Manager = nullptr;
        ReleaseFn Release = nullptr;
        GetStatusFn GetStatus = nullptr;
        bool Resolved = false;
    };
    static ThermalApi sThermal;

    void Platform::Initialize()
    {
        //Empty for now
    }

    void Platform::Shutdown()
    {
        if(sThermal.Manager && sThermal.Release)
            sThermal.Release(sThermal.Manager);
        if(sThermal.Library)
            dlclose(sThermal.Library);
        sThermal = {};
    }

    int Platform::GetThermalStatus()
    {
        if(!sThermal.Resolved)
        {
            sThermal.Resolved = true;
            sThermal.Library = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
            if(sThermal.Library)
            {
                const auto acquire = reinterpret_cast<ThermalApi::AcquireFn>(dlsym(sThermal.Library, "AThermal_acquireManager"));
                sThermal.Release = reinterpret_cast<ThermalApi::ReleaseFn>(dlsym(sThermal.Library, "AThermal_releaseManager"));
                sThermal.GetStatus = reinterpret_cast<ThermalApi::GetStatusFn>(dlsym(sThermal.Library, "AThermal_getCurrentThermalStatus"));
                if(acquire && sThermal.GetStatus)
                    sThermal.Manager = acquire();
            }
        }

        if(!sThermal.Manager)
            return -1;

        const int status = sThermal.GetStatus(sThermal.Manager);
        return status >= 0 && status <= 6 ? status : -1; // ATHERMAL_STATUS_ERROR = -1
    }

    String Platform::GetCPUName()
    {
        const auto property = [](const char* name) {
            char value[PROP_VALUE_MAX] = {};
            return __system_property_get(name, value) > 0 ? String(value) : String();
        };

        // Android 12+ names the SoC ("QTI" + "SM6115"), older versions only have the board platform
        const String model = property("ro.soc.model");
        if(!model.empty())
        {
            const String manufacturer = property("ro.soc.manufacturer");
            return manufacturer.empty() ? model : manufacturer + " " + model;
        }
        return property("ro.board.platform");
    }

    double Platform::GetProcessCPUSeconds()
    {
        rusage usage = {};
        if(getrusage(RUSAGE_SELF, &usage) != 0)
            return -1.0;

        const auto seconds = [](const timeval& time) { return static_cast<double>(time.tv_sec) + static_cast<double>(time.tv_usec) * 1e-6; };
        return seconds(usage.ru_utime) + seconds(usage.ru_stime);
    }

    void Platform::OpenFolderInExplorer(const String& path)
    {
        // What is this supposed to do on Android? We don't have a file explorer to open
    }

    bool Platform::MoveToRecycleBin(const String&)
    {
        return false; // Editor only, APK assets are readonly
    }

    void Platform::OpenInExplorer(const String&)
    {
        // Tf is this supposed to do on Android? We don't have a file explorer to open
    }

    void Platform::OpenInVSCode(const String& workspacePath, const String& path)
    {
        // Tf is this supposed to do on Android? We don't have VSCode to open
    }

    String Platform::GetPersistantStoragePath()
    {
        android_app* app = Android::GAndroidApp;
        if (app && app->activity && app->activity->internalDataPath)
            return String(app->activity->internalDataPath);

        return "/data/data/surge";
    }

    void Platform::RequestExit()
    {
        android_app* app = Android::GAndroidApp;
        if (app && app->activity)
            GameActivity_finish(app->activity);
    }

    void Platform::ErrorMessageBox(const char* text)
    {
        __android_log_print(ANDROID_LOG_ERROR, SURGE_ANDROID_LOG_TAG, "ERROR: %s", text);
    }

    glm::vec2 Platform::GetScreenSize()
    {
        Window* window = Core::GetWindow();
        if (window)
            return window->GetSize();
        return {0.0f, 0.0f};
    }

    bool Platform::SetEnvVariableForCurrentProcess(const String& key, const String& value)
    {
        return setenv(key.c_str(), value.c_str(), 1) == 0;
    }

    String Platform::GetEnvVariableForCurrentProcess(const String& key)
    {
        return "";
    }

    bool Platform::SetEnvVariable(const String& key, const String& value)
    {
        return setenv(key.c_str(), value.c_str(), 1) == 0;
    }

    bool Platform::HasEnvVariable(const String& key)
    {
        return getenv(key.c_str()) != nullptr;
    }

    String Platform::GetEnvVariable(const String& key)
    {
        const char* val = getenv(key.c_str());
        return val ? String(val) : String();
    }

    void* Platform::LoadSharedLibrary(const String& path)
    {
        return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    }

    void* Platform::GetFunction(void* library, const String& procAddress)
    {
        return dlsym(library, procAddress.c_str());
    }

    void Platform::UnloadSharedLibrary(void* library)
    {
        if (library)
            dlclose(library);
    }

    String Platform::GetCurrentExecutablePath()
    {
        android_app* app = Android::GAndroidApp;
        if (app && app->activity && app->activity->externalDataPath)
            return String(app->activity->externalDataPath);
        return String();
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Text input (GameTextInput). The IME edits its own copy of the text: app side changes are pushed with
    // GameActivity_setTextInputState, IME changes are pulled with GameActivity_getTextInputState once the glue flags them.
    // GameTextInput selections are UTF-16 indices (Java strings), the engine works with UTF-8 byte offsets
    // ---------------------------------------------------------------------------------------------------------------

    namespace
    {
        struct ImeState
        {
            bool Visible = false;
            bool SubmitPending = false;
        };
        ImeState sIme;

        // Byte length of the UTF-8 sequence starting with this byte (modified UTF-8 surrogates are 3 byte sequences, 1 UTF-16 unit each)
        int SequenceLength(unsigned char lead)
        {
            if (lead < 0x80) return 1;
            if ((lead >> 5) == 0x6) return 2;
            if ((lead >> 4) == 0xE) return 3;
            if ((lead >> 3) == 0x1E) return 4;
            return 1; // Stray continuation byte
        }

        int Utf16ToByteOffset(const char* text, int length, int utf16Index)
        {
            int bytes = 0, units = 0;
            while (bytes < length && units < utf16Index)
            {
                const int sequence = SequenceLength(static_cast<unsigned char>(text[bytes]));
                units += sequence == 4 ? 2 : 1;
                bytes = std::min(bytes + sequence, length);
            }
            return bytes;
        }

        int ByteToUtf16Offset(const String& text, int byteOffset)
        {
            int bytes = 0, units = 0;
            const int length = static_cast<int>(text.size());
            while (bytes < length && bytes < byteOffset)
            {
                const int sequence = SequenceLength(static_cast<unsigned char>(text[bytes]));
                units += sequence == 4 ? 2 : 1;
                bytes += sequence;
            }
            return units;
        }

        void PushState(GameActivity* activity, const Platform::TextInputState& state)
        {
            GameTextInputState native = {};
            native.text_UTF8 = state.Text.c_str();
            native.text_length = static_cast<int32_t>(state.Text.size());
            native.selection.start = ByteToUtf16Offset(state.Text, state.SelectionStart);
            native.selection.end = ByteToUtf16Offset(state.Text, state.SelectionEnd);
            native.composingRegion.start = -1;
            native.composingRegion.end = -1;
            GameActivity_setTextInputState(activity, &native);
        }
    }

    void Android::OnSoftKeyboardVisibilityChanged(bool visible)
    {
        sIme.Visible = visible;
    }

    void Android::OnEditorAction(int action)
    {
        const uint32_t imeAction = static_cast<uint32_t>(action);
        if (imeAction != IME_ACTION_NONE && imeAction != IME_ACTION_UNSPECIFIED)
            sIme.SubmitPending = true;
    }

    void Platform::ShowSoftKeyboard(SoftKeyboardType type, bool multiLine, const TextInputState& state)
    {
        android_app* app = Android::GAndroidApp;
        if (!app || !app->activity)
            return;

        uint32_t inputType = TYPE_CLASS_TEXT;
        switch (type)
        {
            case SoftKeyboardType::TEXT:     inputType = TYPE_CLASS_TEXT | TYPE_TEXT_FLAG_CAP_SENTENCES; break;
            case SoftKeyboardType::NUMBER:   inputType = TYPE_CLASS_NUMBER | TYPE_NUMBER_FLAG_SIGNED; break;
            case SoftKeyboardType::DECIMAL:  inputType = TYPE_CLASS_NUMBER | TYPE_NUMBER_FLAG_SIGNED | TYPE_NUMBER_FLAG_DECIMAL; break;
            case SoftKeyboardType::EMAIL:    inputType = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_EMAIL_ADDRESS; break;
            case SoftKeyboardType::PHONE:    inputType = TYPE_CLASS_PHONE; break;
            case SoftKeyboardType::PASSWORD: inputType = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_PASSWORD; break;
        }
        if (multiLine && (inputType & TYPE_MASK_CLASS) == TYPE_CLASS_TEXT)
            inputType |= TYPE_TEXT_FLAG_MULTI_LINE;

        // setImeEditorInfo restarts the input connection, so the keyboard layout follows the focused field
        const uint32_t action = multiLine ? IME_ACTION_NONE : IME_ACTION_DONE;
        const uint32_t options = IME_FLAG_NO_FULLSCREEN | IME_FLAG_NO_EXTRACT_UI;
        GameActivity_setImeEditorInfo(app->activity, static_cast<GameTextInputType>(inputType), static_cast<GameTextInputActionType>(action), static_cast<GameTextInputImeOptions>(options));
        PushState(app->activity, state);
        GameActivity_showSoftInput(app->activity, 0);
        sIme.SubmitPending = false;
        app->textInputState = 0; // Our own push is not an IME edit
    }

    void Platform::HideSoftKeyboard()
    {
        android_app* app = Android::GAndroidApp;
        if (app && app->activity)
            GameActivity_hideSoftInput(app->activity, 0);
        sIme.SubmitPending = false;
    }

    void Platform::SetTextInputState(const TextInputState& state)
    {
        android_app* app = Android::GAndroidApp;
        if (app && app->activity)
            PushState(app->activity, state);
    }

    bool Platform::PollTextInputState(TextInputState& outState)
    {
        android_app* app = Android::GAndroidApp;
        if (!app || !app->activity || !app->textInputState)
            return false;

        app->textInputState = 0;
        GameActivity_getTextInputState(app->activity, [](void* context, const GameTextInputState* state) {
            TextInputState& out = *static_cast<TextInputState*>(context);
            if (!state || !state->text_UTF8)
                return;
            out.Text.assign(state->text_UTF8, static_cast<size_t>(state->text_length));
            out.SelectionStart = Utf16ToByteOffset(state->text_UTF8, state->text_length, state->selection.start);
            out.SelectionEnd = Utf16ToByteOffset(state->text_UTF8, state->text_length, state->selection.end);
        }, &outState);
        return true;
    }

    bool Platform::PollTextInputSubmit()
    {
        const bool submit = sIme.SubmitPending;
        sIme.SubmitPending = false;
        return submit;
    }

    bool Platform::IsSoftKeyboardVisible()
    {
        return sIme.Visible;
    }

    float Platform::GetSoftKeyboardHeight()
    {
        android_app* app = Android::GAndroidApp;
        if (!app || !app->activity || !sIme.Visible)
            return 0.0f;

        ARect ime = {};
        GameActivity_getWindowInsets(app->activity, GAMECOMMON_INSETS_TYPE_IME, &ime);
        return static_cast<float>(ime.bottom);
    }

    glm::vec4 Platform::GetSafeAreaInsets()
    {
        android_app* app = Android::GAndroidApp;
        if (!app || !app->activity)
            return { 0.0f, 0.0f, 0.0f, 0.0f };

        // Cached on the Java side, cheap to read. Visible system bars and the cutout both cover content (immersive mode hides the bars)
        ARect bars = {}, cutout = {};
        GameActivity_getWindowInsets(app->activity, GAMECOMMON_INSETS_TYPE_SYSTEM_BARS, &bars);
        GameActivity_getWindowInsets(app->activity, GAMECOMMON_INSETS_TYPE_DISPLAY_CUTOUT, &cutout);
        return { static_cast<float>(std::max(bars.left, cutout.left)), static_cast<float>(std::max(bars.top, cutout.top)),
                 static_cast<float>(std::max(bars.right, cutout.right)), static_cast<float>(std::max(bars.bottom, cutout.bottom)) };
    }

    // The IME handles copy/paste itself (long press, clipboard suggestions), the engine has no Java bridge for the clipboard
    String Platform::GetClipboardText() { return String(); }
    void Platform::SetClipboardText(const String&) {}

} // namespace Surge

#endif // SURGE_PLATFORM_ANDROID