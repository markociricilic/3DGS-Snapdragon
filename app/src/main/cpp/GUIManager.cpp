#include "GUIManager.h"

#include <iostream>

#include "imgui.h"
#include "implot.h"

struct ScrollingBuffer {
    int maxSize;
    int offset;
    ImVector<ImVec2> data;
    explicit ScrollingBuffer(const int max_size = 10000) {
        maxSize = max_size;
        offset  = 0;
        data.reserve(maxSize);
    }
    void addPoint(float x, float y) {
        if (data.size() < maxSize)
            data.push_back(ImVec2(x,y));
        else {
            data[offset] = ImVec2(x,y);
            offset =  (offset + 1) % maxSize;
        }
    }
    void clear() {
        if (data.size() > 0) {
            data.shrink(0);
            offset  = 0;
        }
    }
};

static std::shared_ptr<std::unordered_map<std::string, ScrollingBuffer>> metricsMap;
static std::shared_ptr<std::unordered_map<std::string, float>> textMetricsMap;

GUIManager::GUIManager() {
    metricsMap = std::make_shared<std::unordered_map<std::string, ScrollingBuffer>>();
    textMetricsMap = std::make_shared<std::unordered_map<std::string, float>>();
}

void GUIManager::init() {
    ImPlot::CreateContext();
}

void GUIManager::buildGui() {
    if (mouseCapture) {
        ImGui::BeginDisabled(true);
    }

    if(showMetrics) {
        ImGui::SetNextWindowSize(ImVec2(500, 250), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("Performance");

        static float history = 10;

        static ImPlotAxisFlags flags = ImPlotAxisFlags_AutoFit;
        // Make sure there's enough room for the legend on the right
        ImPlot::GetStyle().LegendInnerPadding = ImVec2(6, 6);
        ImPlot::GetStyle().LegendSpacing = ImVec2(5, 2);

        // Ensure legend always appears outside the plot area
        ImPlotFlags plotFlags = ImPlotFlags_NoBoxSelect;

        // Use fixed size for plot to ensure the legend has room
        if (ImPlot::BeginPlot("##Scrolling", ImVec2(-1, -1), plotFlags)) {
            // Force legend to right side of plot area with no background
            ImPlot::SetupLegend(ImPlotLocation_East,
                                ImPlotLegendFlags_Outside |
                                ImPlotLegendFlags_NoButtons |
                                ImPlotLegendFlags_NoHighlightItem);

            // Make more space for legend
            ImPlot::GetStyle().PlotPadding = ImVec2(10, 10);
            ImPlot::GetStyle().LegendPadding = ImVec2(10, 10);

            ImPlot::SetupAxes("s", "time (ms)", flags, flags);
            const auto t = ImGui::GetTime();
            ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
            // Increase line thickness for better visibility
            ImPlot::GetStyle().LineWeight = 3.0f;

            // Apply fill style
            ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.5f);

            for (auto &[name, values]: *metricsMap) {
                if (!values.data.empty()) {
                    // Make lines thicker for individual plots and set line style
                    ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 3.0f);

                    ImPlot::PlotLine(name.c_str(), &values.data[0].x, &values.data[0].y,
                                     values.data.size(), 0,
                                     values.offset, 2 * sizeof(float));
                }
            }
            ImPlot::EndPlot();
        }
        ImGui::SliderFloat("History", &history, 1, 30, "%.1f s");
        ImGui::End();

        // always auto resize

        bool popen = true;
        ImGui::SetNextWindowPos(ImVec2(10, 270), ImGuiCond_FirstUseEver);
        ImGui::Begin("Metrics", &popen, ImGuiWindowFlags_AlwaysAutoResize);
        for (auto &[name, value]: *textMetricsMap) {
            ImGui::Text("%s: %.2f", name.c_str(), value);
        }
        for (auto &[name, values]: *metricsMap) {
            ImGui::Text("%s: %.2f", name.c_str(), values.data.empty() ? 0 : values.data.back().y);
        }
        ImGui::End();
    }

    float screenWidth = ImGui::GetIO().DisplaySize.x;
    float screenHeight = ImGui::GetIO().DisplaySize.y;

    // same as main.cpp...
    // TODO: find a way to only define this once
    float buttonWidth = 300;
    float buttonHeight = 70;
    // top left corner of resolution button
    float resolution_button_x = (screenWidth - buttonWidth) / 4;
    float resolution_button_y = screenHeight - 80;

    ImGui::SetNextWindowPos(ImVec2(resolution_button_x, resolution_button_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(buttonWidth, buttonHeight), ImGuiCond_Always);
    ImGui::Begin("Resolution", nullptr, 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoCollapse);
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
    
    // Show current resolution status
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "  PRESS TO TOGGLE");
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.0f, 1.0f), " Resolution: %s", useHalfResolution ? "HALF" : "FULL");

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::End();

    // start of new code

    // top left corner of gui button
    float gui_button_x = 3*(screenWidth - buttonWidth) / 4;
    float gui_button_y = screenHeight - 80;

    ImGui::SetNextWindowPos(ImVec2(gui_button_x, gui_button_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(buttonWidth, buttonHeight), ImGuiCond_Always);

    // Set the font scale before creating the window.
    ImGui::Begin("GUI Toggle", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse);

    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "  PRESS TO SWITCH");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "       Scene");

    // end of new code

    ImGui::End();

    if (mouseCapture) {
        ImGui::EndDisabled();
    }
}

void GUIManager::pushTextMetric(const std::string& name, float value) {
    if (textMetricsMap->find(name) == textMetricsMap->end()) {
        textMetricsMap->insert({name, value});
    } else {
        textMetricsMap->at(name) = value;
    }
}

void GUIManager::pushMetric(const std::string& name, float value) {
    int maxSize = 600;
    if (metricsMap->find(name) == metricsMap->end()) {
        metricsMap->insert({name, ScrollingBuffer{}});
    }
    metricsMap->at(name).addPoint(ImGui::GetTime(), value);
}

void GUIManager::pushMetric(const std::unordered_map<std::string, float>& name) {
    for (auto& [n, v]: name) {
        pushMetric(n, v);
    }
}

bool GUIManager::wantCaptureMouse() {
    return ImGui::GetIO().WantCaptureMouse;
}

bool GUIManager::wantCaptureKeyboard() {
    return ImGui::GetIO().WantCaptureKeyboard;
}
