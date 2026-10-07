// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Export/ExportTypes.hpp"

namespace Surge
{
    class ExportSession;

    // Platform independent helpers shared by every exporter
    namespace ExportUtils
    {
        FORCEINLINE bool StartsWith(const String& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }
        FORCEINLINE bool Contains(const String& s, const char* needle) { return s.find(needle) != String::npos; }

        String Quote(const String& s); // For command lines; inner double quotes become single quotes
        String SanitizeFileName(const String& name);
        String VersionString(const ExportBranding& branding, bool withBuild); // M.m.p[.build]
        String FormatSize(uintmax_t bytes);
        String FormatDuration(float seconds);

        Path GetProjectDir();

        // Rejects output folders inside the project, and (when cleaning) drive roots, top-level folders and anything containing the project / working directory
        bool ValidateOutputFolder(const String& rawPath, bool clean, String& reason);
        void PrepareOutputFolder(ExportSession& session, const Path& out, bool clean);

        // Engine fonts/shaders + the cooked project content + a .surgeproj pointing at the chosen start scene, rooted at root
        void StageRuntimeContent(const Path& root, const ExportJobInfo& job);

        // Sets an environment variable for child processes, removes it again on scope exit
        struct ScopedEnv
        {
            String Name;
            ScopedEnv(const String& name, const String& value);
            ~ScopedEnv();
            SURGE_DISABLE_COPY_AND_MOVE(ScopedEnv);
        };

        // fs::exists() for per-frame issue checks: re-checked at most once a second unless the path changes
        class CachedExists
        {
        public:
            bool Check(const String& path);

        private:
            String mChecked;
            bool mExists = false;
            double mTime = -100.0;
        };

    } // namespace ExportUtils

} // namespace Surge
