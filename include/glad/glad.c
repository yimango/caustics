#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glad.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// OpenGL function pointers
PFNGLCLEARPROC glad_glClear = NULL;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
PFNGLDRAWELEMENTSPROC glad_glDrawElements = NULL;
PFNGLVIEWPORTPROC glad_glViewport = NULL;
PFNGLENABLEPROC glad_glEnable = NULL;
PFNGLDISABLEPROC glad_glDisable = NULL;
PFNGLCULLFACEPROC glad_glCullFace = NULL;
PFNGLBLENDFUNCPROC glad_glBlendFunc = NULL;
PFNGLDEPTHMASKPROC glad_glDepthMask = NULL;
PFNGLDRAWBUFFERSPROC glad_glDrawBuffers = NULL;
PFNGLREADBUFFERPROC glad_glReadBuffer = NULL;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;

// Vertex Arrays
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;

// Buffers
PFNGLGENBUFFERSPROC glGenBuffers = NULL;
PFNGLBINDBUFFERPROC glBindBuffer = NULL;
PFNGLBUFFERDATAPROC glBufferData = NULL;
PFNGLBUFFERSUBDATAPROC glBufferSubData = NULL;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = NULL;

// Shaders
PFNGLCREATESHADERPROC glCreateShader = NULL;
PFNGLSHADERSOURCEPROC glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glCompileShader = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
PFNGLDELETESHADERPROC glDeleteShader = NULL;

// Programs
PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
PFNGLATTACHSHADERPROC glAttachShader = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
PFNGLUSEPROGRAMPROC glUseProgram = NULL;
PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;

// Uniforms
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
PFNGLUNIFORM1IPROC glUniform1i = NULL;
PFNGLUNIFORM1FPROC glUniform1f = NULL;
PFNGLUNIFORM3FVPROC glUniform3fv = NULL;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = NULL;

// Textures
PFNGLGENTEXTURESPROC glGenTextures = NULL;
PFNGLBINDTEXTUREPROC glBindTexture = NULL;
PFNGLTEXIMAGE2DPROC glTexImage2D = NULL;
PFNGLTEXPARAMETERIPROC glTexParameteri = NULL;
PFNGLACTIVETEXTUREPROC glActiveTexture = NULL;
PFNGLDELETETEXTURESPROC glDeleteTextures = NULL;

// Framebuffers
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;

static void* get_proc(GLADloadproc load, const char *name) {
    void *proc = load(name);
    if (!proc) {
        fprintf(stderr, "Failed to load %s\n", name);
    }
    return proc;
}

int gladLoadGLLoader(GLADloadproc load) {
    if (!load) {
        return 0;
    }

    // Standard OpenGL functions
    glad_glClear = (PFNGLCLEARPROC)get_proc(load, "glClear");
    glad_glClearColor = (PFNGLCLEARCOLORPROC)get_proc(load, "glClearColor");
    glad_glDrawElements = (PFNGLDRAWELEMENTSPROC)get_proc(load, "glDrawElements");
    glad_glViewport = (PFNGLVIEWPORTPROC)get_proc(load, "glViewport");
    glad_glEnable = (PFNGLENABLEPROC)get_proc(load, "glEnable");
    glad_glDisable = (PFNGLDISABLEPROC)get_proc(load, "glDisable");
    glad_glCullFace = (PFNGLCULLFACEPROC)get_proc(load, "glCullFace");
    glad_glBlendFunc = (PFNGLBLENDFUNCPROC)get_proc(load, "glBlendFunc");
    glad_glDepthMask = (PFNGLDEPTHMASKPROC)get_proc(load, "glDepthMask");
    glad_glDrawBuffers = (PFNGLDRAWBUFFERSPROC)get_proc(load, "glDrawBuffers");
    glad_glReadBuffer = (PFNGLREADBUFFERPROC)get_proc(load, "glReadBuffer");
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)get_proc(load, "glDrawArrays");

    // Vertex Arrays
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)get_proc(load, "glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)get_proc(load, "glBindVertexArray");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)get_proc(load, "glDeleteVertexArrays");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)get_proc(load, "glEnableVertexAttribArray");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)get_proc(load, "glVertexAttribPointer");

    // Buffers
    glGenBuffers = (PFNGLGENBUFFERSPROC)get_proc(load, "glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)get_proc(load, "glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)get_proc(load, "glBufferData");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)get_proc(load, "glBufferSubData");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)get_proc(load, "glDeleteBuffers");

    // Shaders
    glCreateShader = (PFNGLCREATESHADERPROC)get_proc(load, "glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)get_proc(load, "glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)get_proc(load, "glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)get_proc(load, "glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)get_proc(load, "glGetShaderInfoLog");
    glDeleteShader = (PFNGLDELETESHADERPROC)get_proc(load, "glDeleteShader");

    // Programs
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)get_proc(load, "glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)get_proc(load, "glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)get_proc(load, "glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)get_proc(load, "glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)get_proc(load, "glGetProgramInfoLog");
    glUseProgram = (PFNGLUSEPROGRAMPROC)get_proc(load, "glUseProgram");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)get_proc(load, "glDeleteProgram");

    // Uniforms
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)get_proc(load, "glGetUniformLocation");
    glUniform1i = (PFNGLUNIFORM1IPROC)get_proc(load, "glUniform1i");
    glUniform1f = (PFNGLUNIFORM1FPROC)get_proc(load, "glUniform1f");
    glUniform3fv = (PFNGLUNIFORM3FVPROC)get_proc(load, "glUniform3fv");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)get_proc(load, "glUniformMatrix4fv");

    // Textures
    glGenTextures = (PFNGLGENTEXTURESPROC)get_proc(load, "glGenTextures");
    glBindTexture = (PFNGLBINDTEXTUREPROC)get_proc(load, "glBindTexture");
    glTexImage2D = (PFNGLTEXIMAGE2DPROC)get_proc(load, "glTexImage2D");
    glTexParameteri = (PFNGLTEXPARAMETERIPROC)get_proc(load, "glTexParameteri");
    glActiveTexture = (PFNGLACTIVETEXTUREPROC)get_proc(load, "glActiveTexture");
    glDeleteTextures = (PFNGLDELETETEXTURESPROC)get_proc(load, "glDeleteTextures");

    // Framebuffers
    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)get_proc(load, "glGenFramebuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)get_proc(load, "glBindFramebuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)get_proc(load, "glFramebufferTexture2D");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)get_proc(load, "glCheckFramebufferStatus");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)get_proc(load, "glDeleteFramebuffers");

    return 1; // Success
} 