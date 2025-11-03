#include "app.h"

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

App::App(
    SimulationSettings settings,
    std::unordered_map<int, glm::ivec3> speciesColors
) {
    initGLFW();
    appState.window = createWindow();
    initGlad();
    appState.io = &initImGui(appState.window);

    appState.currentConfig = settings;
    appState.pendingConfig = settings;

    appState.speciesColors = std::move(speciesColors);

    appState.quad = createCenteredQuadResources();

    applyPendingConfig();
}

App::~App() {
    destroyResources();
}

void App::destroyResources() {
    destroyImGui();
    destroyCenteredQuadResources(appState.quad);
    shutdownGLFW(appState.window);
}

void App::applyPendingConfig(const bool forceRecreate) {
    const auto &dirtyConfig = appState.pendingConfig;

    const bool gridSizeChanged =
            dirtyConfig.gridWidth != appState.prevGridWidth
            || dirtyConfig.gridHeight != appState.prevGridHeight;
    if (gridSizeChanged || forceRecreate) {
        appState.prevGridWidth = dirtyConfig.gridWidth;
        appState.prevGridHeight = dirtyConfig.gridHeight;

        glDeleteTextures(1, &appState.gridTex);
        appState.gridTex = 0;

        appState.gridTex = createGridTexture(dirtyConfig.gridWidth, dirtyConfig.gridHeight);
        const std::vector gridColors(dirtyConfig.gridWidth * dirtyConfig.gridHeight, packRGBA(0, 0, 0, 255));
        updateGridTexture(appState.gridTex, dirtyConfig.gridWidth, dirtyConfig.gridHeight, gridColors);

        appState.sim = createEcosystemSimulation(dirtyConfig);
    } else {
        appState.sim = createEcosystemSimulation(dirtyConfig);
    }

    appState.configDirty = false;
}

void App::drawSimulationToTexture() const {
    auto [
        grid,
        entities
    ] = appState.sim->getGridData();

    glBindTexture(GL_TEXTURE_2D, appState.gridTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (int y = 0; y < appState.currentConfig.gridHeight; ++y) {
        for (int x = 0; x < appState.currentConfig.gridWidth; ++x) {
            const int cellIdx = y * appState.currentConfig.gridWidth + x;
            const GridCell &cell = grid[cellIdx];

            uint8_t r = 0, g = 0, b = 0;

            constexpr uint8_t a = 255;

            if (cell.plantIndex >= 0) {
                const auto color = getSpeciesColor(SPECIES_GRASS);
                r = color.r;
                g = color.g;
                b = color.b;
            }
            if (cell.animalIndex >= 0) {
                const auto animal = entities[cell.animalIndex];
                const auto col = getSpeciesColor(animal.speciesId);

                r = std::clamp(r + col.r, 0, 255);
                g = std::clamp(g + col.r, 0, 255);
                b = std::clamp(b + col.r, 0, 255);
            }

            const uint8_t px[4] = {r, g, b, a};
            glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void App::renderUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("Simulation Settings");

        ImGui::SliderInt("Steps / s", &appState.stepsPerSecond, 0, 240);
        ImGui::Checkbox("Pause", &appState.pauseSim);
        ImGui::Text("Current step %d", appState.sim->iteration);
        ImGui::End();
    }

    {
        ImGui::Begin("World Settings");

        if (ImGui::InputInt("Width", &appState.pendingConfig.gridWidth)) appState.configDirty = true;
        if (ImGui::InputInt("Height", &appState.pendingConfig.gridHeight)) appState.configDirty = true;
        appState.pendingConfig.gridWidth = std::clamp(appState.pendingConfig.gridWidth, 10, 320);
        appState.pendingConfig.gridHeight = std::clamp(appState.pendingConfig.gridHeight, 10, 320);

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Entities Counts")) {
            if (ImGui::InputInt("Grass", &appState.pendingConfig.grassCount)) appState.configDirty = true;
            if (ImGui::InputInt("Rabbit", &appState.pendingConfig.rabbitCount)) appState.configDirty = true;
            if (ImGui::InputInt("Wolf", &appState.pendingConfig.wolfCount)) appState.configDirty = true;
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Grass Traits")) {
            ImGui::PushID("Grass");
            ImGui::DragFloat("maxEnergy", &appState.pendingConfig.grassTraits.maxEnergy, 1.0f, 0.0f, 10000.0f);
            ImGui::DragInt("reproductionCooldown", &appState.pendingConfig.grassTraits.reproductionCooldown, 1, 0,
                           10000);
            ImGui::DragFloat("reproductionChance", &appState.pendingConfig.grassTraits.reproductionChance, 0.01f,
                             0.0f,
                             1.0f);
            ImGui::DragFloat("spontaneousReproductionChance",
                             &appState.pendingConfig.grassTraits.spontaneousReproductionChance,
                             0.0001f, 0.0f, 0.1f);
            ImGui::DragFloat("hungerDamage", &appState.pendingConfig.grassTraits.hungerDamage, 0.1f, 0.0f, 1000.0f);
            ImGui::DragInt("maxAge", &appState.pendingConfig.grassTraits.maxAge, 1, 0, 100000);
            appState.configDirty = appState.configDirty || ImGui::IsItemEdited();
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Rabbit Traits")) {
            ImGui::PushID("Rabbit Traits");
            ImGui::DragFloat("maxEnergy", &appState.pendingConfig.rabbitTraits.maxEnergy, 1.0f, 0.0f, 10000.0f);
            ImGui::DragFloat("movementEnergyCost", &appState.pendingConfig.rabbitTraits.movementEnergyCost, 0.1f,
                             0.0f,
                             1000.0f);
            ImGui::DragFloat("reproductionThreshold", &appState.pendingConfig.rabbitTraits.reproductionThreshold,
                             1.0f,
                             0.0f, 10000.0f);
            ImGui::DragInt("reproductionCooldown", &appState.pendingConfig.rabbitTraits.reproductionCooldown, 1, 0,
                           100000);
            ImGui::DragFloat("reproductionChance", &appState.pendingConfig.rabbitTraits.reproductionChance, 0.01f,
                             0.0f,
                             1.0f);
            ImGui::DragFloat("reproductionEnergyCost", &appState.pendingConfig.rabbitTraits.reproductionEnergyCost,
                             1.0f,
                             0.0f, 10000.0f);
            ImGui::DragFloat("visionRange", &appState.pendingConfig.rabbitTraits.visionRange, 0.5f, 0.0f, 100.0f);
            ImGui::DragFloat("fleeingRange", &appState.pendingConfig.rabbitTraits.fleeingRange, 0.5f, 0.0f, 100.0f);
            ImGui::DragFloat("hungerDamage", &appState.pendingConfig.rabbitTraits.hungerDamage, 0.1f, 0.0f,
                             1000.0f);
            ImGui::DragFloat("feedingThreshold", &appState.pendingConfig.rabbitTraits.feedingThreshold, 1.0f, 0.0f,
                             10000.0f);
            ImGui::DragInt("maxAge", &appState.pendingConfig.rabbitTraits.maxAge, 1, 0, 100000);
            appState.configDirty = appState.configDirty || ImGui::IsItemEdited();
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Wolf Traits")) {
            ImGui::PushID("Wolf Traits");
            ImGui::DragFloat("maxEnergy", &appState.pendingConfig.wolfTraits.maxEnergy, 1.0f, 0.0f, 10000.0f);
            ImGui::DragFloat("movementEnergyCost", &appState.pendingConfig.wolfTraits.movementEnergyCost, 0.1f,
                             0.0f,
                             1000.0f);
            ImGui::DragFloat("reproductionThreshold", &appState.pendingConfig.wolfTraits.reproductionThreshold,
                             1.0f, 0.0f,
                             10000.0f);
            ImGui::DragInt("reproductionCooldown", &appState.pendingConfig.wolfTraits.reproductionCooldown, 1, 0,
                           100000);
            ImGui::DragFloat("reproductionChance", &appState.pendingConfig.wolfTraits.reproductionChance, 0.01f,
                             0.0f,
                             1.0f);
            ImGui::DragFloat("reproductionEnergyCost", &appState.pendingConfig.wolfTraits.reproductionEnergyCost,
                             1.0f,
                             0.0f, 10000.0f);
            ImGui::DragFloat("visionRange", &appState.pendingConfig.wolfTraits.visionRange, 0.5f, 0.0f, 100.0f);
            ImGui::DragFloat("hungerDamage", &appState.pendingConfig.wolfTraits.hungerDamage, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat("feedingThreshold", &appState.pendingConfig.wolfTraits.feedingThreshold, 1.0f, 0.0f,
                             10000.0f);
            ImGui::DragInt("maxAge", &appState.pendingConfig.wolfTraits.maxAge, 1, 0, 100000);
            appState.configDirty = appState.configDirty || ImGui::IsItemEdited();
            ImGui::PopID();
        }

        ImGui::Spacing();

        if (ImGui::Button("Apply")) {
            applyPendingConfig();
            appState.currentConfig = appState.pendingConfig;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            appState.pendingConfig = appState.currentConfig;
            appState.configDirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Recreate Now")) {
            applyPendingConfig(true);
            appState.currentConfig = appState.pendingConfig;
        }

        if (appState.configDirty) {
            ImGui::TextColored(ImVec4(1, 0.7f, 0, 1), "Unsaved changes");
        }

        ImGui::End();
    }

    {
        ImGui::Begin("Stats");

        ImGui::Text("%.1f FPS", appState.io->Framerate);

        ImGui::Separator();

        auto entityCounts = appState.sim->getEntityCounts();
        ImGui::Text("Grass: %d", entityCounts[SPECIES_GRASS]);
        ImGui::Text("Rabbit: %d", entityCounts[SPECIES_RABBIT]);
        ImGui::Text("Wolf: %d", entityCounts[SPECIES_WOLF]);

        ImGui::End();
    }

    // Rendering
    ImGui::Render();
}


void App::run() {
    double lastTime = glfwGetTime();
    double timeAccum = 0.0;


    while (!glfwWindowShouldClose(appState.window)) {
        glfwPollEvents();

        renderUI();

        const double now = glfwGetTime();
        const double delta = now - lastTime;
        lastTime = now;
        timeAccum += delta;

        const double stepInterval =
                appState.stepsPerSecond > 0 && !appState.pauseSim
                    ? (1.0 / static_cast<double>(appState.stepsPerSecond))
                    : 1.0e9;
        int stepsThisFrame = 0;

        constexpr int maxStepsPerFrame = 240;

        while (timeAccum >= stepInterval && stepsThisFrame < maxStepsPerFrame) {
            if (appState.sim) appState.sim->step();
            timeAccum -= stepInterval;
            ++stepsThisFrame;
        }

        drawSimulationToTexture();

        int displayW, displayH;
        glfwGetFramebufferSize(appState.window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        drawCenteredTexture(
            appState.quad,
            appState.gridTex,
            appState.currentConfig.gridWidth,
            appState.currentConfig.gridHeight,
            displayW,
            displayH
        );

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(appState.window);
    }
}
