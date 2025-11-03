//
// Created by Vitaly Lysen on 03/11/2025.
//

#ifndef ECOSYSTEM_APP_H
#define ECOSYSTEM_APP_H
#include <unordered_map>
#include <memory>

#include "gl_utils.h"
#include "simulation.h"
#include "GLFW/glfw3.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

struct AppState {
    GLFWwindow *window = nullptr;
    ImGuiIO *io = nullptr;

    CenteredQuad quad{};
    GLuint gridTex = 0;

    std::unique_ptr<ISimulation> sim;
    std::unordered_map<int, glm::ivec3> speciesColors;
    SimulationSettings currentConfig{};
    SimulationSettings pendingConfig{};
    bool configDirty = false;

    int prevGridWidth = 0, prevGridHeight = 0;
    int stepsPerSecond = 10;
    bool pauseSim = false;
};

class App {
    AppState appState;

    constexpr static auto clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    constexpr static auto errorColor = glm::ivec3(255, 0, 255);

public:
    explicit App(
        SimulationSettings settings,
        std::unordered_map<int, glm::ivec3> speciesColors = {}
    );

    ~App();

    void run();

private:
    void destroyResources();

    glm::ivec3 getSpeciesColor(const int speciesId) const {
        const auto it = appState.speciesColors.find(speciesId);
        if (it != appState.speciesColors.end()) {
            const glm::ivec3 &col = it->second;
            return col;
        }

        return errorColor;
    }

    void renderUI();

    void applyPendingConfig(bool forceRecreate = false);

    void drawSimulationToTexture() const;
};

#endif //ECOSYSTEM_APP_H
