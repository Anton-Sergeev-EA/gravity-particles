#pragma once
#include <GL/glew.h>

// Звёздный фон: звёзды разного размера и цветовой температуры (от голубых
// до оранжевых), с мерцанием; у самых ярких — дифракционные лучи.
class StarField {
public:
    StarField(int count, float worldW, float worldH);
    ~StarField();

    StarField(const StarField&) = delete;
    StarField& operator=(const StarField&) = delete;

    void render() const;

private:
    GLuint vao_ = 0, vbo_ = 0;
    int count_;
};
