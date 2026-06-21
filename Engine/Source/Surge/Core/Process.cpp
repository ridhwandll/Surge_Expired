// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Process.hpp"
#include "Surge/Core/Logger/Logger.hpp"
#include <fcntl.h>


#if defined(SURGE_PLATFORM_WINDOWS)
#include <Windows.h>
#include <corecrt_io.h>
#define fdopen _fdopen
#elif defined(SURGE_LINUX) || defined(SURGE_APPLE)
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(SURGE_PLATFORM_WINDOWS)
using ProcessID = HANDLE;
#elif defined(SURGE_PLATFORM_ANDROID)
using ProcessID = int*; //Dummy
#elif defined(SURGE_LINUX) || defined(SURGE_APPLE)
using ProcessID = pid_t;
#endif

namespace Surge
{
    static ProcessID StartProcess([[maybe_unused]] const String& commandLine, [[maybe_unused]] FILE* outputStream)
    {
#if defined(SURGE_PLATFORM_WINDOWS)
        STARTUPINFOW startupInfo = {};
        startupInfo.cb = sizeof(STARTUPINFO);
        startupInfo.wShowWindow = SW_HIDE;
        startupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        startupInfo.hStdOutput = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(outputStream)));
        startupInfo.hStdError = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(outputStream)));

        PROCESS_INFORMATION processInfo;
        std::wstring wstr(commandLine.begin(), commandLine.end());
        CreateProcessW(nullptr, static_cast<LPWSTR>(wstr.data()), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startupInfo, &processInfo);
        SURGE_GET_WIN32_LAST_ERROR
        CloseHandle(processInfo.hThread);

        return processInfo.hProcess;

#elif defined SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("Android process is not supported yet");
        return nullptr;

#elif defined(SURGE_LINUX) || defined(SURGE_APPLE)
        ProcessID PID = fork();
        if (!PID) // The child
        {
            // Take control of output
            dup2(fileno(outputStream), 1);
            dup2(fileno(outputStream), 2);
            execl("/bin/sh", "/bin/sh", "-c", UTF8Converter().to_bytes(commandLine.data(), commandLine.data() + commandLine.size()).c_str(), NULL);
            exit(EXIT_FAILURE);
        }
        return PID;
#endif
    }

    static int WaitProcess([[maybe_unused]] ProcessID pid)
    {
#if defined(SURGE_PLATFORM_WINDOWS)
        BOOL result;
        DWORD exitCode;
        result = GetExitCodeProcess(pid, &exitCode);
        while (exitCode == STILL_ACTIVE)
        {
            result = GetExitCodeProcess(pid, &exitCode);
            Sleep(1);
        }

        SURGE_GET_WIN32_LAST_ERROR
        CloseHandle(pid);

        return result ? static_cast<int>(exitCode) : -1;
#elif defined SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("Android process is not supported yet");
        return 0;
#elif defined(SURGE_LINUX) || defined(SURGE_APPLE)
        int status;
        waitpid(pid, &status, 0);
        return status;
#endif
    }

    int Process::ResultOf(const String& commandLine)
    {
        ProcessID pid = StartProcess(commandLine, stdout);
        return WaitProcess(pid);
    }

    String Process::OutputOf([[maybe_unused]] const String& commandLine, [[maybe_unused]] int& result)
    {
#if defined(SURGE_PLATFORM_WINDOWS)
        HANDLE read;
        HANDLE write;
        SECURITY_ATTRIBUTES securityAttributes = {};
        securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
        securityAttributes.bInheritHandle = TRUE;
        securityAttributes.lpSecurityDescriptor = nullptr;

        if (CreatePipe(&read, &write, &securityAttributes, 0))
        {
            String output;
            String ansiBuffer;
            FILE* procOutputHandle = fdopen(_open_osfhandle(reinterpret_cast<intptr_t>(write), _O_APPEND), "w");
            ProcessID PID = StartProcess(commandLine, procOutputHandle);
            result = WaitProcess(PID);

            DWORD bytesAvailable;
            if (PeekNamedPipe(read, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable)
            {
                std::wstring outputWstr;
                ansiBuffer.resize(bytesAvailable);
                outputWstr.resize(bytesAvailable);

                ReadFile(read, ansiBuffer.data(), bytesAvailable, nullptr, nullptr);
                outputWstr.resize(bytesAvailable);
                MultiByteToWideChar(CP_ACP, 0, ansiBuffer.c_str(), bytesAvailable, outputWstr.data(), bytesAvailable);
                output = String(outputWstr.begin(), outputWstr.end());
            }
            fclose(procOutputHandle);
            CloseHandle(read);
            return output;
        }
        return {};
#elif defined SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("Android process is not supported yet");
        return {};

#elif defined(SURGE_LINUX) || defined(SURGE_APPLE)
        int fileDescriptors[2];
        pipe(fileDescriptors);
        fcntl(fileDescriptors[0], F_SETFL, O_NONBLOCK);

        FILE* stream = fdopen(fileDescriptors[1], "w");
        ProcessID PID = StartProcess(commandLine, stream);
        result = WaitProcess(PID);

        char buffer[1024];
        ssize_t length;
        String output;

        while ((length = read(fileDescriptors[0], buffer, std::size(buffer))) > 0)
        {
            output.append(buffer, length);
        }

        fclose(stream);
        close(fileDescriptors[0]);

        return output; // TODO: Convert to WSTRING
#endif
    }

    String Process::OutputOf(const String& commandLine)
    {
        int result;
        return OutputOf(commandLine, result);
    }

} // namespace Surge
