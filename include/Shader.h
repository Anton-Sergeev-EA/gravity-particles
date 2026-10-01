#pragma once
#include <GL/glew.h>
#include <string>
#include <vector>

// Простой класс-обёртка для компиляции и линковки шейдерных программ OpenGL.
class Shader {
public:
    GLuint id = 0;

    Shader() = default;
    Shader(const std::string& vertPath, const std::string& fragPath);
    // Программа только с вершинным шейдером, результаты которого записываются
    // в буфер через transform feedback (GPU-физика частиц).
    Shader(const std::string& vertPath, const std::vector<std::string>& feedbackVaryings);
    ~Shader();

    // Запрещаем копирование (владение GL-хендлом), разрешаем перемещение.
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void use() const;

    void setInt(const std::string& name, int value) const;
    void setUInt(const std::string& name, unsigned value) const;
    void setVec4Array(const std::string& name, const float* data, int count) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, float x, float y) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setMat4(const std::string& name, const float* mat4) const;

private:
    static std::string readFile(const std::string& path);
    static GLuint compile(GLenum type, const std::string& source, const std::string& debugName);
    GLint uniformLoc(const std::string& name) const;
    void link(GLuint vs, GLuint fs, const std::string& debugName);
};
