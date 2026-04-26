// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Utility/Platform.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <ShlObj_core.h>
#include "Surge/Core/Core.hpp"

namespace Surge
{
    static bool sPersistantDirectoryExists = false;
    String Platform::GetPersistantStoragePath()
    {
        String resultantPath;
        PWSTR roamingFilePath;
        HRESULT result = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, &roamingFilePath);
        SG_ASSERT_NOMSG(result == S_OK);
        std::wstring filepath = roamingFilePath;
        std::replace(filepath.begin(), filepath.end(), L'\\', L'/');
        std::transform(filepath.begin(), filepath.end(), std::back_inserter(resultantPath), [](wchar_t c) { return (char)c; });
        resultantPath += "/Surge Engine";

        if (!sPersistantDirectoryExists)
            sPersistantDirectoryExists = Filesystem::CreateOrEnsureDirectory(resultantPath);
        return resultantPath;
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

        return String();
    }

    void* Platform::LoadSharedLibrary(const String& path)
    {
        HMODULE library = LoadLibrary(path.c_str());
        return library;
    }

    void* Platform::GetFunction(void* library, const String& procAddress)
    {
        void* functionAdress = GetProcAddress((HMODULE)library, procAddress.c_str());
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

        return dir;
    }

} // namespace Surge
