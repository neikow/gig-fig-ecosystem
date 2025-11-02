#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>

#include "simulation.h"
#include "glm/vec3.hpp"

static GLuint compileShader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::cerr << log << std::endl;
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(p, len, nullptr, log.data());
        std::cerr << log << std::endl;
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

struct CenteredQuad {
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint uniTex = -1;
};

// Configure how many screen pixels each logical cell should occupy by default.
// This will be reduced automatically if the grid would not fit the screen.
static constexpr int BASE_CELL_PIXEL_SIZE = 2;

static CenteredQuad createCenteredQuadResources() {
    const char *vs_src =
            "#version 330 core\n"
            "layout(location=0) in vec2 aPos;\n"
            "layout(location=1) in vec2 aUV;\n"
            "out vec2 vUV;\n"
            "void main() {\n"
            "  vUV = aUV;\n"
            "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
            "}\n";
    const char *fs_src =
            "#version 330 core\n"
            "in vec2 vUV;\n"
            "out vec4 outColor;\n"
            "uniform sampler2D uTex;\n"
            "void main() {\n"
            "  outColor = texture(uTex, vUV);\n"
            "}\n";
    GLuint vs = compileShader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fs_src);
    GLuint prog = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    CenteredQuad q{};
    q.program = prog;
    q.uniTex = glGetUniformLocation(prog, "uTex");

    glGenVertexArrays(1, &q.vao);
    glBindVertexArray(q.vao);
    glGenBuffers(1, &q.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, q.vbo);
    // allocate space for 6 vertices (pos.xy + uv.xy) floats
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(2 * sizeof(float)));

    glBindVertexArray(0);
    return q;
}


static GLuint createGridTexture(const int w, const int h) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // ensure tight packing for arbitrary widths
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // allocate but do not initialize
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static void updateGridTexture(const GLuint tex, const int w, const int h, const std::vector<uint32_t> &colors) {
    assert(static_cast<int>(colors.size()) == w * h);
    std::vector<uint8_t> pixels;
    pixels.resize(w * h * 4);
    for (int i = 0; i < w * h; ++i) {
        uint32_t c = colors[i];
        uint8_t r = (c >> 24) & 0xFF;
        uint8_t g = (c >> 16) & 0xFF;
        uint8_t b = (c >> 8) & 0xFF;
        uint8_t a = (c >> 0) & 0xFF;
        pixels[i * 4 + 0] = r;
        pixels[i * 4 + 1] = g;
        pixels[i * 4 + 2] = b;
        pixels[i * 4 + 3] = a;
    }
    glBindTexture(GL_TEXTURE_2D, tex);
    // ensure tight packing for arbitrary widths
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // replace whole texture
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

static inline uint32_t packRGBA(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a = 255) {
    return (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16) | (static_cast<uint32_t>(b) << 8) |
           static_cast<uint32_t>(a);
}

static void drawCenteredTexture(const CenteredQuad &q, GLuint tex, int gridW, int gridH, int displayW, int displayH) {
    if (!q.program || !q.vao) return;
    // base desired on-screen size (BASE_CELL_PIXEL_SIZE pixels per logical cell)
    const int desiredCellPx = BASE_CELL_PIXEL_SIZE;

    // Compute the maximum integer cell size that fits the display.
    // We allow scaling down (cell size reduced) so the entire grid fits. Minimum is 1.
    int maxCellByWidth = displayW / std::max(1, gridW);
    int maxCellByHeight = displayH / std::max(1, gridH);
    int maxCellThatFits = std::min(maxCellByWidth, maxCellByHeight);
    // Scale up: choose the largest integer cell size that fits the display so the
    // grid fills as much screen space as possible (but at least 1 px).
    int cellPx = std::max(1, maxCellThatFits);

    const int targetW = gridW * cellPx;
    const int targetH = gridH * cellPx;

    // If still larger than display due to rounding, clamp (defensive)
    const int drawW = std::min(targetW, displayW);
    const int drawH = std::min(targetH, displayH);

    // top-left in pixels (origin top-left)
    const int px = (displayW - drawW) / 2;
    const int py = (displayH - drawH) / 2;

    // Convert pixel coords to NDC (-1..1)
    const float leftN = 2.0f * (float(px) / float(displayW)) - 1.0f;
    const float rightN = 2.0f * (float(px + drawW) / float(displayW)) - 1.0f;
    // note: OpenGL NDC Y goes -1 bottom to +1 top
    const float topN = 1.0f - 2.0f * (float(py) / float(displayH));
    const float bottomN = 1.0f - 2.0f * (float(py + drawH) / float(displayH));

    // Two triangles (pos.xy, uv.xy)
    // Swap V: top -> 0.0, bottom -> 1.0 to flip vertically (texture upload is top-down)
    float verts[6 * 4] = {
        // tri 1
        leftN, topN, 0.0f, 0.0f,
        leftN, bottomN, 0.0f, 1.0f,
        rightN, bottomN, 1.0f, 1.0f,
        // tri 2
        leftN, topN, 0.0f, 0.0f,
        rightN, bottomN, 1.0f, 1.0f,
        rightN, topN, 1.0f, 0.0f
    };

    // Setup state
    glUseProgram(q.program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    if (q.uniTex >= 0)
        glUniform1i(q.uniTex, 0);

    glBindVertexArray(q.vao);
    glBindBuffer(GL_ARRAY_BUFFER, q.vbo);
    // update vertex data
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
}

static void destroyCenteredQuadResources(CenteredQuad &q) {
    if (q.vbo) {
        glDeleteBuffers(1, &q.vbo);
        q.vbo = 0;
    }
    if (q.vao) {
        glDeleteVertexArrays(1, &q.vao);
        q.vao = 0;
    }
    if (q.program) {
        glDeleteProgram(q.program);
        q.program = 0;
    }
}

static int clamp(const int val, const int minVal, const int maxVal) {
    return std::max(minVal, std::min(maxVal, val));
}

glm::ivec3 speciesColor(const int speciesId) {
    switch (speciesId) {
        case SPECIES_RABBIT:
            return glm::ivec3(200, 200, 200);
        case SPECIES_WOLF:
            return glm::ivec3(100, 100, 100);
        case SPECIES_GRASS:
            return glm::ivec3(0, 100, 0);
        default:
            return glm::ivec3(255, 0, 255);
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

                r = clamp(r + col.r, 0, 255);
                g = clamp(g + col.r, 0, 255);
                b = clamp(b + col.r, 0, 255);
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

    SimulationSettings config;
    config.gridWidth = 40;
    config.gridHeight = 40;

    config.grassCount = 200;
    config.grassTraits = SpeciesTraits{
        .maxEnergy = 30.0f,
        .reproductionCooldown = 10,
        .reproductionChance = 0.1f,
        .hungerDamage = 0.0f,
        .maxAge = 500,
    };

    config.rabbitCount = 100;
    config.rabbitTraits = SpeciesTraits{
        .maxEnergy = 200.0f,
        .movementEnergyCost = 5.0f,
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
        .maxEnergy = 300.0f,
        .movementEnergyCost = 8.0f,
        .reproductionThreshold = 150.0f,
        .reproductionCooldown = 80,
        .reproductionChance = 0.2f,
        .reproductionEnergyCost = 150.0f,
        .visionRange = 8.0f,
        .hungerDamage = 2.0f,
        .feedingThreshold = 250.0f,
        .maxAge = 700,
    };


    int prevGridHeight = config.gridHeight, prevGridWidth = config.gridWidth;

    GLuint gridTex = createGridTexture(config.gridWidth, config.gridHeight);
    std::vector<uint32_t> gridColors(config.gridWidth * config.gridHeight, packRGBA(0, 0, 0, 255));
    updateGridTexture(gridTex, config.gridWidth, config.gridHeight, gridColors);

    auto sim = createEcosystemSimulation(config);

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

            ImGui::SliderInt("Height", &config.gridHeight, 10, 320);
            ImGui::SliderInt("Width", &config.gridWidth, 10, 320);
            ImGui::End();
        }

        {
            ImGui::Begin("Stats");

            ImGui::Text("%.1f FPS", io.Framerate);

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

        double stepInterval = (stepsPerSecond > 0 && !pauseSim) ? (1.0 / static_cast<double>(stepsPerSecond)) : 1.0e9;
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
