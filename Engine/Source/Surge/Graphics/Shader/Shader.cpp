// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Shader/Shader.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Utility/Filesystem.hpp"

#ifdef SURGE_PLATFORM_WINDOWS
#include <shaderc/shaderc.hpp>
#elif defined(SURGE_PLATFORM_ANDROID)
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#endif

namespace Surge
{
	void Shader::ParseShader()
	{
#if defined(SURGE_PLATFORM_WINDOWS)
		String source = Filesystem::ReadFile<String>(String(ENGINE_SHADER_PATH) + "/" + mName);
		const char* typeToken = "[Shader:";
		size_t typeTokenLength = strlen(typeToken);
		size_t pos = source.find(typeToken, 0);
		while (pos != std::string::npos)
		{
			size_t eol = source.find_first_of("\r\n", pos);
			size_t begin = pos + typeTokenLength + 1;
			String type = source.substr(begin, eol - begin);
			type.pop_back(); // Remove the closing ']' character
			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			Log<Severity::Info>("Parsing shader type: {0}", type);
			ShaderType shaderType = VulkanUtils::ShaderTypeFromString(type);
			SG_ASSERT((int)shaderType, "Invalid shader type!");
			pos = source.find(typeToken, nextLinePos);
			mShaderSources[shaderType] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
			//mHashCodes[shaderType] = Hash().Generate<String>(mShaderSources.at(shaderType));
			mTypesBit |= shaderType;
		}
#endif
	}

#ifdef SURGE_PLATFORM_WINDOWS
	static shaderc_shader_kind ShadercShaderKindFromSurgeShaderType(const ShaderType& type)
	{
		switch (type)
		{
		case ShaderType::VERTEX: return shaderc_glsl_vertex_shader;
		case ShaderType::FRAGMENT: return shaderc_glsl_fragment_shader;
		case ShaderType::COMPUTE: return shaderc_glsl_compute_shader;
		case ShaderType::NONE: SG_ASSERT_INTERNAL("ShaderType::None is invalid in this case!");
		}

		return static_cast<shaderc_shader_kind>(-1);
	}
#endif


	void Shader::Compile()
	{
		// We compile only on windows, on android we load the precompiled SPIRV from the assets folder
		//const VulkanRHI& vulkanRHI = Core::GetRenderer()->GetRHI()->GetBackendRHI();

#ifdef SURGE_PLATFORM_WINDOWS
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
		// NOTE(Rid - AC3R) If we enable optimization, it removes the name :kekCry:
		options.SetOptimizationLevel(shaderc_optimization_level_performance);
		options.SetGenerateDebugInfo();

		for (auto&& [stage, source] : mShaderSources)
		{
			SPIRVHandle spirvHandle;
			spirvHandle.Type = stage;

			shaderc::CompilationResult result = compiler.CompileGlslToSpv(source, ShadercShaderKindFromSurgeShaderType(stage), mName.c_str(), options);
			if (result.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				Log<Severity::Error>("{} Shader compilation failure!", VulkanUtils::ShaderTypeToString(stage));
				Log<Severity::Error>("{} Error(s): \n{1}", result.GetNumErrors(), result.GetErrorMessage());
				SG_ASSERT_INTERNAL("Shader Compilation failure!");
			}
			else
				spirvHandle.SPIRV = Vector<Uint>(result.cbegin(), result.cend());

			String path = GetShaderCachePath(stage);
			FILE* f = nullptr;
			f = fopen(path.c_str(), "wb");
			if (f)
			{
				fwrite(spirvHandle.SPIRV.data(), sizeof(Uint), spirvHandle.SPIRV.size(), f);
				fclose(f);
				Log<Severity::Info>("Cached Shader at: {0}", path);
			}

			SG_ASSERT(!spirvHandle.SPIRV.empty(), "Invalid SPIRV!");
			mShaderSPIRVs.push_back(spirvHandle);
		}

#elif defined(SURGE_PLATFORM_ANDROID)
        Uint value = static_cast<Uint>(mTypesBit);
        while (value > 0)
		{
            Uint lsb = value & -value;

			SPIRVHandle spirvHandle;
			spirvHandle.Type = static_cast<ShaderType>(lsb);

			android_app* app = Android::GAndroidApp;
			AAssetManager* assetManager = app->activity->assetManager;
			AAssetDir* assetDir = AAssetManager_openDir(assetManager, ENGINE_SHADER_PATH); // Path relative to assets folder

            String fullPath = GetShaderCachePath(spirvHandle.Type);
			AAsset* asset = AAssetManager_open(assetManager, fullPath.c_str(), AASSET_MODE_BUFFER);

			// Read the buffer
			size_t size = AAsset_getLength(asset);
			if (size % 4 != 0)
			{
				Log<Severity::Error>("Shader %s size is not a multiple of 4!", mName);
				AAsset_close(asset);
			}

			Vector<Uint> spirvCode(size / sizeof(Uint));
			AAsset_read(asset, spirvCode.data(), size);
			AAsset_close(asset);
			spirvHandle.SPIRV = spirvCode;

			SG_ASSERT(!spirvHandle.SPIRV.empty(), "Invalid SPIRV!");
			mShaderSPIRVs.push_back(spirvHandle);

            value ^= lsb;
        }
#endif
		ShaderReflector reflector;
		mReflectionData = reflector.Reflect(mName, mShaderSPIRVs);
		mReflectionData.LogAll();
	}

	String Shader::GetShaderCachePath(ShaderType type)
	{
		String path = std::format("{0}/{1}_{2}.spv", ENGINE_SHADER_PATH, mName, VulkanUtils::ShaderTypeToString(type));
		return path;
	}

	void Shader::Load(const String& name, ShaderType type)
	{
		mName = name;
		mTypesBit = ShaderType::NONE;
		ParseShader();
#ifdef SURGE_PLATFORM_WINDOWS		
		//On android we directly laod SPIRV from the assets folder, so we don't need to check if the shader type is present in the shader file
		SG_ASSERT(type == mTypesBit, "Given shader type does not match the types present in the shader file! Make sure to use proper ShaderType in Shader::Load");
#endif
		mTypesBit = type; // We need this for ANDROID as we directly load SPIRV from the assets folder, so we need to know which stages to load
        Compile();
	}

} // namespace Surge
