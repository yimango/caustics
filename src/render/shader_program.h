#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    bool build(const char* vertexSource, const char* fragmentSource, const char* debugName = nullptr);
    void destroy();

    void use() const;
    bool valid() const { return program_ != 0; }
    unsigned int id() const { return program_; }

    int uniformLocation(const char* name) const;
    void setInt(const char* name, int value) const;
    void setUInt(const char* name, unsigned int value) const;
    void setFloat(const char* name, float value) const;
    void setVec2(const char* name, const glm::vec2& value) const;
    void setVec3(const char* name, const glm::vec3& value) const;
    void setVec4(const char* name, const glm::vec4& value) const;
    void setMat4(const char* name, const glm::mat4& value) const;

private:
    unsigned int program_ = 0;
    mutable std::unordered_map<std::string, int> uniformCache_;
};
