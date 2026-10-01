#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

std::string Shader::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Не удалось открыть файл шейдера: " + path);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint Shader::compile(GLenum type, const std::string& source, const std::string& debugName) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "[Shader] Ошибка компиляции (" << debugName << "):\n" << log << std::endl;
        throw std::runtime_error("Ошибка компиляции шейдера: " + debugName);
    }
    return shader;
}

Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    GLuint vs = compile(GL_VERTEX_SHADER, readFile(vertPath), vertPath);
    GLuint fs = compile(GL_FRAGMENT_SHADER, readFile(fragPath), fragPath);
    id = glCreateProgram();
    link(vs, fs, vertPath + " + " + fragPath);
}

Shader::Shader(const std::string& vertPath, const std::vector<std::string>& feedbackVaryings) {
    GLuint vs = compile(GL_VERTEX_SHADER, readFile(vertPath), vertPath);
    id = glCreateProgram();
    std::vector<const char*> names;
    for (const auto& v : feedbackVaryings) names.push_back(v.c_str());
    // Список выходов задаётся до линковки.
    glTransformFeedbackVaryings(id, static_cast<GLsizei>(names.size()), names.data(),
                                GL_INTERLEAVED_ATTRIBS);
    link(vs, 0, vertPath);
}

void Shader::link(GLuint vs, GLuint fs, const std::string& debugName) {
    glAttachShader(id, vs);
    if (fs) glAttachShader(id, fs);
    glLinkProgram(id);

    GLint success = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(id, sizeof(log), nullptr, log);
        std::cerr << "[Shader] Ошибка линковки (" << debugName << "):\n" << log << std::endl;
        glDeleteProgram(id);
        id = 0;
        throw std::runtime_error("Ошибка линковки шейдерной программы: " + debugName);
    }
}

Shader::~Shader() {
    if (id != 0) glDeleteProgram(id);
}

Shader::Shader(Shader&& other) noexcept : id(other.id) {
    other.id = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (id != 0) glDeleteProgram(id);
        id = other.id;
        other.id = 0;
    }
    return *this;
}

void Shader::use() const {
    glUseProgram(id);
}

GLint Shader::uniformLoc(const std::string& name) const {
    return glGetUniformLocation(id, name.c_str());
}

void Shader::setInt(const std::string& name, int value) const {
    glUniform1i(uniformLoc(name), value);
}

void Shader::setUInt(const std::string& name, unsigned value) const {
    glUniform1ui(uniformLoc(name), value);
}

void Shader::setVec4Array(const std::string& name, const float* data, int count) const {
    if (count > 0) glUniform4fv(uniformLoc(name), count, data);
}

void Shader::setFloat(const std::string& name, float value) const {
    glUniform1f(uniformLoc(name), value);
}

void Shader::setVec2(const std::string& name, float x, float y) const {
    glUniform2f(uniformLoc(name), x, y);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const {
    glUniform3f(uniformLoc(name), x, y, z);
}

void Shader::setMat4(const std::string& name, const float* mat4) const {
    glUniformMatrix4fv(uniformLoc(name), 1, GL_FALSE, mat4);
}
