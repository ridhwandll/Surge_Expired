// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/Vector.hpp"
#include <array>

namespace Surge
{
    struct DiagnosticsScope
    {
        String Name;
        Uint Depth = 0; // 0 = a graph (every view, then the main graph), 1 = its groups, 2 = their passes
        float AverageMs = 0.0f;
        float MaxMs = 0.0f;
    };

    struct DiagnosticsGroup
    {
        String Name;
        float AverageMs = 0.0f; // Summed over the graphs that ran it (the Editor's views)
    };

    // Averaged over Diagnostics::WINDOW, Last* are single frames (graphs)
    struct FrameTimings
    {
        float FPS = 0.0f;
        float FrameMs = 0.0f;            // Frame time: the CPU work plus every wait (GPU fence, swapchain, frame pacer, cap)
        float FrameMaxMs = 0.0f;
        float CPUWorkMs = 0.0f;          // The frame's own CPU time, without those waits
        float CPUWorkMaxMs = 0.0f;
        float GPUWorkMs = 0.0f;          // Sum of the top level GPU scopes, not the swapchain wait
        float GPUWorkMaxMs = 0.0f;
        float GPUCommandBufferMs = 0.0f; // Whole command buffer, includes waiting for the swapchain image
        float GPUDisplayWaitMs = 0.0f;   // GPU idle until the display released the swapchain image (vsync), not in GPUWorkMs or Scopes
        float LastFrameMs = 0.0f;
        float LastGPUWorkMs = 0.0f;      // Last resolved GPU frame, FRAMES_IN_FLIGHT frames old
        bool GPUTiming = false;          // The GPU numbers are valid (timestamps on, supported and resolved this window)
        bool GPUOverflowed = false;      // More scopes than the query pool holds, the last ones were dropped
        int ThermalStatus = -1;          // Platform::GetThermalStatus, polled once a second
        float CPUUsage = -1.0f;          // Percent of every CPU thread this process used over the last second, -1 until known
        Vector<DiagnosticsScope> Scopes; // Same pre-order as GPUTimings::Scopes
        Vector<DiagnosticsGroup> Groups; // Depth 1 scopes summed by name, slowest first
    };

    // What the last frame drew and held, captured at the end of each measured frame
    struct FrameCounters
    {
        Uint DrawCalls = 0;
        uint64_t Triangles = 0; // Every pass of the frame (RHIStats::Triangles)
        Uint Meshes = 0;
        Uint Lights = 0;
        Uint MaxLights = 0;
        Uint Sprites = 0;
        Uint Texts = 0;
        Uint UIItems = 0;
        Uint Particles = 0;
        Uint MaxParticles = 0;
        Uint Animators = 0;
        Uint Entities = 0; // In the running scene
        uint64_t GPUMemoryMB = 0;
        uint64_t GPUMemoryBudgetMB = 0;
        String GPUName; // Set once
        String CPUName; // Set once, without the vendor marks and clock ("Intel Core i7-10750H"), "" when unknown
        Uint CPUThreads = 1;
        Uint ScreenWidth = 0;
        Uint ScreenHeight = 0;
        Uint RenderWidth = 0; // 3D scene resolution, render scale applied
        Uint RenderHeight = 0;
        float RenderScale = 1.0f;
    };

    // The last SIZE measured frames, Sample(values, 0) is the oldest
    struct FrameHistory
    {
        static constexpr Uint SIZE = 240;
        std::array<float, SIZE> FrameMs = {};
        std::array<float, SIZE> CPUWorkMs = {};
        std::array<float, SIZE> GPUWorkMs = {};
        Uint Offset = 0; // Next write
        Uint Count = 0;

        float Sample(const std::array<float, SIZE>& values, Uint index) const { return values[((Count < SIZE ? 0 : Offset) + index) % SIZE]; }
    };

    /*
    * Every runtime measurement of the engine: frame and CPU work time, GPU scope timings (read from the RHI's timestamp queries),
    * thermal state and per frame counters; Measures only while someone reads: each GetTimings()/Request() keeps it running for a second and turns the GPU timestamps
    * on, after that EndFrame() is a single branch and the timestamps go back off (unless SetGPUTimingEnabled(true) asked for them)
    */
    class Diagnostics
    {
    public:
        static constexpr float WINDOW = 0.5f; // Seconds the timings are averaged over

        void Initialize(); // Core::Initialize, after the Renderer
        void Shutdown();
        void EndFrame(float frameMilliseconds); // Core::Run, at the end of a frame: milliseconds since it began, before the cap

        void Request();
        const FrameTimings& GetTimings(); // Request() + the last finished window
        const FrameCounters& GetCounters() const { return mCounters; }
        const FrameHistory& GetHistory() const { return mHistory; }

        // GPU timestamps: an explicit switch (the Editor's Renderer panel), wins over the automatic one of Request()
        void SetGPUTimingEnabled(bool enabled);
        bool IsGPUTimingEnabled() const;
        bool SupportsGPUTiming() const; // Compiled in (RHISettings::GPU_TIMING) and the queue has timestamps

        static const char* ThermalStatusToString(int status); // "" when unknown

    private:
        void Activate();
        void ResetWindow();
        void ResetGPUWindow();
        void AccumulateGPU();
        void CaptureCounters();
        void Publish();
        void SampleCPUUsage(); // Once a second, with the thermal state

    private:
        struct ScopeSum
        {
            float Sum = 0.0f;
            float Peak = 0.0f;
        };

        FrameTimings mTimings;
        FrameCounters mCounters;
        FrameHistory mHistory;
        Vector<ScopeSum> mScopeSums; // Parallel to mTimings.Scopes

        float mFrameSum = 0.0f;
        float mFramePeak = 0.0f;
        float mCPUWorkSum = 0.0f;
        float mCPUWorkPeak = 0.0f;
        float mGPUWorkSum = 0.0f;
        float mGPUWorkPeak = 0.0f;
        float mGPUCommandBufferSum = 0.0f;
        float mGPUDisplayWaitSum = 0.0f;
        Uint mFrameSamples = 0;
        Uint mGPUSamples = 0;
        uint64_t mLastResolvedFrame = 0;

        float mWindowTime = 0.0f;
        float mStatusTimer = 0.0f;       // Thermal state + CPU usage, once a second
        float mCPUSampleTime = 0.0f;     // Seconds since the last CPU usage sample
        double mLastProcessCPUSeconds = -1.0;
        float mRequestTime = 0.0f;       // Seconds measuring keeps running without another Request()
        bool mActive = false;
        bool mGPUTimingAllowed = true;   // SetGPUTimingEnabled(false) keeps Request() from turning it on
        bool mOwnsGPUTiming = false;     // Request() turned it on, so it turns it off again
    };

} // namespace Surge
