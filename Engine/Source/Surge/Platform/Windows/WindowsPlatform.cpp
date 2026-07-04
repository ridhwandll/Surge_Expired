// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Utility/Platform.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <ShlObj_core.h>

namespace Surge
{
    static bool sPersistantDirectoryExists = false;
    String Platform::GetPersistantStoragePath()
    {
        String resultantPath;
        PWSTR roamingFilePath;
        [[maybe_unused]] HRESULT result = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, &roamingFilePath);
        SG_ASSERT_NOMSG(result == S_OK);
        std::wstring filepath = roamingFilePath;
        std::replace(filepath.begin(), filepath.end(), L'\\', L'/');
        std::transform(filepath.begin(), filepath.end(), std::back_inserter(resultantPath), [](wchar_t c) { return (char)c; });
        resultantPath += "/Surge Engine";

        if (!sPersistantDirectoryExists)
            sPersistantDirectoryExists = Filesystem::CreateOrEnsureDirectories(resultantPath);
        return resultantPath;
    }

    void Platform::OpenInExplorer(const String& path)
    {
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.size(), NULL, 0);
        if(sizeNeeded <= 0)
            return;

        std::wstring wpath(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.size(), &wpath[0], sizeNeeded);

        // Convert all forward slashes to backslashes for the Shell API
        for(auto& ch : wpath)
        {
            if(ch == L'/')
                ch = L'\\';
        }

        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        bool shouldUninitialize = SUCCEEDED(hr);
        if(FAILED(hr) && hr != RPC_E_CHANGED_MODE)
            return;

        PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(wpath.c_str());
        if(pidl)
        {
            SHOpenFolderAndSelectItems(pidl, 0, NULL, 0);
            ILFree(pidl);
        }

        if(shouldUninitialize && hr != S_FALSE)
            CoUninitialize();
    }

    void Platform::OpenInVSCode(const String& workspacePath, const String& path)
    {
        SG_ASSERT_NOMSG(!path.empty());

        String combinedArgs;
        if (!workspacePath.empty())
            combinedArgs = "\"" + workspacePath + "\" \"" + path + "\"";
        else
            combinedArgs = "\"" + path + "\"";

        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, combinedArgs.c_str(), (int)combinedArgs.size(), NULL, 0);
        if(sizeNeeded <= 0)
            return;

        std::wstring wArgs(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, combinedArgs.c_str(), (int)combinedArgs.size(), &wArgs[0], sizeNeeded);

        ShellExecuteW( NULL, L"open", L"code.cmd",
            wArgs.c_str(), // Contains: "Folder" "File"
            NULL, SW_HIDE);
    }

    void Platform::RequestExit()
    {
        SendMessage(static_cast<HWND>(Core::GetWindow()->GetNativeWindowHandle()), WM_QUIT, 0, 0);
    }

    void Platform::ErrorMessageBox(const char* text)
    {
        MessageBox(NULL, text, "Error!", MB_ICONERROR | MB_OK);
    }

    glm::vec2 Platform::GetScreenSize()
    {
        return glm::vec2(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    }

    bool Platform::SetEnvVariable(const String& key, const String& value)
    {
        HKEY hKey;
        LPCSTR keyPath = "Environment";
        DWORD createdNewKey;
        LSTATUS lOpenStatus = RegCreateKeyExA(HKEY_CURRENT_USER, keyPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &createdNewKey);
        if (lOpenStatus == ERROR_SUCCESS)
        {
            LSTATUS lSetStatus = RegSetValueExA(hKey, key.c_str(), 0, REG_SZ, (LPBYTE)value.c_str(), static_cast<DWORD>(value.length()) + 1);
            RegCloseKey(hKey);

            if (lSetStatus == ERROR_SUCCESS)
            {
                SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM) "Environment", SMTO_BLOCK, 100, NULL);
                return true;
            }
        }
        return false;
    }

    bool Platform::HasEnvVariable(const String& key)
    {
        HKEY hKey;
        LPCSTR keyPath = "Environment";
        LSTATUS lOpenStatus = RegOpenKeyExA(HKEY_CURRENT_USER, keyPath, 0, KEY_ALL_ACCESS, &hKey);

        if (lOpenStatus == ERROR_SUCCESS)
        {
            lOpenStatus = RegQueryValueExA(hKey, key.c_str(), 0, NULL, NULL, NULL);
            RegCloseKey(hKey);
        }

        return lOpenStatus == ERROR_SUCCESS;
    }

    String Platform::GetEnvVariable(const String& key)
    {
        HKEY hKey;
        LPCSTR keyPath = "Environment";
        DWORD createdNewKey;
        LSTATUS lOpenStatus = RegCreateKeyExA(HKEY_CURRENT_USER, keyPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &createdNewKey);
        if (lOpenStatus == ERROR_SUCCESS)
        {
            DWORD valueType;
            char* data = new char[512];
            DWORD dataSize = 512;
            LSTATUS status = RegGetValueA(hKey, NULL, key.c_str(), RRF_RT_ANY, &valueType, (PVOID)data, &dataSize);
            RegCloseKey(hKey);

            if (status == ERROR_SUCCESS)
            {
                String result(data);
                delete[] data;
                return result;
            }
        }

        return "";
    }

    void* Platform::LoadSharedLibrary(const String& path)
    {
        HMODULE library = LoadLibrary(path.c_str());
        return library;
    }

    void* Platform::GetFunction(void* library, const String& procAddress)
    {
        void* functionAdress = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(GetProcAddress((HMODULE)library, procAddress.c_str())));
        return functionAdress;
    }

    void Platform::UnloadSharedLibrary(void* library)
    {
        FreeLibrary((HMODULE)library);
    }

    // Look at this stackoverflow post for other platfrom implementation: https://stackoverflow.com/a/60250581/14349078
    String Platform::GetCurrentExecutablePath()
    {
        char rawPathName[MAX_PATH];
        GetModuleFileNameA(NULL, rawPathName, MAX_PATH);

        Path dir = Path(rawPathName);

        return dir.string();
    }

} // namespace Surge
