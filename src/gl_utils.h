#ifndef GIG_FIG_ECOSYSTEM_GL_UTILS_H
#define GIG_FIG_ECOSYSTEM_GL_UTILS_H
#include "glad/gl.h"

void error_callback(int error, const char *description);

GLuint compile_shader(GLenum type, const char *src);

GLuint link_program(GLuint vs, GLuint fs);

#endif //GIG_FIG_ECOSYSTEM_GL_UTILS_H
