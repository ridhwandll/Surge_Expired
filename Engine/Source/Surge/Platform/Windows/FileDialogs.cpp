// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Utility/FileDialogs.hpp"
#include <commdlg.h>
#include <ShlObj_core.h>
#include "Surge/Core/Core.hpp"

namespace Surge
{
    String FileDialog::OpenFile(const char* filter)
    {
        OPENFILENAMEA ofn;
        CHAR szFile[260] = {0};
        CHAR currentDir[256] = {0};
        ZeroMemory(&ofn, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = static_cast<HWND>(Core::GetWindow()->GetNativeWindowHandle());
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        if (GetCurrentDirectoryA(256, currentDir))
            ofn.lpstrInitialDir = currentDir;
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

        if (GetOpenFileNameA(&ofn) == TRUE)
        {
            String result = ofn.lpstrFile;
            std::replace(result.begin(), result.end(), '\\', '/');
            return result;
        }

        return String();
    }

    String FileDialog::SaveFile(const char* filter)
    {
        OPENFILENAMEA ofn;
        CHAR szFile[260] = {0};
        CHAR currentDir[256] = {0};
        ZeroMemory(&ofn, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = static_cast<HWND>(Core::GetWindow()->GetNativeWindowHandle());
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        if (GetCurrentDirectoryA(256, currentDir))
            ofn.lpstrInitialDir = currentDir;
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        // Sets the default extension by extracting it from the filter
        ofn.lpstrDefExt = strchr(filter, '\0') + 1;

        if (GetSaveFileNameA(&ofn) == TRUE)
            return ofn.lpstrFile;

        return String();
    }

    Surge::String FileDialog::ChooseFolder()
    {
        TCHAR path[MAX_PATH];

        BROWSEINFO bi = {0};
        bi.lpszTitle = ("Choose Folder");
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
        bi.lpfn = NULL;
        bi.lParam = NULL;

        LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
        if (pidl != 0)
        {
            // Get the name of the folder and put it in path
            SHGetPathFromIDList(pidl, path);

            //Free memory used
            IMalloc* imalloc = 0;
            if (SUCCEEDED(SHGetMalloc(&imalloc)))
            {
                imalloc->Free((void*)pidl);
                imalloc->Release();
            }

            return path;
        }
        return "";
    }

} // namespace Surge