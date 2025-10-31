#include "gl_utils.h"

#include <iostream>
#include <ostream>

#include "glad/gl.h"

void error_callback(int error, const char *description) {
    std::cerr << "Error: " << description << std::endl;
}

GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        fprintf(stderr, "Shader compile error: %s\n", buf);
    }
    return s;
}


GLuint link_program(const GLuint vs, const GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetProgramInfoLog(p, sizeof(buf), nullptr, buf);
        fprintf(stderr, "Program link error: %s\n", buf);
    }
    return p;
}
