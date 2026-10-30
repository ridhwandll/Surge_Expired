// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/Platforms/Android/AndroidLogcat.hpp"
#include "Export/ExportUtils.hpp"
#include "Surge/Core/Process.hpp"
#include "Surge/Core/Vector.hpp"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>

namespace Surge
{
    namespace fs = std::filesystem;
    using ExportUtils::Contains;
    using ExportUtils::Quote;
    using ExportUtils::StartsWith;

    namespace
    {
        constexpr int HISTORY_LINES = 5000;              // -T on the first connect: lines of a launch that happened just before attaching
        constexpr const char* ENGINE_TAG = "SurgeEngine"; // SURGE_ANDROID_LOG_TAG
        constexpr const char* NATIVE_LIB = "libSurgePlayer.so";

        void Emit(Severity severity, const String& text)
        {
            switch(severity)
            {
                case Severity::Trace: Log<Severity::Trace>("{} {}", AndroidLogcat::LOG_TAG, text); break;
                case Severity::Info:  Log<Severity::Info>("{} {}", AndroidLogcat::LOG_TAG, text); break;
                case Severity::Debug: Log<Severity::Debug>("{} {}", AndroidLogcat::LOG_TAG, text); break;
                case Severity::Warn:  Log<Severity::Warn>("{} {}", AndroidLogcat::LOG_TAG, text); break;
                case Severity::Error:
                case Severity::Fatal: Log<Severity::Error>("{} {}", AndroidLogcat::LOG_TAG, text); break;
            }
        }

        Severity FromPriority(char priority)
        {
            switch(priority)
            {
                case 'I': return Severity::Info;
                case 'W': return Severity::Warn;
                case 'E':
                case 'F':
                case 'A': return Severity::Error;
                default: return Severity::Trace; // V, D
            }
        }

        bool IsWarningOrWorse(char priority) { return priority == 'W' || priority == 'E' || priority == 'F' || priority == 'A'; }

        // Splits a process' raw output into lines
        struct LineSplitter
        {
            String Pending;

            template <typename Fn>
            void Feed(const char* data, size_t size, Fn&& onLine)
            {
                Pending.append(data, size);
                size_t start = 0;
                size_t newline;
                while((newline = Pending.find('\n', start)) != String::npos)
                {
                    size_t end = newline;
                    if(end > start && Pending[end - 1] == '\r')
                        --end;
                    onLine(Pending.substr(start, end - start));
                    start = newline + 1;
                }
                Pending.erase(0, start);
            }
        };

        // Runs a short command and returns its whole output (Process::OutputOf only reads after exit, a full pipe would block it)
        String RunCapture(const String& command, const std::atomic<bool>* cancel, int* exitCode = nullptr)
        {
            String output;
            const int result = Process::Run(command, String(), [](const char* data, size_t size, void* user) { static_cast<String*>(user)->append(data, size); }, &output, cancel);
            if(exitCode)
                *exitCode = result;
            return output;
        }

        // "-v threadtime": "10-14 04:07:39.811 22004 22025 E SurgeEngine: message"
        struct LogcatLine
        {
            int Pid = 0;
            char Priority = 'V';
            String Tag;
            String Message;
        };

        bool ParseLine(const String& line, LogcatLine& out)
        {
            size_t pos = 0;
            auto nextToken = [&]() -> String {
                while(pos < line.size() && line[pos] == ' ')
                    ++pos;
                const size_t start = pos;
                while(pos < line.size() && line[pos] != ' ')
                    ++pos;
                return line.substr(start, pos - start);
            };

            const String date = nextToken();
            const String time = nextToken();
            const String pid = nextToken();
            const String tid = nextToken();
            const String priority = nextToken();
            if(date.size() != 5 || date[2] != '-' || time.empty() || pid.empty() || tid.empty() || priority.size() != 1)
                return false;

            while(pos < line.size() && line[pos] == ' ')
                ++pos;
            size_t colon = line.find(": ", pos);
            if(colon == String::npos)
                colon = line.find(':', pos);
            if(colon == String::npos)
                return false;

            out.Pid = std::atoi(pid.c_str());
            out.Priority = priority[0];
            out.Tag = line.substr(pos, colon - pos);
            while(!out.Tag.empty() && out.Tag.back() == ' ')
                out.Tag.pop_back();
            out.Message = colon + 2 <= line.size() ? line.substr(colon + 2) : String();
            return true;
        }

        // Filters the device log down to one app and symbolizes its native crashes. Worker thread only
        class LogcatFilter
        {
        public:
            LogcatFilter(const AndroidLogcat::Request& request, std::atomic<int>& appPid)
                : mRequest(request), mAppPid(appPid)
            {
                std::error_code ec;
                mRepoRoot = fs::current_path(ec).generic_string() + "/";
            }

            String LastRawLine; // Last line that is not a log entry: adb's own errors ("more than one device", ...)

            void OnLine(const String& line)
            {
                LogcatLine entry;
                if(!ParseLine(line, entry))
                {
                    if(!line.empty() && !StartsWith(line, "---------"))
                        LastRawLine = line;
                    return;
                }

                if(entry.Tag == "ActivityManager" || entry.Tag == "Zygote")
                {
                    TrackProcess(entry);
                    return;
                }
                if(entry.Tag == "DEBUG")
                {
                    OnTombstoneLine(entry.Message);
                    return;
                }

                const int appPid = mAppPid;
                if(appPid == 0)
                    return;

                // Includes libc's "Fatal signal 11 (SIGSEGV)" line, logged by the crashing process itself
                if(entry.Pid != appPid)
                    return;
                if(entry.Tag == ENGINE_TAG)
                    Emit(FromPriority(entry.Priority), StripEngineTimestamp(entry.Message));
                else if(IsWarningOrWorse(entry.Priority)) // Drivers and the framework are chatty at Info
                    Emit(FromPriority(entry.Priority), entry.Tag + ": " + entry.Message);
            }

        private:
            // "Zygote: Process 22004 created for com.app" / "ActivityManager: Start proc 22004:com.app/u0a644 for ..."
            // "ActivityManager: Process com.app (pid 22004) has died"
            void TrackProcess(const LogcatLine& entry)
            {
                const String& pkg = mRequest.PackageId;
                const String& msg = entry.Message;
                int started = 0;

                if(StartsWith(msg, "Process ") && msg.ends_with(" created for " + pkg))
                    started = std::atoi(msg.c_str() + 8);
                else if(const size_t at = msg.find("Start proc "); at != String::npos)
                {
                    const int pid = std::atoi(msg.c_str() + at + 11);
                    const String rest = msg.substr(at + 11);
                    if(pid > 0 && StartsWith(rest.substr(rest.find(':') + 1), (pkg + "/").c_str()))
                        started = pid;
                }
                else if(Contains(msg, ("Process " + pkg + " (pid ").c_str()) && Contains(msg, "has died"))
                    Emit(Severity::Warn, std::format("App process died ({})", msg.substr(msg.find("(pid"))));

                if(started > 0 && started != mAppPid)
                {
                    mAppPid = started;
                    Emit(Severity::Info, std::format("{} started (pid {})", pkg, started));
                }
            }

            static String StripEngineTimestamp(const String& message)
            {
                // The engine's own "[04:07:39] " prefix, the Console adds its own time
                if(message.size() > 11 && message[0] == '[' && message[3] == ':' && message[9] == ']' && message[10] == ' ')
                    return message.substr(11);
                return message;
            }

            // A native crash report from debuggerd: header, "pid: ..., name: ...  >>> com.app <<<", registers, "N total frames", backtrace
            void OnTombstoneLine(const String& message)
            {
                if(StartsWith(message, "*** ***"))
                {
                    mTombstone = Tombstone::PENDING;
                    mHeader.clear();
                    mFrames.clear();
                    mFramesExpected = 0;
                    mFramesSeen = 0;
                    mHeader.push_back(message);
                    return;
                }

                switch(mTombstone)
                {
                    case Tombstone::NONE:
                    case Tombstone::OTHER_APP:
                        return;

                    case Tombstone::PENDING:
                        mHeader.push_back(message);
                        if(Contains(message, (">>> " + mRequest.PackageId + " <<<").c_str()))
                        {
                            mTombstone = Tombstone::OURS;
                            Emit(Severity::Error, "NATIVE CRASH");
                            for(const String& line : mHeader)
                                Emit(Severity::Error, line);
                            mHeader.clear();
                        }
                        else if(Contains(message, ">>> "))
                            mTombstone = Tombstone::OTHER_APP;
                        return;

                    case Tombstone::OURS:
                        break;
                }

                Emit(Severity::Error, message);

                String trimmed = message;
                trimmed.erase(0, trimmed.find_first_not_of(' '));
                if(const size_t at = trimmed.find(" total frames"); at != String::npos && !trimmed.empty() && std::isdigit(static_cast<unsigned char>(trimmed[0])))
                {
                    mFramesExpected = std::atoi(trimmed.c_str());
                    return;
                }

                const bool isFrame = StartsWith(trimmed, "#") && Contains(trimmed, " pc ");
                if(isFrame)
                {
                    mFramesSeen++;
                    if(Contains(trimmed, NATIVE_LIB))
                    {
                        Frame frame;
                        frame.Index = trimmed.substr(0, trimmed.find(' '));
                        const size_t pc = trimmed.find(" pc ") + 4;
                        frame.Pc = "0x" + trimmed.substr(pc, trimmed.find(' ', pc) - pc);
                        if(const size_t id = trimmed.find("(BuildId: "); id != String::npos)
                            frame.BuildId = trimmed.substr(id + 10, trimmed.find(')', id) - id - 10);
                        mFrames.push_back(std::move(frame));
                    }
                }

                // Older Android versions don't print "N total frames": the backtrace ends at the first line that isn't a frame
                const bool backtraceDone = (mFramesExpected > 0 && mFramesSeen >= mFramesExpected) || (!isFrame && mFramesSeen > 0);
                if(backtraceDone)
                {
                    Symbolize();
                    mTombstone = Tombstone::NONE;
                }
            }

            void Symbolize()
            {
                if(mFrames.empty())
                    return;
                if(mRequest.Symbolizer.empty())
                {
                    Emit(Severity::Warn, "Crash frames not symbolized: llvm-symbolizer not found (is the NDK installed?)");
                    return;
                }

                const Path library = FindSymbols();
                if(library.empty())
                {
                    Emit(Severity::Warn, std::format("Crash frames not symbolized: no unstripped {} under {}, build the APK on this machine first", NATIVE_LIB, mRequest.SymbolsDir.string()));
                    return;
                }

                const String& crashBuildId = mFrames.front().BuildId;
                const String libraryBuildId = ReadBuildId(library);
                if(!crashBuildId.empty() && !libraryBuildId.empty() && crashBuildId != libraryBuildId)
                {
                    Emit(Severity::Warn, std::format("Crash frames not symbolized: the app on the device (BuildId {}) is not the last build (BuildId {})", crashBuildId, libraryBuildId));
                    return;
                }

                String command = Quote(mRequest.Symbolizer.string()) + " --obj=" + Quote(library.string()) + " --inlines --pretty-print";
                for(const Frame& frame : mFrames)
                    command += " " + frame.Pc;
                const String output = RunCapture(command, nullptr);

                // One block per address, separated by a blank line: "function at file:line:col", then " (inlined by) ..." lines
                Emit(Severity::Error, "Symbolized backtrace (" + library.filename().string() + "):");
                size_t frameIndex = 0;
                LineSplitter splitter;
                bool blockStarted = false;
                splitter.Feed(output.data(), output.size(), [&](const String& line) {
                    if(line.empty())
                    {
                        if(blockStarted)
                            frameIndex++;
                        blockStarted = false;
                        return;
                    }
                    const String text = ShortenPaths(line);
                    if(!blockStarted && frameIndex < mFrames.size())
                        Emit(Severity::Error, std::format("  {} {}", mFrames[frameIndex].Index, text));
                    else
                        Emit(Severity::Error, "      " + text);
                    blockStarted = true;
                });
            }

            // Newest Android/app/build/intermediates/cxx/<config>/<hash>/obj/arm64-v8a/libSurgePlayer.so
            Path FindSymbols() const
            {
                std::error_code ec;
                Path newest;
                fs::file_time_type newestTime {};
                for(fs::recursive_directory_iterator it(mRequest.SymbolsDir, ec), end; !ec && it != end; it.increment(ec))
                {
                    if(it->path().filename() != NATIVE_LIB || it->path().parent_path().filename() != "arm64-v8a")
                        continue;
                    const fs::file_time_type time = fs::last_write_time(it->path(), ec);
                    if(!ec && (newest.empty() || time > newestTime))
                    {
                        newest = it->path();
                        newestTime = time;
                    }
                }
                return newest;
            }

            String ReadBuildId(const Path& library) const
            {
                const Path readelf = mRequest.Symbolizer.parent_path() / "llvm-readelf.exe";
                const String output = RunCapture(Quote(readelf.string()) + " -n " + Quote(library.string()), nullptr);
                const size_t at = output.find("Build ID: ");
                if(at == String::npos)
                    return {};
                const size_t start = at + 10;
                const size_t end = output.find_first_of("\r\n ", start);
                return output.substr(start, end == String::npos ? String::npos : end - start);
            }

            String ShortenPaths(String text) const
            {
                for(size_t at; (at = text.find(mRepoRoot)) != String::npos;)
                    text.erase(at, mRepoRoot.size());
                return text;
            }

        private:
            enum class Tombstone { NONE, PENDING, OURS, OTHER_APP };

            struct Frame
            {
                String Index; // "#02"
                String Pc;
                String BuildId;
            };

            const AndroidLogcat::Request& mRequest;
            std::atomic<int>& mAppPid;
            String mRepoRoot;

            Tombstone mTombstone = Tombstone::NONE;
            Vector<String> mHeader; // Until the tombstone names its app
            Vector<Frame> mFrames;
            int mFramesExpected = 0;
            int mFramesSeen = 0;
        };

        struct StreamContext
        {
            LogcatFilter* Filter;
            LineSplitter Splitter;
        };

        void OnStreamOutput(const char* data, size_t size, void* userData)
        {
            StreamContext& context = *static_cast<StreamContext*>(userData);
            context.Splitter.Feed(data, size, [&](const String& line) { context.Filter->OnLine(line); });
        }

        void SleepUnlessStopped(const std::atomic<bool>& stop, int milliseconds)
        {
            for(int waited = 0; waited < milliseconds && !stop; waited += 50)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    } // namespace

    void AndroidLogcat::Start(Request request)
    {
        Stop();

        mRequest = std::move(request);
        mStop = false;
        mRunning = true;
        mAppPid = 0;
        mState = State::CONNECTING;
        {
            std::scoped_lock lock(mErrorMutex);
            mError.clear();
        }

        mWorker = std::thread([this]() {
            Run();
            mRunning = false;
        });
    }

    void AndroidLogcat::Stop()
    {
        mStop = true;
        if(mWorker.joinable())
            mWorker.join();
        if(mState != State::FAILED)
            mState = State::IDLE;
    }

    String AndroidLogcat::GetStatusText() const
    {
        const int pid = mAppPid;
        switch(mState.load())
        {
            case State::IDLE: return "Not attached";
            case State::CONNECTING: return "Connecting to the device...";
            case State::STREAMING:
                return pid > 0 ? std::format("Attached to {} (pid {})", mRequest.PackageId, pid) : std::format("Waiting for {} to start", mRequest.PackageId);
            case State::DISCONNECTED: return "Device disconnected, waiting for it to come back";
            case State::FAILED:
            {
                std::scoped_lock lock(mErrorMutex);
                return mError;
            }
        }
        return {};
    }

    void AndroidLogcat::Fail(const String& reason)
    {
        {
            std::scoped_lock lock(mErrorMutex);
            mError = reason;
        }
        mState = State::FAILED;
        Emit(Severity::Error, reason);
    }

    void AndroidLogcat::Run()
    {
        const String adb = Quote(mRequest.Adb.string());
        Emit(Severity::Info, "Attaching to " + mRequest.PackageId);

        // Started on its own: cancelling the logcat stream terminates its process tree, which must not take the adb server with it
        RunCapture(adb + " start-server", &mStop);
        if(mStop)
            return;

        LogcatFilter filter(mRequest, mAppPid);
        bool firstConnect = true;
        int quickExits = 0;

        while(!mStop)
        {
            // Already running (attached by hand, or the launch finished before the stream started)
            if(const String pid = RunCapture(adb + " shell pidof -s " + mRequest.PackageId, &mStop); !pid.empty() && std::atoi(pid.c_str()) > 0)
                mAppPid = std::atoi(pid.c_str());

            mState = State::STREAMING;
            const auto begin = std::chrono::steady_clock::now();
            StreamContext context { &filter, {} };
            const String command = adb + " logcat -v threadtime -T " + std::to_string(firstConnect ? HISTORY_LINES : 1);
            Process::Run(command, String(), &OnStreamOutput, &context, &mStop);
            if(mStop)
                break;

            // adb exited: no device, several devices, or the cable was pulled
            const bool quick = std::chrono::steady_clock::now() - begin < std::chrono::seconds(3);
            quickExits = quick ? quickExits + 1 : 0;
            if(Contains(filter.LastRawLine, "more than one device"))
            {
                Fail("More than one device is connected: unplug the others (adb can't pick one)");
                return;
            }
            if(quickExits >= 3)
            {
                Fail("adb logcat keeps exiting" + (filter.LastRawLine.empty() ? String() : ": " + filter.LastRawLine));
                return;
            }

            mState = State::DISCONNECTED;
            Emit(Severity::Warn, "Device disconnected, waiting for it to come back");
            Process::Run(adb + " wait-for-device", String(), nullptr, nullptr, &mStop);
            if(mStop)
                break;
            if(quick)
                SleepUnlessStopped(mStop, 1000);
            firstConnect = false;
        }
    }

} // namespace Surge
