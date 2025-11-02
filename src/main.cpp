#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>

#include "gl_utils.h"
#include "simulation.h"
#include "glm/vec3.hpp"

glm::ivec3 speciesColor(const int speciesId) {
    switch (speciesId) {
        case SPECIES_RABBIT:
            return {200, 200, 200};
        case SPECIES_WOLF:
            return {100, 100, 100};
        case SPECIES_GRASS:
            return {0, 100, 0};
        default:
            return {255, 0, 255};
    }
}

static void drawSimulationToTexture(
    ISimulation &sim,
    const GLuint tex,
    const int gridW,
    const int gridH
) {
    auto [grid, entities] = sim.getGridData();

    // bind once and ensure tight packing
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (int y = 0; y < gridH; ++y) {
        for (int x = 0; x < gridW; ++x) {
            const int cellIdx = y * gridW + x;
            const GridCell &cell = grid[cellIdx];

            uint8_t r = 0, g = 0, b = 0, a = 255;
            if (cell.plantIndex >= 0) {
                const auto color = speciesColor(SPECIES_GRASS);
                r = color.r;
                g = color.g;
                b = color.b;
            }
            if (cell.animalIndex >= 0) {
                const auto animal = entities[cell.animalIndex];
                const auto col = speciesColor(animal.speciesId);

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

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    GLFWwindow *window = glfwCreateWindow(1920, 1080, "EcoSystems", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize GLAD
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    auto quad = createCenteredQuadResources();

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    // Application state
    constexpr auto clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    int stepsPerSecond = 10;
    bool pauseSim = false;
    double lastTime = glfwGetTime();
    double timeAccum = 0.0;
    const int maxStepsPerFrame = 100;

    SimulationSettings config{};
    config.gridWidth = 40;
    config.gridHeight = 40;

    config.grassCount = 300;
    config.grassTraits = SpeciesTraits{
        .maxEnergy = 40.0f,
        .reproductionCooldown = 10,
        .reproductionChance = 0.1f,
        .spontaneousReproductionChance = 0.001f,
        .hungerDamage = 0.0f,
        .maxAge = 500,
    };

    config.rabbitCount = 200;
    config.rabbitTraits = SpeciesTraits{
        .maxEnergy = 200.0f,
        .movementEnergyCost = 1.0f,
        .reproductionThreshold = 100.0f,
        .reproductionCooldown = 50,
        .reproductionChance = 0.3f,
        .reproductionEnergyCost = 100.0f,
        .visionRange = 16.0f,
        .fleeingRange = 4.0f,
        .hungerDamage = 1.0f,
        .feedingThreshold = 150.0f,
        .maxAge = 500,
    };

    config.wolfCount = 20;
    config.wolfTraits = SpeciesTraits{
        .maxEnergy = 400.0f,
        .movementEnergyCost = 2.0f,
        .reproductionThreshold = 150.0f,
        .reproductionCooldown = 120,
        .reproductionChance = 0.01f,
        .reproductionEnergyCost = 150.0f,
        .visionRange = 12.0f,
        .hungerDamage = 0.5f,
        .feedingThreshold = 250.0f,
        .maxAge = 700,
    };


    int prevGridHeight = config.gridHeight, prevGridWidth = config.gridWidth;


    static SimulationSettings uiPending = config;
    static bool uiPendingDirty;

    GLuint gridTex = createGridTexture(config.gridWidth, config.gridHeight);
    std::vector<uint32_t> gridColors(config.gridWidth * config.gridHeight, packRGBA(0, 0, 0, 255));
    updateGridTexture(gridTex, config.gridWidth, config.gridHeight, gridColors);

    auto sim = createEcosystemSimulation(config);

    auto applyPendingConfig = [&](const bool forceRecreate = false) {
        // If grid size changed we must recreate texture and simulation
        const bool gridSizeChanged = (uiPending.gridWidth != prevGridWidth) || (uiPending.gridHeight != prevGridHeight);
        if (gridSizeChanged || forceRecreate) {
            prevGridWidth = uiPending.gridWidth;
            prevGridHeight = uiPending.gridHeight;

            if (gridTex) {
                glDeleteTextures(1, &gridTex);
                gridTex = 0;
            }

            gridTex = createGridTexture(uiPending.gridWidth, uiPending.gridHeight);
            gridColors.assign(uiPending.gridWidth * uiPending.gridHeight, packRGBA(0, 0, 0, 255));
            updateGridTexture(gridTex, uiPending.gridWidth, uiPending.gridHeight, gridColors);

            sim = createEcosystemSimulation(uiPending);
        } else {
            // grid size unchanged: recreate sim to pick up changed counts/traits (simple, safe)
            sim = createEcosystemSimulation(uiPending);
        }

        uiPendingDirty = false;
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::Begin("Simulation Settings");

            ImGui::SliderInt("Steps / s", &stepsPerSecond, 0, 240);
            ImGui::Checkbox("Pause", &pauseSim);
            ImGui::Text("Current step %d", sim->iteration);
            ImGui::End();
        }

        {
            ImGui::Begin("World Settings");

            if (ImGui::InputInt("Width", &uiPending.gridWidth)) uiPendingDirty = true;
            if (ImGui::InputInt("Height", &uiPending.gridHeight)) uiPendingDirty = true;
            uiPending.gridWidth = std::clamp(uiPending.gridWidth, 10, 320);
            uiPending.gridHeight = std::clamp(uiPending.gridHeight, 10, 320);

            ImGui::Separator();

            if (ImGui::CollapsingHeader("Entities Counts")) {
                if (ImGui::InputInt("Grass", &uiPending.grassCount)) uiPendingDirty = true;
                if (ImGui::InputInt("Rabbit", &uiPending.rabbitCount)) uiPendingDirty = true;
                if (ImGui::InputInt("Wolf", &uiPending.wolfCount)) uiPendingDirty = true;
            }

            ImGui::Separator();

            if (ImGui::CollapsingHeader("Grass Traits")) {
                ImGui::PushID("Grass");
                ImGui::DragFloat("maxEnergy", &uiPending.grassTraits.maxEnergy, 1.0f, 0.0f, 10000.0f);
                ImGui::DragInt("reproductionCooldown", &uiPending.grassTraits.reproductionCooldown, 1, 0, 10000);
                ImGui::DragFloat("reproductionChance", &uiPending.grassTraits.reproductionChance, 0.01f, 0.0f,
                                 1.0f);
                ImGui::DragFloat("spontaneousReproductionChance", &uiPending.grassTraits.spontaneousReproductionChance,
                                 0.0001f, 0.0f, 0.1f);
                ImGui::DragFloat("hungerDamage", &uiPending.grassTraits.hungerDamage, 0.1f, 0.0f, 1000.0f);
                ImGui::DragInt("maxAge", &uiPending.grassTraits.maxAge, 1, 0, 100000);
                uiPendingDirty = uiPendingDirty || ImGui::IsItemEdited();
                ImGui::PopID();
            }
            if (ImGui::CollapsingHeader("Rabbit Traits")) {
                ImGui::PushID("Rabbit Traits");
                ImGui::DragFloat("maxEnergy", &uiPending.rabbitTraits.maxEnergy, 1.0f, 0.0f, 10000.0f);
                ImGui::DragFloat("movementEnergyCost", &uiPending.rabbitTraits.movementEnergyCost, 0.1f, 0.0f,
                                 1000.0f);
                ImGui::DragFloat("reproductionThreshold", &uiPending.rabbitTraits.reproductionThreshold, 1.0f,
                                 0.0f, 10000.0f);
                ImGui::DragInt("reproductionCooldown", &uiPending.rabbitTraits.reproductionCooldown, 1, 0,
                               100000);
                ImGui::DragFloat("reproductionChance", &uiPending.rabbitTraits.reproductionChance, 0.01f, 0.0f,
                                 1.0f);
                ImGui::DragFloat("reproductionEnergyCost", &uiPending.rabbitTraits.reproductionEnergyCost, 1.0f,
                                 0.0f, 10000.0f);
                ImGui::DragFloat("visionRange", &uiPending.rabbitTraits.visionRange, 0.5f, 0.0f, 100.0f);
                ImGui::DragFloat("fleeingRange", &uiPending.rabbitTraits.fleeingRange, 0.5f, 0.0f, 100.0f);
                ImGui::DragFloat("hungerDamage", &uiPending.rabbitTraits.hungerDamage, 0.1f, 0.0f, 1000.0f);
                ImGui::DragFloat("feedingThreshold", &uiPending.rabbitTraits.feedingThreshold, 1.0f, 0.0f,
                                 10000.0f);
                ImGui::DragInt("maxAge", &uiPending.rabbitTraits.maxAge, 1, 0, 100000);
                uiPendingDirty = uiPendingDirty || ImGui::IsItemEdited();
                ImGui::PopID();
            }
            if (ImGui::CollapsingHeader("Wolf Traits")) {
                ImGui::PushID("Wolf Traits");
                ImGui::DragFloat("maxEnergy", &uiPending.wolfTraits.maxEnergy, 1.0f, 0.0f, 10000.0f);
                ImGui::DragFloat("movementEnergyCost", &uiPending.wolfTraits.movementEnergyCost, 0.1f, 0.0f,
                                 1000.0f);
                ImGui::DragFloat("reproductionThreshold", &uiPending.wolfTraits.reproductionThreshold, 1.0f, 0.0f,
                                 10000.0f);
                ImGui::DragInt("reproductionCooldown", &uiPending.wolfTraits.reproductionCooldown, 1, 0, 100000);
                ImGui::DragFloat("reproductionChance", &uiPending.wolfTraits.reproductionChance, 0.01f, 0.0f,
                                 1.0f);
                ImGui::DragFloat("reproductionEnergyCost", &uiPending.wolfTraits.reproductionEnergyCost, 1.0f,
                                 0.0f, 10000.0f);
                ImGui::DragFloat("visionRange", &uiPending.wolfTraits.visionRange, 0.5f, 0.0f, 100.0f);
                ImGui::DragFloat("hungerDamage", &uiPending.wolfTraits.hungerDamage, 0.1f, 0.0f, 1000.0f);
                ImGui::DragFloat("feedingThreshold", &uiPending.wolfTraits.feedingThreshold, 1.0f, 0.0f, 10000.0f);
                ImGui::DragInt("maxAge", &uiPending.wolfTraits.maxAge, 1, 0, 100000);
                uiPendingDirty = uiPendingDirty || ImGui::IsItemEdited();
                ImGui::PopID();
            }

            ImGui::Spacing();

            if (ImGui::Button("Apply")) {
                applyPendingConfig();
                config = uiPending;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                uiPending = config;
                uiPendingDirty = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Recreate Now")) {
                applyPendingConfig(true);
                config = uiPending;
            }

            if (uiPendingDirty) {
                ImGui::TextColored(ImVec4(1, 0.7f, 0, 1), "Unsaved changes");
            }

            ImGui::End();
        }

        {
            ImGui::Begin("Stats");

            ImGui::Text("%.1f FPS", io.Framerate);

            ImGui::Separator();

            auto entityCounts = sim->getEntityCounts();
            ImGui::Text("Grass: %d", entityCounts[SPECIES_GRASS]);
            ImGui::Text("Rabbit: %d", entityCounts[SPECIES_RABBIT]);
            ImGui::Text("Wolf: %d", entityCounts[SPECIES_WOLF]);

            ImGui::End();
        }

        if (config.gridHeight != prevGridHeight || config.gridWidth != prevGridWidth) {
            prevGridHeight = config.gridHeight;
            prevGridWidth = config.gridWidth;

            glDeleteTextures(0, &gridTex);
            gridTex = createGridTexture(config.gridWidth, config.gridHeight);
            gridColors.resize(config.gridWidth * config.gridHeight, packRGBA(0, 0, 0, 255));
            updateGridTexture(gridTex, config.gridWidth, config.gridHeight, gridColors);

            sim = createEcosystemSimulation(config);
        }

        // Rendering
        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        int displayWidth, displayHeight;

        const double now = glfwGetTime();
        const double delta = now - lastTime;
        lastTime = now;
        timeAccum += delta;

        double stepInterval = (stepsPerSecond > 0 && !pauseSim)
                                  ? (1.0 / static_cast<double>(stepsPerSecond))
                                  : 1.0e9;
        int stepsThisFrame = 0;
        while (timeAccum >= stepInterval && stepsThisFrame < maxStepsPerFrame) {
            sim->step();
            timeAccum -= stepInterval;
            ++stepsThisFrame;
        }

        drawSimulationToTexture(*sim, gridTex, config.gridWidth, config.gridHeight);

        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
        drawCenteredTexture(quad, gridTex, config.gridWidth, config.gridHeight, displayWidth, displayHeight);

        // Render ImGui
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    destroyCenteredQuadResources(quad);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
