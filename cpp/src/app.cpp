#include <stdio.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>

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
#include <GLFW/glfw3.h> // drag system OpenGL headers

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

struct Settings {
    std::string version = "v1.0.0";
    float sampleRate = 44100.0;
    unsigned long samplesPerBuffer = 1024;
    int ringBufferCount = 64;
    int displayBufferCount = 256; 
    int computeBufferCount = 8; // must be < displayBufferCount
    int computeRingFrameCount = 1;
    int octaves = 4;
    float threshold = 0.016f;
    float maxDisplayHz = 1100;
    ImVec4 accentCol1 = ImColor::HSV(219/360., .58, .93), accentCol2 = ImColor::HSV(99/360., .58, .93), accentCol3 = ImColor::HSV(349/360., .58, .93);
};

struct GuiState { 
    std::string chordName = "C#";
    const float mainDisplayRatio = 0.8f;
    bool collapsed = false;
    int octaves = 4;
    float threshold = 0.016f;
    float plotMxs[3] = {0.2, 14.87, 17.3};
};

ImVec4 alpha(ImVec4 c, float a) {
    return ImVec4(c.x, c.y, c.z, a);
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

struct PaContext {
    PaUtilRingBuffer rBuffFromRT;
    void* rBuffFromRTData;
};

int paCallback(const void* inputBuffer, void* output, unsigned long samplesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags flags, void* userData) {
    PaContext* paCtx = (PaContext*) userData;
    PaUtil_WriteRingBuffer(&paCtx->rBuffFromRT, inputBuffer, 1);
    return paContinue;
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

void compute(Settings &settings, ComputeContext &ctx){
    int n = settings.samplesPerBuffer*settings.computeBufferCount;
    float* readData = (float*)PaUtil_AllocateZeroInitializedMemory(sizeof(float)*n*settings.computeRingFrameCount);
    ChordConfig cfg = initChordConfig(n, settings.sampleRate, settings.octaves, settings.threshold);
    auto st = std::chrono::high_resolution_clock::now(); auto end = st;
    double dt = 0;
    while(ctx.run) {
        int available = PaUtil_GetRingBufferReadAvailable(&ctx.rBuffFromGui);
        if(available > 0){
            st = std::chrono::high_resolution_clock::now();
            PaUtil_ReadRingBuffer(&ctx.rBuffFromGui, readData, available);
            float* samples = &readData[n*(available-1)];
            ChordComputeData* pt = initChordComputeData(n); 
            pt->dt = dt;

            cfg.octaves = settings.octaves; cfg.threshold = settings.threshold;
            computeChord(*pt, samples, cfg);
            PaUtil_WriteRingBuffer(&ctx.rBuffToGui, &pt, 1);
            end = std::chrono::high_resolution_clock::now();
            dt = std::chrono::duration<double, std::milli>(end-st).count(); // ms 
        }
    }

    if(readData) PaUtil_FreeMemory(readData);
    freeChordConfig(cfg);
}

int gui(int argc, char* argv[])
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
    GLFWwindow* window = glfwCreateWindow(1280, 720, ("Chordy: Real-Time Chord Detection ("+settings.version+")").c_str(), nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    ImGui::StyleColorsDark();

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
    paCtx.rBuffFromRTData = PaUtil_AllocateZeroInitializedMemory(sizeof(float) * settings.samplesPerBuffer * settings.ringBufferCount);
    if(paCtx.rBuffFromRTData == nullptr) return 1;
    PaUtil_InitializeRingBuffer(&paCtx.rBuffFromRT, sizeof(float)*settings.samplesPerBuffer, settings.ringBufferCount, paCtx.rBuffFromRTData);
    float* readData = (float*)PaUtil_AllocateZeroInitializedMemory(sizeof(float)*settings.samplesPerBuffer*settings.ringBufferCount);
    float* displayData = (float*)PaUtil_AllocateZeroInitializedMemory(sizeof(float)*settings.displayBufferCount*settings.samplesPerBuffer);
    float* tmpData = (float*)PaUtil_AllocateZeroInitializedMemory(sizeof(float)*settings.displayBufferCount*settings.samplesPerBuffer);
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
    computeCtx.rBuffFromGuiData = PaUtil_AllocateZeroInitializedMemory(sizeof(float)*settings.samplesPerBuffer*settings.computeBufferCount*settings.computeRingFrameCount);
    PaUtil_InitializeRingBuffer(&computeCtx.rBuffFromGui, sizeof(float)*settings.samplesPerBuffer*settings.computeBufferCount, settings.computeRingFrameCount, computeCtx.rBuffFromGuiData);
    computeCtx.rBuffToGuiData = PaUtil_AllocateZeroInitializedMemory(sizeof(ChordComputeData*)*settings.computeRingFrameCount);
    PaUtil_InitializeRingBuffer(&computeCtx.rBuffToGui, sizeof(ChordComputeData*), settings.computeRingFrameCount, computeCtx.rBuffToGuiData);
    std::thread computeThread(compute, std::ref(settings), std::ref(computeCtx));

    ImFont* fontSm, *fontMd, *fontLg; 
    auto execPath = std::filesystem::path(argv[0]).parent_path();
    std::string fontFile = execPath / "res/font.ttf";
    if(std::filesystem::exists(fontFile)) {
        fontSm = io.Fonts->AddFontFromFileTTF(fontFile.c_str(), 16.f);
        fontMd = io.Fonts->AddFontFromFileTTF(fontFile.c_str(), 24.f);
        fontLg = io.Fonts->AddFontFromFileTTF(fontFile.c_str(), 48.f);
    } else {
        io.Fonts->AddFontDefault();
    }
     
    // state 
    GuiState state; state.threshold = settings.threshold; state.octaves = settings.octaves;
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
            int countRight = settings.displayBufferCount*settings.samplesPerBuffer-displayWriteInd, countLeft = 0;
            if(available > countRight){
                countLeft = available - countRight; // ensure that settings.displayBufferCount >= settings.ringBufferCount
                memcpy(&tmpData[displayWriteInd], readData, sizeof(float)*countRight);
                memcpy(&tmpData[0], &readData[countRight], sizeof(float)*countLeft);
                displayWriteInd = countLeft;
            } else {
                countRight = available;
                memcpy(&tmpData[displayWriteInd], readData, sizeof(float)*countRight);
                displayWriteInd += countRight;
            }

            memcpy(displayData, &tmpData[displayWriteInd], sizeof(float)*(settings.displayBufferCount*settings.samplesPerBuffer-displayWriteInd));
            memcpy(&displayData[settings.samplesPerBuffer*settings.displayBufferCount-displayWriteInd], tmpData, sizeof(float)*displayWriteInd);
            PaUtil_WriteRingBuffer(&computeCtx.rBuffFromGui, &displayData[settings.samplesPerBuffer*(settings.displayBufferCount-settings.computeBufferCount)], 1);
        }
        
        available = PaUtil_GetRingBufferReadAvailable(&computeCtx.rBuffToGui);
        if(available > 0) {
            if(chordComputeData) freeChordComputeData(chordComputeData);
            while(available--) PaUtil_ReadRingBuffer(&computeCtx.rBuffToGui, &chordComputeData, 1);
            
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

        // Main Window
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos); 
            ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x*(state.collapsed ? 1. : state.mainDisplayRatio), viewport->WorkSize.y));
            
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

            ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

            auto winSize = ImGui::GetWindowSize();

            if(fontLg) ImGui::PushFont(fontLg);
            else ImGui::SetWindowFontScale(4.0);

            ImGui::SetCursorPosX((winSize.x - ImGui::CalcTextSize(state.chordName.c_str()).x)*0.5f);
            ImGui::SetCursorPosY((winSize.y - ImGui::CalcTextSize(state.chordName.c_str()).y)*0.25f);
            ImGui::TextColored(ImColor(100, 149, 237, 255), state.chordName.c_str());
            const float notesY = 0.25f*winSize.y +0.75f*ImGui::CalcTextSize(state.chordName.c_str()).y + 0.025f*winSize.y;

            if(fontLg) ImGui::PopFont();
            if(chordComputeData) {
                if(fontMd) ImGui::PushFont(fontMd);
                else ImGui::SetWindowFontScale(2.0);

                const float spacing = 20.f;
                float ts = 0; for(int p = 0; p < 12; p++) ts += ImGui::CalcTextSize(notes[p].c_str()).x;
                float total = ts + spacing * 11; float run = 0;

                for(int p = 0; p < 12; p++) {
                    ImGui::SetCursorPosX((winSize.x-total)*0.5f+run);
                    ImGui::SetCursorPosY(notesY);
                    ImGui::TextColored(alpha(settings.accentCol1, chordComputeData->chroma[p]*0.9+0.1), notes[p].c_str());
                    run += ImGui::CalcTextSize(notes[p].c_str()).x+spacing;
                }
                if(fontMd) ImGui::PopFont();
            }
                        
            if(fontSm) ImGui::PushFont(fontSm);
            else ImGui::SetWindowFontScale(1.0);

            ImPlotFlags plotFlags = ImPlotFlags_CanvasOnly;
            ImPlotAxisFlags plotAxisFlags = ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_NoHighlight;
            ImPlotStyle& plotStyle = ImPlot::GetStyle();
            plotStyle.Colors[ImPlotCol_FrameBg] = ImVec4(1, 1, 1, 0);
            plotStyle.Colors[ImPlotCol_PlotBg] = ImVec4(0, 1, 0, 0);
            plotStyle.Colors[ImPlotCol_PlotBorder] = ImVec4(1, 1, 1, 0);
            plotStyle.PlotPadding = ImVec2(0,5);

            float plotSpecHeight = 0.1f;
            float plotHPSHeight = 0.1f;
            float plotWaveHeight = 0.1f;
            
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
            ImGui::SetCursorPosY(winSize.y*(1-plotSpecHeight-plotWaveHeight-plotHPSHeight));
            ImPlot::SetNextAxesLimits(-settings.displayBufferCount*(long)settings.samplesPerBuffer*1.f/settings.sampleRate, 0, -state.plotMxs[0], state.plotMxs[0], ImPlotCond_Always); 
            if (ImPlot::BeginPlot("Waveform", ImVec2(-1, winSize.y*plotWaveHeight), plotFlags)) { 
                ImPlot::SetupAxes("Time", "Amplitude", plotAxisFlags, plotAxisFlags);
                ImPlot::SetNextLineStyle(settings.accentCol1);
                ImPlot::PlotLine("Audio", xDisplay, displayData, settings.displayBufferCount*settings.samplesPerBuffer);
                ImPlot::EndPlot();
            }
            if(ImGui::BeginItemTooltip()) {
                ImGui::SetTooltip("Audio Signal (Time Domain)");
                ImGui::EndTooltip();
            }
            
            if(chordComputeData) {
                ImGui::SetCursorPosY(winSize.y*(1-plotSpecHeight-plotHPSHeight));
                
                ImPlot::SetNextAxesLimits(0, settings.maxDisplayHz, 0, exp2(state.plotMxs[1]), ImPlotCond_Always); 
                if (ImPlot::BeginPlot("Spectra", ImVec2(-1, winSize.y*plotSpecHeight), plotFlags)) {
                    ImPlot::SetupAxes("Frequency", "Power", plotAxisFlags, plotAxisFlags);
                    ImPlot::SetNextLineStyle(settings.accentCol2);
                    ImPlot::PlotLine("Spectra", xSpec, chordComputeData->spec, settings.computeBufferCount*settings.samplesPerBuffer*settings.maxDisplayHz/settings.sampleRate);
                    ImPlot::EndPlot();
                }
                if(ImGui::BeginItemTooltip()) {
                    ImGui::SetTooltip("Power Spectrum (Frequency Domain)");
                    ImGui::EndTooltip();
                }
                
                ImGui::SetCursorPosY(winSize.y*(1-plotHPSHeight));
                ImPlot::SetNextAxesLimits(0, settings.maxDisplayHz, 0, exp2(state.plotMxs[2]), ImPlotCond_Always); 
                if (ImPlot::BeginPlot("HPS", ImVec2(-1, winSize.y*plotHPSHeight), plotFlags)) {
                    ImPlot::SetupAxes("Frequency", "HPS", plotAxisFlags^ImPlotAxisFlags_NoTickLabels, plotAxisFlags);
                    ImPlot::SetNextLineStyle(settings.accentCol3);
                    ImPlot::PlotLine("HPS", xSpec, chordComputeData->hps, settings.computeBufferCount*settings.samplesPerBuffer*settings.maxDisplayHz/settings.sampleRate);
                    ImPlot::EndPlot();
                }
                if(ImGui::BeginItemTooltip()) {
                    ImGui::SetTooltip("Harmonic Product Spectrum (Frequency Domain)");
                    ImGui::EndTooltip();
                }
            }
            if(fontSm) ImGui::PopFont();
            ImGui::PopStyleVar();

            ImGui::End();
            ImGui::PopStyleVar();
        }

        // Settings Window
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x+viewport->WorkSize.x*state.mainDisplayRatio, viewport->WorkPos.y)); 
            ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x*(1-state.mainDisplayRatio), viewport->WorkSize.y));

            // ImGuiStyle& style = ImGui::GetStyle();
            // style.WindowPadding = ImVec2(0, 0);

            state.collapsed = !ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);
            if(!state.collapsed) {
                auto winSize = ImGui::GetWindowSize();

                ImGui::PushTextWrapPos(winSize.x);
                ImGui::TextColored(ImVec4(1,1,1,1), ("Chordy " + settings.version + "\nA configurable, multi-threaded, real-time chord detector in C++.").c_str());
                ImGui::PopTextWrapPos();
                ImGui::TextLinkOpenURL("GitHub", "https://github.com/arulandu/chordy");
                ImGui::SameLine();
                ImGui::TextLinkOpenURL("Website", "https://arulandu.com");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1,1,1,1), "© Alvan Arulandu");
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                ImGui::TextColored(ImVec4(1, 1, 1, 1), "Main:    %.2f fps", io.Framerate);
                if(chordComputeData){ 
                    ImGui::TextColored(ImVec4(1, 1, 1, 1), "Compute: %.2f ms/job", chordComputeData->dt);
                }
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1, 1, 1, .1));                
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1, 1, 1, .2));                
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1, 1, 1, .3));                
                ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1, 1, 1, .4));                
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1, 1, 1, .6));                
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "Octaves");
                if(ImGui::BeginItemTooltip()) {
                    ImGui::SetTooltip("octaves for HPS/chroma calculation.");
                    ImGui::EndTooltip();
                }
                if(ImGui::SliderInt("##1", &state.octaves, 1, 6, "%d", ImGuiSliderFlags_NoInput|ImGuiSliderFlags_AlwaysClamp)){
                    settings.octaves = state.octaves;
                }
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(1, 1, 1, 1), "Threshold");
                if(ImGui::BeginItemTooltip()) {
                    ImGui::SetTooltip("N/A threshold for chord/avg ratio.");
                    ImGui::EndTooltip();
                }
                if(ImGui::SliderFloat("##2", &state.threshold, 0.001, 0.025, "%.3f", ImGuiSliderFlags_NoInput|ImGuiSliderFlags_AlwaysClamp)){
                    settings.threshold = state.threshold;
                }
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(1, 1, 1, 1), "Display Maximum");
                if(ImGui::BeginItemTooltip()) {
                    ImGui::SetTooltip("Maximum for the y-axis of each plot.");
                    ImGui::EndTooltip();
                }
                ImGui::PushStyleColor(ImGuiCol_SliderGrab, alpha(settings.accentCol1, .4));                
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, alpha(settings.accentCol1, .6));                
                ImGui::PushStyleColor(ImGuiCol_Text, settings.accentCol1);                
                ImGui::SliderFloat("Waveform", &state.plotMxs[0], 0, 2, "%.3f", ImGuiSliderFlags_AlwaysClamp);

                ImGui::PushStyleColor(ImGuiCol_SliderGrab, alpha(settings.accentCol2, .4));                
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, alpha(settings.accentCol2, .6));                
                ImGui::PushStyleColor(ImGuiCol_Text, settings.accentCol2);                
                ImGui::SliderFloat("log(power)", &state.plotMxs[1], 0, 20, "%.3f", ImGuiSliderFlags_AlwaysClamp);

                ImGui::PushStyleColor(ImGuiCol_SliderGrab, alpha(settings.accentCol3, .4));                
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, alpha(settings.accentCol3, .6));                
                ImGui::PushStyleColor(ImGuiCol_Text, settings.accentCol3);                
                ImGui::SliderFloat("log2(hps)", &state.plotMxs[2], 0, 51, "%.3f", ImGuiSliderFlags_AlwaysClamp);

                ImGui::PopStyleColor(14);
            }
            ImGui::End();
        }
        
        // if(fontSm) ImGui::PopFont();
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
