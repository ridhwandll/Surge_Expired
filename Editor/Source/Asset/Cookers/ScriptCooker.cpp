// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/Script/ScriptFormat.hpp"
#include "Surge/ScriptEngine/ScriptEngine.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    bool ScriptCooker::Cook(const CookContext& context) const
    {
        String sourceCode;
        if(!Filesystem::ReadTextFile(context.SourcePath, sourceCode))
        {
            Log<Severity::Error>("[ScriptCooker] Failed to read script: {}", context.SourcePath.string());
            return false;
        }

        // '@' + asset path: errors and tracebacks read "Scripts/Player.lua:12: ...", and require() names match it
        const Vector<Byte> bytecode = Core::GetScriptEngine()->Compile(sourceCode, "@" + context.Metadata.RelativePath);
        if(bytecode.empty())
        {
            Log<Severity::Error>("[ScriptCooker] Failed to compile script: {}", context.SourcePath.string());
            return false;
        }
        Rapt0R::Writer payload(bytecode.size());
        ScriptBinary::Write(payload, bytecode);
        return context.Write(ScriptBinary::VERSION, payload);
    }
}
