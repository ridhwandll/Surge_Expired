// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/ExportUtils.hpp"
#include "Export/ExportSession.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/Shader/ShaderBinaryFormat.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "Shader/ShaderCompiler.hpp"
#include "Asset/AssetDatabase.hpp"
#include "Surge/Asset/Format/Registry/RegistryFormat.hpp"
#include "Surge/Core/Project.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Utility/Platform.hpp"

#include <algorithm>
#include <chrono>
#include <format>
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

    bool CollectCookedContent(ExportJobInfo& job, Vector<String>& outMissing)
    {
        AssetManager* am = Core::GetAssetManager();
        const Path assetsDir = am->GetAssetsDirectory();

        job.CookedFiles.clear();
        job.CookedFiles.reserve(am->GetRegistryMap().size());
        for(const auto& [id, meta] : am->GetRegistryMap())
        {
            if(HasFlag(meta.Flags, AssetFlags::MEMORY))
                continue; // Generated at runtime

            const Path sidecar = am->GetSidecarPath(id);
            std::error_code ec;
            if(!fs::is_regular_file(sidecar, ec))
            {
                outMissing.push_back(meta.RelativePath);
                continue;
            }
            job.CookedFiles.push_back({ sidecar, fs::relative(sidecar, assetsDir, ec) });
        }

        // The cooked registry and project: the Player reads them instead of AssetRegistry.surge and the .surgeproj (main thread, the registry can change)
        {
            std::error_code ec;
            const Path registryPath = assetsDir / RegistryBinary::FILE_NAME;
            const Path projectPath = assetsDir / Project::COOKED_FILE_NAME;
            if(AssetDatabase::Get().WriteCookedRegistry(registryPath))
                job.CookedFiles.push_back({ registryPath, fs::relative(registryPath, assetsDir, ec) });
            else
                outMissing.push_back(RegistryBinary::FILE_NAME);
            if(job.Proj.SaveCooked(projectPath))
                job.CookedFiles.push_back({ projectPath, fs::relative(projectPath, assetsDir, ec) });
            else
                outMissing.push_back(Project::COOKED_FILE_NAME);
        }

        // Engine shaders ship cooked only (Shaders/Cooked/<Name>.rSHADER), compiled by ShaderCompiler::CookLoadedShaders
        for(const Shader& shader : Core::GetRenderer()->GetShaderManager().GetAllShaders())
        {
            const String cookedPath = ShaderBinary::GetCookedPath(shader.GetPath());
            if(!ShaderCompiler::IsUpToDate(shader.GetPath(), cookedPath))
            {
                outMissing.push_back(shader.GetPath());
                continue;
            }
            std::error_code ec;
            job.CookedFiles.push_back({ cookedPath, fs::relative(cookedPath, "Engine/Assets", ec) });
        }

        // Engine UI font (performance overlay, UI text without a font), cooked by the Editor at startup
        {
            std::error_code ec;
            if(fs::is_regular_file(EngineFont::COOKED_PATH, ec))
                job.CookedFiles.push_back({ EngineFont::COOKED_PATH, fs::relative(EngineFont::COOKED_PATH, "Engine/Assets", ec) });
            else
                outMissing.push_back(EngineFont::COOKED_PATH);
        }

        std::sort(outMissing.begin(), outMissing.end());
        return outMissing.empty();
    }

    bool StageRuntimeContent(ExportSession& session, const Path& root, const ExportJobInfo& job)
    {
        // Cooked project content only: Internal/ (assets, Registry.rREG, Project.rPROJ), nothing else of the project's Assets folder
        const Path assetsPath = root / "Engine/Assets";
        Filesystem::CreateOrEnsureDirectories(assetsPath);

        for(const ExportCookedFile& file : job.CookedFiles)
        {
            if(session.IsCancelled())
                return false;

            const Path destination = assetsPath / file.Destination;
            Filesystem::CreateOrEnsureDirectories(destination.parent_path());
            if(!Filesystem::CopyFile(file.Source, destination))
            {
                session.Error("Could not copy " + file.Source.string() + " to " + destination.string());
                return false;
            }
        }
        session.Log(Severity::Info, std::format("Staged {} cooked asset(s)", job.CookedFiles.size()));

        return true;
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
