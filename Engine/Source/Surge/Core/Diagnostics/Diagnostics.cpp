// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Diagnostics/Diagnostics.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Client.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Utility/Platform.hpp"

#include <algorithm>
#include <iterator>
#include <thread>

namespace Surge
{
    static constexpr float REQUEST_KEEPALIVE = 1.0f;
    static constexpr float STATUS_INTERVAL = 1.0f; // Thermal state (a binder call on Android) and CPU usage

    // "Intel(R) Core(TM) i7-10750H CPU @ 2.60GHz" -> "Intel Core i7-10750H", "AMD Ryzen 7 5800H with Radeon Graphics" -> "AMD Ryzen 7 5800H"
    static String TidyCPUName(String name)
    {
        for(const char* mark : { "(R)", "(r)", "(TM)", "(tm)" })
        {
            for(size_t at = name.find(mark); at != String::npos; at = name.find(mark))
                name.erase(at, std::char_traits<char>::length(mark));
        }
        for(const char* tail : { " CPU @", " @", " with " })
        {
            if(const size_t at = name.find(tail); at != String::npos)
                name.erase(at);
        }

        String result;
        result.reserve(name.size());
        for(const char c : name)
        {
            if(c != ' ' || (!result.empty() && result.back() != ' '))
                result.push_back(c);
        }
        while(!result.empty() && result.back() == ' ')
            result.pop_back();
        return result;
    }

    void Diagnostics::Initialize()
    {
        mTimings.Scopes.reserve(64);
        mTimings.Groups.reserve(16);
        mScopeSums.reserve(64);
        mCounters.GPUName = Core::GetRenderer()->GetRHI()->GetStats().GPUName;
        mCounters.CPUName = TidyCPUName(Platform::GetCPUName());
        mCounters.CPUThreads = std::max(std::thread::hardware_concurrency(), 1u);
    }

    void Diagnostics::Shutdown()
    {
        if(mOwnsGPUTiming)
            Core::GetRenderer()->GetRHI()->SetGPUTimingEnabled(false);
        mOwnsGPUTiming = false;
        mActive = false;
    }

    void Diagnostics::Request()
    {
        mRequestTime = REQUEST_KEEPALIVE;
        if(!mActive)
            Activate();
    }

    const FrameTimings& Diagnostics::GetTimings()
    {
        Request();
        return mTimings;
    }

    void Diagnostics::Activate()
    {
        mActive = true;

        // Nothing from an earlier session: its scopes may not match the graphs anymore
        mTimings = {};
        mScopeSums.clear();
        mHistory.Offset = mHistory.Count = 0;
        ResetWindow();
        mWindowTime = WINDOW * 0.6f; // First numbers after ~0.2 s instead of a full window
        mStatusTimer = 0.0f;
        mCPUSampleTime = 0.0f;
        mLastProcessCPUSeconds = -1.0; // The first sample is only the baseline: the time while idle isn't in the window

        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();
        mLastResolvedFrame = rhi->GetGPUTimings().ResolvedFrames;
        if(mGPUTimingAllowed && SupportsGPUTiming() && !rhi->IsGPUTimingEnabled())
        {
            rhi->SetGPUTimingEnabled(true);
            mOwnsGPUTiming = true;
        }
    }

    void Diagnostics::EndFrame(float frameMilliseconds)
    {
        if(!mActive)
            return;

        SURGE_PROFILE_FUNC("Diagnostics::EndFrame");
        const float deltaTime = Core::GetClock().GetSeconds();
        mRequestTime -= deltaTime;
        if(mRequestTime <= 0.0f)
        {
            // Nobody read for a while: idle until the next Request()
            mActive = false;
            if(mOwnsGPUTiming)
                Core::GetRenderer()->GetRHI()->SetGPUTimingEnabled(false);
            mOwnsGPUTiming = false;
            return;
        }

        // Frame time includes every wait, CPU work is what the CPU itself spent: GPU bound, the frame is mostly these waits
        const float frameTime = Core::GetClock().GetMilliseconds();
        const float cpuWork = std::max(frameMilliseconds - Core::GetRenderer()->GetRHI()->GetStats().CPUWaitMilliseconds, 0.0f);
        mFrameSum += frameTime;
        mFramePeak = std::max(mFramePeak, frameTime);
        mCPUWorkSum += cpuWork;
        mCPUWorkPeak = std::max(mCPUWorkPeak, cpuWork);
        mFrameSamples++;
        mTimings.LastFrameMs = frameTime;

        AccumulateGPU();
        CaptureCounters();

        mHistory.FrameMs[mHistory.Offset] = frameTime;
        mHistory.CPUWorkMs[mHistory.Offset] = cpuWork;
        mHistory.GPUWorkMs[mHistory.Offset] = mTimings.LastGPUWorkMs;
        mHistory.Offset = (mHistory.Offset + 1) % FrameHistory::SIZE;
        mHistory.Count = std::min(mHistory.Count + 1, FrameHistory::SIZE);

        mCPUSampleTime += deltaTime;
        mStatusTimer -= deltaTime;
        if(mStatusTimer <= 0.0f)
        {
            mStatusTimer = STATUS_INTERVAL;
            mTimings.ThermalStatus = Platform::GetThermalStatus();
            SampleCPUUsage();
        }

        mWindowTime += deltaTime;
        if(mWindowTime >= WINDOW)
            Publish();
    }

    void Diagnostics::SampleCPUUsage()
    {
        const double cpuSeconds = Platform::GetProcessCPUSeconds();
        if(cpuSeconds >= 0.0 && mLastProcessCPUSeconds >= 0.0 && mCPUSampleTime > 0.0f)
        {
            const double usage = (cpuSeconds - mLastProcessCPUSeconds) / (static_cast<double>(mCPUSampleTime) * mCounters.CPUThreads) * 100.0;
            mTimings.CPUUsage = static_cast<float>(std::clamp(usage, 0.0, 100.0));
        }
        mLastProcessCPUSeconds = cpuSeconds;
        mCPUSampleTime = 0.0f;
    }

    void Diagnostics::AccumulateGPU()
    {
        // Results land FRAMES_IN_FLIGHT frames late and not every frame: only new ones count
        const GPUTimings& gpu = Core::GetRenderer()->GetRHI()->GetGPUTimings();
        if(!gpu.Valid || gpu.ResolvedFrames == mLastResolvedFrame)
            return;

        mLastResolvedFrame = gpu.ResolvedFrames;

        // A view opened/closed or a pass toggled: the scopes no longer line up, restart them
        Vector<DiagnosticsScope>& scopes = mTimings.Scopes;
        bool sameLayout = scopes.size() == gpu.Scopes.size();
        for(size_t i = 0; sameLayout && i < scopes.size(); i++)
            sameLayout = scopes[i].Depth == gpu.Scopes[i].Depth && scopes[i].Name == gpu.Scopes[i].Name;

        if(!sameLayout)
        {
            scopes.resize(gpu.Scopes.size());
            for(size_t i = 0; i < scopes.size(); i++)
            {
                scopes[i].Name = gpu.Scopes[i].Name;
                scopes[i].Depth = gpu.Scopes[i].Depth;
                scopes[i].AverageMs = scopes[i].MaxMs = gpu.Scopes[i].Milliseconds;
            }
            mScopeSums.assign(scopes.size(), {});
            ResetGPUWindow();
        }

        float work = 0.0f;
        for(size_t i = 0; i < scopes.size(); i++)
        {
            const float milliseconds = gpu.Scopes[i].Milliseconds;
            mScopeSums[i].Sum += milliseconds;
            mScopeSums[i].Peak = std::max(mScopeSums[i].Peak, milliseconds);
            if(gpu.Scopes[i].Depth == 0)
                work += milliseconds;
        }

        mTimings.LastGPUWorkMs = work;
        mTimings.GPUOverflowed = gpu.Overflowed;
        mGPUWorkSum += work;
        mGPUWorkPeak = std::max(mGPUWorkPeak, work);
        mGPUCommandBufferSum += gpu.FrameMilliseconds;
        mGPUDisplayWaitSum += gpu.DisplayWaitMilliseconds;
        mGPUSamples++;
    }

    void Diagnostics::CaptureCounters()
    {
        Renderer* renderer = Core::GetRenderer();
        const RHIStats& rhiStats = renderer->GetRHI()->GetStats();
        const FrameBlackboard& blackBoard = renderer->GetRenderGraphBlackBoard();
        const RendererFrameCounts& counts = renderer->GetLastFrameCounts();

        mCounters.DrawCalls = rhiStats.DrawCalls;
        mCounters.Triangles = rhiStats.Triangles;
        mCounters.GPUMemoryMB = rhiStats.UsedGPUMemory;
        mCounters.GPUMemoryBudgetMB = rhiStats.TotalAllowedGPUMemory;
        mCounters.Meshes = counts.Meshes;
        mCounters.Lights = counts.Lights;
        mCounters.Sprites = counts.Sprites;
        mCounters.Texts = counts.Texts;
        mCounters.UIItems = counts.UIItems;
        mCounters.MaxLights = blackBoard.Lighting.MaxLights;
        mCounters.Particles = blackBoard.ParticleStatistics.AliveParticles;
        mCounters.MaxParticles = blackBoard.Particles.MaxParticles;
        mCounters.Animators = blackBoard.AnimationStatistics.Animators;

        mCounters.ScreenWidth = static_cast<Uint>(blackBoard.ScreenWidth);
        mCounters.ScreenHeight = static_cast<Uint>(blackBoard.ScreenHeight);
        const glm::uvec2 renderSize = blackBoard.GetSceneSize(mCounters.ScreenWidth, mCounters.ScreenHeight);
        mCounters.RenderWidth = renderSize.x;
        mCounters.RenderHeight = renderSize.y;
        mCounters.RenderScale = blackBoard.RenderScale;

        const Ref<Scene> scene = Core::GetClient()->GetRuntimeScene();
        mCounters.Entities = scene ? static_cast<Uint>(scene->GetRegistry().alive()) : 0;
    }

    void Diagnostics::Publish()
    {
        const float frameSamples = static_cast<float>(std::max(mFrameSamples, 1u));
        mTimings.FrameMs = mFrameSum / frameSamples;
        mTimings.FrameMaxMs = mFramePeak;
        mTimings.CPUWorkMs = mCPUWorkSum / frameSamples;
        mTimings.CPUWorkMaxMs = mCPUWorkPeak;
        mTimings.FPS = mTimings.FrameMs > 0.0f ? 1000.0f / mTimings.FrameMs : 0.0f;

        mTimings.GPUTiming = mGPUSamples > 0;
        const float gpuSamples = static_cast<float>(std::max(mGPUSamples, 1u));
        mTimings.GPUWorkMs = mGPUWorkSum / gpuSamples;
        mTimings.GPUWorkMaxMs = mGPUWorkPeak;
        mTimings.GPUCommandBufferMs = mGPUCommandBufferSum / gpuSamples;
        mTimings.GPUDisplayWaitMs = mGPUDisplayWaitSum / gpuSamples;
        if(!mTimings.GPUTiming)
            mTimings.LastGPUWorkMs = 0.0f;

        Vector<DiagnosticsScope>& scopes = mTimings.Scopes;
        for(size_t i = 0; mTimings.GPUTiming && i < scopes.size(); i++)
        {
            scopes[i].AverageMs = mScopeSums[i].Sum / gpuSamples;
            scopes[i].MaxMs = mScopeSums[i].Peak;
        }

        // Groups summed by name over the graphs, names assigned into the existing strings
        Vector<DiagnosticsGroup>& groups = mTimings.Groups;
        size_t groupCount = 0;
        for(const DiagnosticsScope& scope : scopes)
        {
            if(!mTimings.GPUTiming || scope.Depth != 1)
                continue;

            size_t index = 0;
            while(index < groupCount && groups[index].Name != scope.Name)
                index++;
            if(index == groupCount)
            {
                if(groupCount == groups.size())
                    groups.emplace_back();
                groups[index].Name = scope.Name;
                groups[index].AverageMs = 0.0f;
                groupCount++;
            }
            groups[index].AverageMs += scope.AverageMs;
        }
        groups.resize(groupCount);
        std::erase_if(groups, [](const DiagnosticsGroup& group) { return group.AverageMs <= 0.0f; });
        std::sort(groups.begin(), groups.end(), [](const DiagnosticsGroup& a, const DiagnosticsGroup& b) { return a.AverageMs > b.AverageMs; });

        ResetWindow();
    }

    void Diagnostics::ResetWindow()
    {
        mFrameSum = mFramePeak = 0.0f;
        mCPUWorkSum = mCPUWorkPeak = 0.0f;
        mFrameSamples = 0;
        mWindowTime = 0.0f;
        ResetGPUWindow();
    }

    void Diagnostics::ResetGPUWindow()
    {
        for(ScopeSum& sum : mScopeSums)
            sum = {};
        mGPUWorkSum = mGPUWorkPeak = mGPUCommandBufferSum = mGPUDisplayWaitSum = 0.0f;
        mGPUSamples = 0;
    }

    void Diagnostics::SetGPUTimingEnabled(bool enabled)
    {
        if(!SupportsGPUTiming())
            return;

        mGPUTimingAllowed = enabled;
        mOwnsGPUTiming = false; // Explicit: stays as set when measuring goes idle
        Core::GetRenderer()->GetRHI()->SetGPUTimingEnabled(enabled);

        mTimings.Scopes.clear();
        mTimings.Groups.clear();
        mTimings.GPUTiming = false;
        mScopeSums.clear();
        ResetGPUWindow();
    }

    bool Diagnostics::IsGPUTimingEnabled() const
    {
        return Core::GetRenderer()->GetRHI()->IsGPUTimingEnabled();
    }

    bool Diagnostics::SupportsGPUTiming() const
    {
        return RHISettings::GPU_TIMING && Core::GetRenderer()->GetRHI()->SupportsGPUTiming();
    }

    const char* Diagnostics::ThermalStatusToString(int status)
    {
        static constexpr const char* NAMES[] = { "None", "Light", "Moderate", "Severe", "Critical", "Emergency", "Shutdown" };
        return status >= 0 && status < static_cast<int>(std::size(NAMES)) ? NAMES[status] : "";
    }

} // namespace Surge
