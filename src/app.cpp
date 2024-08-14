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
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
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
     
};

void compute(ComputeContext data){

}

int gui()
{
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
    
    const float sampleRate = 44100.0;
    const unsigned long samplesPerBuffer = 1024;
    const int ringBufferCount = 64;
    const int displaySamples = 1024*256;
    const int displayBufferCount = (displaySamples/samplesPerBuffer) + (displaySamples % samplesPerBuffer == 0 ? 0 : 1);

    PaContext paCtx;
    paCtx.rBuffFromRTData = PaUtil_AllocateZeroInitializedMemory(sizeof(float32_t) * samplesPerBuffer * ringBufferCount);
    if(paCtx.rBuffFromRTData == nullptr) return 1;
    PaUtil_InitializeRingBuffer(&paCtx.rBuffFromRT, sizeof(float32_t)*samplesPerBuffer, ringBufferCount, paCtx.rBuffFromRTData);
    float* readData = (float*)PaUtil_AllocateZeroInitializedMemory(sizeof(float32_t)*samplesPerBuffer*ringBufferCount);
    float* displayData = (float*)PaUtil_AllocateZeroInitializedMemory(sizeof(float32_t)*displayBufferCount*samplesPerBuffer);
    float* tmpData = (float*)PaUtil_AllocateZeroInitializedMemory(sizeof(float32_t)*displayBufferCount*samplesPerBuffer);
    int displayWriteInd = 0;
    
    float xDisplay[samplesPerBuffer*displayBufferCount], xRead[samplesPerBuffer*ringBufferCount];
    for(int i = 0; i < samplesPerBuffer*displayBufferCount; i++){
        xDisplay[i] = i;
        if(i < samplesPerBuffer*ringBufferCount) xRead[i] = i;
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
    paErr = Pa_OpenStream(&stream, &inputParams, nullptr, sampleRate, samplesPerBuffer, paFlags, paCallback, &paCtx);
    if(paErr != paNoError) return pa_error_handler(paErr);
    paErr = Pa_StartStream(stream);
    if(paErr != paNoError) return pa_error_handler(paErr);

    ComputeContext computeContext;

    // initialize compute thread
    std::thread computeThread([&computeContext]() {

    });

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
        available *= samplesPerBuffer;

        if(available > 0){
            // std::cout << "Read " << available << " Ind: " << displayWriteInd;
            int countRight = displayBufferCount*samplesPerBuffer-displayWriteInd, countLeft = 0;
            if(available > countRight){
                countLeft = available - countRight; // ensure that displayBufferCount >= ringBufferCount
                memcpy(&tmpData[displayWriteInd], readData, sizeof(float32_t)*countRight);
                memcpy(&tmpData[0], &readData[countRight], sizeof(float32_t)*countLeft);
                displayWriteInd = countLeft;
            } else {
                countRight = available;
                memcpy(&tmpData[displayWriteInd], readData, sizeof(float32_t)*countRight);
                displayWriteInd += countRight;
            }

            memcpy(displayData, &tmpData[displayWriteInd], sizeof(float32_t)*(displayBufferCount*samplesPerBuffer-displayWriteInd));
            memcpy(&displayData[samplesPerBuffer*displayBufferCount-displayWriteInd], tmpData, sizeof(float32_t)*displayWriteInd);
            // std::cout << " FInd: " << displayWriteInd << '\n';
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

            ImGui::SetWindowFontScale(4.0);
            ImGui::SetCursorPosX((winSize.x - ImGui::CalcTextSize(state.chordName.c_str()).x)*0.5f);
            ImGui::SetCursorPosY((winSize.y - ImGui::CalcTextSize(state.chordName.c_str()).y)*0.25f);
            ImGui::TextColored(ImVec4(1, 1, 1, 1), state.chordName.c_str());
            // ImGui::TextColored(ImVec4(1, 1, 1, 1), std::to_string(displayWriteInd).c_str());
            // ImGui::TextColored(ImVec4(1, 1, 1, 1), std::to_string(available/samplesPerBuffer).c_str());
            ImGui::SetWindowFontScale(1.0);
            ImGui::TextColored(ImVec4(1, 1, 1, 1), std::to_string(io.Framerate).c_str());

            ImPlotFlags plotFlags = ImPlotFlags_CanvasOnly;
            ImPlotAxisFlags plotAxisFlags = ImPlotAxisFlags_NoDecorations;
            // plotAxisFlags ^= ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickLabels;
            ImPlotStyle& plotStyle = ImPlot::GetStyle();
            plotStyle.Colors[ImPlotCol_FrameBg] = ImVec4(1, 1, 1, 0.02);
            plotStyle.Colors[ImPlotCol_PlotBg] = ImVec4(0, 1, 0, 0);
            plotStyle.Colors[ImPlotCol_PlotBorder] = ImVec4(1, 1, 1, 0);
            plotStyle.PlotPadding = ImVec2(0,5);

            float plotHeightPercent = 0.5f;
            ImGui::SetCursorPosY(winSize.y*(1-plotHeightPercent));
            if (ImPlot::BeginPlot("Waveform", ImVec2(-1, winSize.y*plotHeightPercent), plotFlags)) {
                ImPlot::SetupAxes("Time", "Amplitude", plotAxisFlags, plotAxisFlags);
                // ImPlot::SetupAxesLimits(0, 3, -0.12, 0.12);
                ImPlot::SetupAxisLimits(ImAxis_Y1, -0.2, 0.2);
                
                // TODO: get 
                ImPlot::PlotLine("My Line Plot", xDisplay, displayData, displayBufferCount*samplesPerBuffer);
                // ImPlot::PlotLine("My Line Plot", xRead, readData, ringBufferCount*samplesPerBuffer);

                ImPlot::EndPlot();
            }
            
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(state.clear_color.x * state.clear_color.w, state.clear_color.y * state.clear_color.w, state.clear_color.z * state.clear_color.w, state.clear_color.w);
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

    paErr = Pa_Terminate();
    if(paErr != paNoError){
        printf("PortAudio termination error: %s\n", Pa_GetErrorText(paErr));
    }

    return 0;
}
