#version 330 core
layout (location = 0) in vec3 posIn;
layout (location = 1) in vec2 mandelbrotIn;

out vec2 mandelbrotPos;

void main() {
    gl_Position = vec4(posIn, 1.0);
    mandelbrotPos = mandelbrotIn;
}
