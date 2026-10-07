// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/ExportUtils.hpp"
#include "Export/ExportSession.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Serializer/Serializer.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Utility/Platform.hpp"

#include <chrono>
#include <cstring>
#include <iterator>

#undef CopyDirectory // Fuckass Windows.h

namespace Surge::ExportUtils
{
    namespace fs = std::filesystem;

    String Quote(const String& s)
    {
        String r = "\"";
        for(char c : s)
            r += (c == '"') ? '\'' : c;
        r += "\"";
        return r;
    }

    String SanitizeFileName(const String& name)
    {
        String r;
        for(char c : name)
            r += (std::strchr("<>:\"/\\|?*", c) != nullptr || static_cast<unsigned char>(c) < 32) ? '_' : c;
        while(!r.empty() && (r.back() == ' ' || r.back() == '.'))
            r.pop_back();
        return r.empty() ? String("Game") : r;
    }

    String VersionString(const ExportBranding& branding, bool withBuild)
    {
        String v = std::to_string(branding.Major) + "." + std::to_string(branding.Minor) + "." + std::to_string(branding.Patch);
        if(withBuild)
            v += "." + std::to_string(branding.Build);
        return v;
    }

    String FormatSize(uintmax_t bytes)
    {
        const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        return mb >= 1.0 ? std::format("{:.1f} MB", mb) : std::format("{:.0f} KB", static_cast<double>(bytes) / 1024.0);
    }

    String FormatDuration(float seconds)
    {
        const int total = static_cast<int>(seconds);
        return total >= 60 ? std::format("{}m {:02}s", total / 60, total % 60) : std::format("{}s", total);
    }

    Path GetProjectDir()
    {
        return Path(Core::GetAssetManager()->GetAssetsDirectory()).parent_path();
    }

    static bool PathContains(const Path& outer, const Path& inner) // true if inner == outer or inside it
    {
        std::error_code ec;
        const Path rel = fs::relative(inner, outer, ec);
        return !ec && !rel.empty() && *rel.begin() != Path("..");
    }

    bool ValidateOutputFolder(const String& rawPath, bool clean, String& reason)
    {
        std::error_code ec;
        const Path out = fs::weakly_canonical(Path(rawPath), ec);
        if(ec || out.empty())
        {
            reason = "Output folder could not be resolved.";
            return false;
        }

        const Path proj = fs::weakly_canonical(GetProjectDir(), ec);
        if(PathContains(proj, out))
        {
            reason = "Output folder must not be inside the project folder.";
            return false;
        }

        if(clean)
        {
            if(out == out.root_path())
            {
                reason = "Refusing to clean a drive root.";
                return false;
            }

            const Path rel = out.relative_path();
            if(std::distance(rel.begin(), rel.end()) < 2)
            {
                reason = "Refusing to clean a top-level folder. Pick a dedicated subfolder (e.g. D:/Dev/Builds/MyGame).";
                return false;
            }

            if(PathContains(out, proj))
            {
                reason = "Cleaning would delete the project folder.";
                return false;
            }

            if(PathContains(out, fs::weakly_canonical(fs::current_path(ec), ec)))
            {
                reason = "Cleaning would delete the editor's working directory.";
                return false;
            }
        }
        return true;
    }

    static void CleanDirectory(ExportSession& session, const Path& dir)
    {
        std::error_code ec;
        if(!fs::exists(dir, ec))
            return;

        Vector<Path> entries;
        for(const auto& entry : fs::directory_iterator(dir, ec))
            entries.push_back(entry.path());

        for(const Path& p : entries)
        {
            std::error_code rec;
            fs::remove_all(p, rec);
            if(rec)
                session.Log(Severity::Warn, "Could not remove " + p.string() + ": " + rec.message());
        }
        session.Log(Severity::Info, "Cleaned " + dir.string());
    }

    void PrepareOutputFolder(ExportSession& session, const Path& out, bool clean)
    {
        if(clean)
            CleanDirectory(session, out);
        Filesystem::CreateOrEnsureDirectories(out);
    }

    void StageRuntimeContent(const Path& root, const ExportJobInfo& job)
    {
        const Path fontsPath = root / "Engine/Assets/Fonts";
        Filesystem::CreateOrEnsureDirectories(fontsPath);
        Filesystem::CopyDirectory("Engine/Assets/Fonts", fontsPath);

        const Path shadersPath = root / "Engine/Assets/Shaders";
        Filesystem::CreateOrEnsureDirectories(shadersPath);
        Filesystem::CopyDirectory("Engine/Assets/Shaders", shadersPath);

        // Cooked project content
        Project project = job.Proj;
        const Path projectFilePath = root / "Engine" / (project.Name + ".surgeproj");

        // COPY THE WHOLE ASSETS FOLDER
        Filesystem::CopyDirectory(job.ProjectDir / "Assets", root / "Engine/Assets");
        fs::remove_all(root / "Engine/Assets/Meshes");
        fs::remove_all(root / "Engine/Assets/Scripts");
        fs::remove_all(root / "Engine/Assets/Audio");
        fs::remove_all(root / "Engine/Assets/Fonts");
        fs::remove_all(root / "Engine/Assets/Materials");

        Filesystem::CopyFile(job.ProjectDir / (project.Name + ".surgeproj"), projectFilePath);

        // Rewrite the .surgeproj so it points at the chosen start scene
        Serializer::SerializeProject(projectFilePath, &project);
    }

    ScopedEnv::ScopedEnv(const String& name, const String& value)
        : Name(name)
    {
        Platform::SetEnvVariableForCurrentProcess(name, value);
    }

    ScopedEnv::~ScopedEnv()
    {
        Platform::SetEnvVariableForCurrentProcess(Name, "");
    }

    bool CachedExists::Check(const String& path)
    {
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if(mChecked != path || now - mTime > 1.0)
        {
            std::error_code ec;
            mChecked = path;
            mExists = !path.empty() && fs::exists(path, ec);
            mTime = now;
        }
        return mExists;
    }

} // namespace Surge::ExportUtils
