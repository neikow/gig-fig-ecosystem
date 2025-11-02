#include "gl_utils.h"

#include <cassert>
#include <iostream>
#include <string>

GLuint compileShader(const GLenum type, const char *src) {
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

GLuint linkProgram(GLuint vs, GLuint fs) {
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

CenteredQuad createCenteredQuadResources() {
    auto vs_src =
            "#version 330 core\n"
            "layout(location=0) in vec2 aPos;\n"
            "layout(location=1) in vec2 aUV;\n"
            "out vec2 vUV;\n"
            "void main() {\n"
            "  vUV = aUV;\n"
            "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
            "}\n";
    auto fs_src =
            "#version 330 core\n"
            "in vec2 vUV;\n"
            "out vec4 outColor;\n"
            "uniform sampler2D uTex;\n"
            "void main() {\n"
            "  outColor = texture(uTex, vUV);\n"
            "}\n";
    const GLuint vs = compileShader(GL_VERTEX_SHADER, vs_src);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, fs_src);
    const GLuint prog = linkProgram(vs, fs);
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


GLuint createGridTexture(const int w, const int h) {
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

void updateGridTexture(const GLuint tex, const int w, const int h, const std::vector<uint32_t> &colors) {
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

uint32_t packRGBA(const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a) {
    return (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16) | (static_cast<uint32_t>(b) << 8) |
           static_cast<uint32_t>(a);
}

void drawCenteredTexture(
    const CenteredQuad &q,
    const GLuint tex,
    const int gridW,
    const int gridH,
    const int displayW,
    const int displayH
) {
    if (!q.program || !q.vao) return;

    const int maxCellByWidth = displayW / std::max(1, gridW);
    const int maxCellByHeight = displayH / std::max(1, gridH);
    const int maxCellThatFits = std::min(maxCellByWidth, maxCellByHeight);
    const int cellPx = std::max(1, maxCellThatFits);

    const int targetW = gridW * cellPx;
    const int targetH = gridH * cellPx;

    // If still larger than display due to rounding, clamp (defensive)
    const int drawW = std::min(targetW, displayW);
    const int drawH = std::min(targetH, displayH);

    // top-left in pixels (origin top-left)
    const int px = (displayW - drawW) / 2;
    const int py = (displayH - drawH) / 2;

    const float leftN = 2.0f * (static_cast<float>(px) / static_cast<float>(displayW)) - 1.0f;
    const float rightN = 2.0f * (static_cast<float>(px + drawW) / static_cast<float>(displayW)) - 1.0f;
    const float topN = 1.0f - 2.0f * (static_cast<float>(py) / static_cast<float>(displayH));
    const float bottomN = 1.0f - 2.0f * (static_cast<float>(py + drawH) / static_cast<float>(displayH));

    // Two triangles (pos.xy, uv.xy)
    // Swap V: top -> 0.0, bottom -> 1.0 to flip vertically (texture upload is top-down)
    const float verts[6 * 4] = {
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

void destroyCenteredQuadResources(CenteredQuad &q) {
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
