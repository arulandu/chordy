#include <stdio.h>
#include <iostream>
#include <chrono>
#include <thread>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "implot.h"

#include "portaudio.h"
#include "pa_ringbuffer.h"
#include "pa_util.h"

#include "chord.h"

#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include "Eigen/Dense"

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif


struct GuiState { 
    std::string chordName = "C#";
};

struct PaContext {
    PaUtilRingBuffer rBuffFromRT;
    void* rBuffFromRTData;
};

int paCallback(const void* inputBuffer, void* output, unsigned long samplesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags flags, void* userData) {
    PaContext* paCtx = (PaContext*) userData;
    // printf("callback %f\n", timeInfo->currentTime);

    PaUtil_WriteRingBuffer(&paCtx->rBuffFromRT, inputBuffer, 1);
    // PaUtil_Read
    return paContinue;
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static int pa_error_handler(PaError err){
    fprintf(stderr, "PA Error %s\n", Pa_GetErrorText(err));
    return 1;
}

struct ComputeContext {
    bool run = true;     

    PaUtilRingBuffer rBuffFromGui;
    void* rBuffFromGuiData;

    PaUtilRingBuffer rBuffToGui;
    void* rBuffToGuiData;
};

struct Settings {
    float sampleRate = 44100.0;
    unsigned long samplesPerBuffer = 1024;
    int ringBufferCount = 64;
    int displaySamples = 1024*256;
    int displayBufferCount = (displaySamples/samplesPerBuffer) + (displaySamples % samplesPerBuffer == 0 ? 0 : 1);
    int computeBufferCount = 8; // must be < displayBufferCount
    int computeRingFrameCount = 2; // choose small, even 1 tbh
    int harmonics = 4;
};

void compute(Settings &settings, ComputeContext &ctx){
    int n = settings.samplesPerBuffer*settings.computeBufferCount;
    float* readData = (float*)PaUtil_AllocateZeroInitializedMemory(sizeof(float32_t)*n*settings.computeRingFrameCount);
    ChordConfig cfg = initChordConfig(n, settings.sampleRate, settings.harmonics);
    auto st = std::chrono::high_resolution_clock::now();
    auto end = st;
    double dt = 0;
    while(ctx.run) {
        int available = PaUtil_GetRingBufferReadAvailable(&ctx.rBuffFromGui);
        if(available > 0){
            PaUtil_ReadRingBuffer(&ctx.rBuffFromGui, readData, available);
            float* samples = &readData[n*(available-1)];
            ChordComputeData* pt = initChordComputeData(n); 
            pt->dt = dt;
            computeChord(*pt, samples, cfg);
            PaUtil_WriteRingBuffer(&ctx.rBuffToGui, &pt, 1);
            end = std::chrono::high_resolution_clock::now();
            dt = std::chrono::duration<double, std::milli>(end-st).count(); // ms 
            st = end;
        }
    }

    if(readData) PaUtil_FreeMemory(readData);
    freeChordConfig(cfg);
}

int gui()
{
    Settings settings;

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Chordy: Real-Time Chord Detection", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImPlot::CreateContext();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialize Port Audio Input Stream
    PaError paErr;
    paErr = Pa_Initialize();
    if(paErr != paNoError) return pa_error_handler(paErr);

    PaContext paCtx;
    paCtx.rBuffFromRTData = PaUtil_AllocateZeroInitializedMemory(sizeof(float32_t) * settings.samplesPerBuffer * settings.ringBufferCount);
    if(paCtx.rBuffFromRTData == nullptr) return 1;
    PaUtil_InitializeRingBuffer(&paCtx.rBuffFromRT, sizeof(float32_t)*settings.samplesPerBuffer, settings.ringBufferCount, paCtx.rBuffFromRTData);
    float* readData = (float*)PaUtil_AllocateZeroInitializedMemory(sizeof(float32_t)*settings.samplesPerBuffer*settings.ringBufferCount);
    float* displayData = (float*)PaUtil_AllocateZeroInitializedMemory(sizeof(float32_t)*settings.displayBufferCount*settings.samplesPerBuffer);
    float* tmpData = (float*)PaUtil_AllocateZeroInitializedMemory(sizeof(float32_t)*settings.displayBufferCount*settings.samplesPerBuffer);
    int displayWriteInd = 0;
    float xDisplay[settings.samplesPerBuffer*settings.displayBufferCount], xRead[settings.samplesPerBuffer*settings.ringBufferCount], xSpec[settings.samplesPerBuffer*settings.computeBufferCount];
    for(int i = 0; i < settings.samplesPerBuffer*settings.displayBufferCount; i++){
        xDisplay[i] = (i-settings.displayBufferCount*(long)settings.samplesPerBuffer)*1.f/settings.sampleRate;
        if(i < settings.samplesPerBuffer*settings.ringBufferCount) xRead[i] = xDisplay[i];
        if(i < settings.samplesPerBuffer*settings.computeBufferCount) xSpec[i] = i*settings.sampleRate*1.f/(settings.samplesPerBuffer*settings.computeBufferCount*1.f);
    }

    PaStreamParameters inputParams;
    inputParams.device = Pa_GetDefaultInputDevice();
    if(inputParams.device == paNoDevice) return pa_error_handler(PaErrorCode::paDeviceUnavailable);
    inputParams.channelCount = 1; // record in mono
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency = Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    PaStream* stream;    
    const PaStreamFlags paFlags = paDitherOff;
    paErr = Pa_OpenStream(&stream, &inputParams, nullptr, settings.sampleRate, settings.samplesPerBuffer, paFlags, paCallback, &paCtx);
    if(paErr != paNoError) return pa_error_handler(paErr);
    paErr = Pa_StartStream(stream);
    if(paErr != paNoError) return pa_error_handler(paErr);

    // initialize compute thread
    ComputeContext computeCtx;
    ChordComputeData* chordComputeData;
    computeCtx.rBuffFromGuiData = PaUtil_AllocateZeroInitializedMemory(sizeof(float32_t)*settings.samplesPerBuffer*settings.computeBufferCount*settings.computeRingFrameCount);
    PaUtil_InitializeRingBuffer(&computeCtx.rBuffFromGui, sizeof(float32_t)*settings.samplesPerBuffer*settings.computeBufferCount, settings.computeRingFrameCount, computeCtx.rBuffFromGuiData);
    computeCtx.rBuffToGuiData = PaUtil_AllocateZeroInitializedMemory(sizeof(ChordComputeData*)*settings.computeRingFrameCount);
    PaUtil_InitializeRingBuffer(&computeCtx.rBuffToGui, sizeof(ChordComputeData*), settings.computeRingFrameCount, computeCtx.rBuffToGuiData);
    std::thread computeThread(compute, std::ref(settings), std::ref(computeCtx));

    // TODO: load fonts (see imgui docs)
    // state 
    GuiState state;
    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        paErr = Pa_IsStreamActive(stream); // 1 if active
        if(paErr != paNoError && paErr != 1) {
            fprintf(stderr, "Gui window running, but stream is not active.");
            break;
        }
        
        auto available = PaUtil_GetRingBufferReadAvailable(&paCtx.rBuffFromRT);
        PaUtil_ReadRingBuffer(&paCtx.rBuffFromRT, (void*)readData, available);
        available *= settings.samplesPerBuffer;

        if(available > 0){
            // std::cout << "Read " << available << " Ind: " << displayWriteInd;
            int countRight = settings.displayBufferCount*settings.samplesPerBuffer-displayWriteInd, countLeft = 0;
            if(available > countRight){
                countLeft = available - countRight; // ensure that settings.displayBufferCount >= settings.ringBufferCount
                memcpy(&tmpData[displayWriteInd], readData, sizeof(float32_t)*countRight);
                memcpy(&tmpData[0], &readData[countRight], sizeof(float32_t)*countLeft);
                displayWriteInd = countLeft;
            } else {
                countRight = available;
                memcpy(&tmpData[displayWriteInd], readData, sizeof(float32_t)*countRight);
                displayWriteInd += countRight;
            }

            memcpy(displayData, &tmpData[displayWriteInd], sizeof(float32_t)*(settings.displayBufferCount*settings.samplesPerBuffer-displayWriteInd));
            memcpy(&displayData[settings.samplesPerBuffer*settings.displayBufferCount-displayWriteInd], tmpData, sizeof(float32_t)*displayWriteInd);
            // std::cout << " FInd: " << displayWriteInd << '\n';
            PaUtil_WriteRingBuffer(&computeCtx.rBuffFromGui, &displayData[settings.samplesPerBuffer*(settings.displayBufferCount-settings.computeBufferCount)], 1);
        }
        
        available = PaUtil_GetRingBufferReadAvailable(&computeCtx.rBuffToGui);
        if(available > 0) {
            if(chordComputeData) freeChordComputeData(chordComputeData);
            while(available--) PaUtil_ReadRingBuffer(&computeCtx.rBuffToGui, &chordComputeData, 1); // TODO: Optimize
            state.chordName = chordComputeData->name;
            float sm = 0; for(int p = 0; p < 12; p++) sm += chordComputeData->chroma[p];
            for(int p = 0; p < 12; p++) chordComputeData->chroma[p] /= sm; // convert to relative chroma
        }

        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos); 
            ImGui::SetNextWindowSize(viewport->WorkSize);
            
            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowPadding = ImVec2(0, 0);

            ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

            auto winSize = ImGui::GetWindowSize();

            ImGui::TextColored(ImVec4(1, 1, 1, 1), ("Gui:     "+std::to_string(io.Framerate) + " fps").c_str());
            if(chordComputeData) ImGui::TextColored(ImVec4(1, 1, 1, 1), ("Compute: " + std::to_string(1000./chordComputeData->dt)+ " fps").c_str());

            ImGui::SetWindowFontScale(4.0);
            ImGui::SetCursorPosX((winSize.x - ImGui::CalcTextSize(state.chordName.c_str()).x)*0.5f);
            ImGui::SetCursorPosY((winSize.y - ImGui::CalcTextSize(state.chordName.c_str()).y)*0.25f);
            ImGui::TextColored(ImColor(100, 149, 237, 255), state.chordName.c_str());
            const float notesY = 0.25f*winSize.y +0.75f*ImGui::CalcTextSize(state.chordName.c_str()).y + 0.025f*winSize.y;
            
            ImGui::SetWindowFontScale(2.0);
            if(chordComputeData) {
                const float spacing = 20.f;
                float ts = 0; for(int p = 0; p < 12; p++) ts += ImGui::CalcTextSize(notes[p].c_str()).x;
                float total = ts + spacing * 11; float run = 0;

                for(int p = 0; p < 12; p++) {
                    ImGui::SetCursorPosX((winSize.x-total)*0.5f+run);
                    ImGui::SetCursorPosY(notesY);
                    ImGui::TextColored(ImVec4(100/255., 149/255., 237/255., chordComputeData->chroma[p]), notes[p].c_str());
                    run += ImGui::CalcTextSize(notes[p].c_str()).x+spacing;
                }
            }

            ImGui::SetWindowFontScale(1.0);

            ImPlotFlags plotFlags = ImPlotFlags_CanvasOnly;
            ImPlotAxisFlags plotAxisFlags = ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_Lock;
            // plotAxisFlags ^= ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickLabels;
            ImPlotStyle& plotStyle = ImPlot::GetStyle();
            plotStyle.Colors[ImPlotCol_FrameBg] = ImVec4(1, 1, 1, 0);
            plotStyle.Colors[ImPlotCol_PlotBg] = ImVec4(0, 1, 0, 0);
            plotStyle.Colors[ImPlotCol_PlotBorder] = ImVec4(1, 1, 1, 0);
            plotStyle.PlotPadding = ImVec2(0,5);

            float plotSpecHeight = 0.1f;
            float plotHPSHeight = 0.1f;
            float plotWaveHeight = 0.1f;
            
            ImGui::SetCursorPosY(winSize.y*(1-plotSpecHeight-plotWaveHeight-plotHPSHeight));
            if (ImPlot::BeginPlot("Waveform", ImVec2(-1, winSize.y*plotWaveHeight), plotFlags)) {
                ImPlot::SetupAxesLimits(-settings.displayBufferCount*(long)settings.samplesPerBuffer*1.f/settings.sampleRate, 0, -0.2, 0.2); 
                ImPlot::SetupAxes("Time", "Amplitude", plotAxisFlags, plotAxisFlags);
                ImPlot::SetNextLineStyle(ImColor(100, 149, 237, 255));
                ImPlot::PlotLine("Audio", xDisplay, displayData, settings.displayBufferCount*settings.samplesPerBuffer);
                ImPlot::EndPlot();
            }
            
            if(chordComputeData) {
                // plot first 1000hz of spectra
                const float maxDisplayHz = 1100;
                ImGui::SetCursorPosY(winSize.y*(1-plotSpecHeight-plotHPSHeight));
                if (ImPlot::BeginPlot("Spectra", ImVec2(-1, winSize.y*plotSpecHeight), plotFlags)) {
                    ImPlot::SetupAxesLimits(0, maxDisplayHz, 0, 30000); 
                    ImPlot::SetupAxes("Frequency", "Power", plotAxisFlags, plotAxisFlags);
                    ImPlot::SetNextLineStyle(ImColor(100, 237, 118, 255));
                    ImPlot::PlotLine("Spectra", xSpec, chordComputeData->spec, settings.computeBufferCount*settings.samplesPerBuffer*maxDisplayHz/settings.sampleRate);
                    ImPlot::EndPlot();
                }

                ImGui::SetCursorPosY(winSize.y*(1-plotHPSHeight));
                if (ImPlot::BeginPlot("HPS", ImVec2(-1, winSize.y*plotHPSHeight), plotFlags)) {
                    ImPlot::SetupAxesLimits(0, maxDisplayHz, 0, pow(400, settings.harmonics/2)); 
                    ImPlot::SetupAxes("Frequency", "HPS", plotAxisFlags^ImPlotAxisFlags_NoTickLabels, plotAxisFlags);
                    ImPlot::SetNextLineStyle(ImColor(237, 109, 100, 255));
                    ImPlot::PlotLine("HPS", xSpec, chordComputeData->hps, settings.computeBufferCount*settings.samplesPerBuffer*maxDisplayHz/settings.sampleRate);
                    ImPlot::EndPlot();
                }
            }

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        // glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    // dealloc vertex arrays/buffers
    glfwDestroyWindow(window);
    glfwTerminate();

    paErr = Pa_CloseStream(stream);
    if(paErr != paNoError) return pa_error_handler(paErr);

    if(paCtx.rBuffFromRTData) PaUtil_FreeMemory(paCtx.rBuffFromRTData);
    if(readData) PaUtil_FreeMemory(readData);
    if(displayData) PaUtil_FreeMemory(displayData);
    if(tmpData) PaUtil_FreeMemory(tmpData);

    paErr = Pa_Terminate();
    if(paErr != paNoError){
        printf("PortAudio termination error: %s\n", Pa_GetErrorText(paErr));
    }

    computeCtx.run = false;
    if(chordComputeData) freeChordComputeData(chordComputeData);
    if(computeCtx.rBuffFromGuiData) PaUtil_FreeMemory(computeCtx.rBuffFromGuiData);
    if(computeCtx.rBuffToGuiData) PaUtil_FreeMemory(computeCtx.rBuffToGuiData);
    computeThread.join();

    return 0;
}