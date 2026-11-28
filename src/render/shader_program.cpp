#include "render/shader_program.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

namespace {

bool compileStage(unsigned int shader, const char* source, const char* stageName, const char* debugName) {
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, static_cast<GLsizei>(sizeof(infoLog)), nullptr, infoLog);
        std::cerr << "ERROR::SHADER::" << stageName << "::COMPILATION_FAILED";
        if (debugName != nullptr) {
            std::cerr << " (" << debugName << ")";
        }
        std::cerr << "\n" << infoLog << std::endl;
        return false;
    }
    return true;
}

} // namespace

ShaderProgram::~ShaderProgram() {
    destroy();
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : program_(other.program_), uniformCache_(std::move(other.uniformCache_)) {
    other.program_ = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        destroy();
        program_ = other.program_;
        uniformCache_ = std::move(other.uniformCache_);
        other.program_ = 0;
    }
    return *this;
}

bool ShaderProgram::build(const char* vertexSource, const char* fragmentSource, const char* debugName) {
    destroy();

    const unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    if (!compileStage(vertexShader, vertexSource, "VERTEX", debugName)) {
        glDeleteShader(vertexShader);
        return false;
    }

    const unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    if (!compileStage(fragmentShader, fragmentSource, "FRAGMENT", debugName)) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    const unsigned int prog = glCreateProgram();
    glAttachShader(prog, vertexShader);
    glAttachShader(prog, fragmentShader);
    glLinkProgram(prog);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(prog, static_cast<GLsizei>(sizeof(infoLog)), nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED";
        if (debugName != nullptr) {
            std::cerr << " (" << debugName << ")";
        }
        std::cerr << "\n" << infoLog << std::endl;
        glDeleteProgram(prog);
        return false;
    }

    program_ = prog;
    uniformCache_.clear();
    return true;
}

void ShaderProgram::destroy() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    uniformCache_.clear();
}

void ShaderProgram::use() const {
    glUseProgram(program_);
}

int ShaderProgram::uniformLocation(const char* name) const {
    if (program_ == 0 || name == nullptr) {
        return -1;
    }
    auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) {
        return it->second;
    }
    const int loc = glGetUniformLocation(program_, name);
    uniformCache_.emplace(name, loc);
    return loc;
}

void ShaderProgram::setInt(const char* name, int value) const {
    const int loc = uniformLocation(name);
    if (loc >= 0) {
        glUniform1i(loc, value);
    }
}

void ShaderProgram::setUInt(const char* name, unsigned int value) const {
    const int loc = uniformLocation(name);
    if (loc >= 0) {
        glUniform1ui(loc, value);
    }
}

void ShaderProgram::setFloat(const char* name, float value) const {
    const int loc = uniformLocation(name);
    if (loc >= 0) {
        glUniform1f(loc, value);
    }
}

void ShaderProgram::setVec2(const char* name, const glm::vec2& value) const {
    const int loc = uniformLocation(name);
    if (loc >= 0) {
        glUniform2fv(loc, 1, glm::value_ptr(value));
    }
}

void ShaderProgram::setVec3(const char* name, const glm::vec3& value) const {
    const int loc = uniformLocation(name);
    if (loc >= 0) {
        glUniform3fv(loc, 1, glm::value_ptr(value));
    }
}

void ShaderProgram::setVec4(const char* name, const glm::vec4& value) const {
    const int loc = uniformLocation(name);
    if (loc >= 0) {
        glUniform4fv(loc, 1, glm::value_ptr(value));
    }
}

void ShaderProgram::setMat4(const char* name, const glm::mat4& value) const {
    const int loc = uniformLocation(name);
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
    }
}
