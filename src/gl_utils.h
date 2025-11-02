#ifndef ECOSYSTEM_GL_UTILS_H
#define ECOSYSTEM_GL_UTILS_H
#include <vector>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

GLuint compileShader(GLenum type, const char *src);

GLuint linkProgram(GLuint vs, GLuint fs);

struct CenteredQuad {
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint uniTex = -1;
};

constexpr int BASE_CELL_PIXEL_SIZE = 2;

CenteredQuad createCenteredQuadResources();

GLuint createGridTexture(int w, int h);

void updateGridTexture(GLuint tex, int w, int h, const std::vector<uint32_t> &colors
);

uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

void drawCenteredTexture(const CenteredQuad &q, GLuint tex, int gridW, int gridH, int displayW, int displayH);

void destroyCenteredQuadResources(CenteredQuad &q);

#endif //ECOSYSTEM_GL_UTILS_H
