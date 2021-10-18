#version 330 core
out vec4 FragColor;

in vec2 mandelbrotPos;

int mandelbrot(vec2 c, int max_its) {
    vec2 z = c;
    for (int i = 0; i < max_its; i++){
        if (dot(z, z) > 4.) return i;
        z = vec2(z.x * z.x - z.y * z.y, 2. * z.x * z.y) + c;
    }
    return max_its;
}

void main() {
    int n = mandelbrot(mandelbrotPos, 100);
    float a = 0.1;
    FragColor = vec4(0.5 * sin(a * float(n)) + 0.5, 0.5 * sin(a * float(n) + 2.094) + 0.5, 0.5 * sin(a * float(n) + 4.188) + 0.5, 255);
}
