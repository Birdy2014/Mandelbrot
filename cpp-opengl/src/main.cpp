#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>

const char* vertexShaderSource = R"(
#version 440 core
layout (location = 0) in vec2 posIn;

out smooth vec2 pos;

void main() {
    gl_Position = vec4(posIn, 0.0, 1.0);
    pos = posIn;
}
)";

const char* fragmentShaderSource = R"(
#version 440 core
out vec4 FragColor;

in vec2 pos;

uniform dvec2 center;
uniform dvec2 scale;
uniform int max_iterations;

int mandelbrot(dvec2 c, int max_its) {
    dvec2 z = c;
    for (int i = 0; i < max_its; i++){
        if (dot(z, z) > 4.) return i;
        z = dvec2(z.x * z.x - z.y * z.y, 2. * z.x * z.y) + c;
    }
    return max_its;
}

void main() {
    dvec2 mandelbrotPos = center + dvec2(pos) * scale;
    int n = mandelbrot(mandelbrotPos, max_iterations);
    float a = 0.1;
    FragColor = vec4(0.5 * sin(a * float(n)) + 0.5, 0.5 * sin(a * float(n) + 2.094) + 0.5, 0.5 * sin(a * float(n) + 4.188) + 0.5, 255);
}
)";

GLFWwindow* window;
GLuint vao, vbo_pos;
GLuint shader;
glm::dvec2 pos_middle = glm::dvec2(0);
double pixel_per_mandelbrot = 0.003;
int max_iterations = 500;
bool redraw = true;
bool redraw2 = false;

int width = 1290;
int height = 720;

void init_shader() {
    int success;
    char info_log[1024];
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 1024, nullptr, info_log);
        std::cerr << "shader compilation error: name: mandelbrot.vert\n" << info_log << std::endl;
        exit(1);
    }
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 1024, nullptr, info_log);
        std::cerr << "shader compilation error: name: mandelbrot.frag\n" << info_log << std::endl;
        exit(1);
    }
    shader = glCreateProgram();
    glAttachShader(shader, vertexShader);
    glAttachShader(shader, fragmentShader);
    glLinkProgram(shader);
    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shader, 1024, nullptr, info_log);
        std::cerr << "program linking error: \n" << info_log << std::endl;
        exit(1);
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glUseProgram(shader);
}

void init_quad() {
    float quad_vertices[] = {
        // positions
        -1.0f, 1.0f,
        -1.0f, -1.0f,
        1.0f,  1.0f,
        1.0f,  -1.0f,
    };
    // setup plane VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo_pos);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
}

void render_quad() {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void update_mandelbrot() {
    glUniform2d(glGetUniformLocation(shader, "center"), pos_middle.x, pos_middle.y);
    glUniform2d(glGetUniformLocation(shader, "scale"), pixel_per_mandelbrot * (width / 2), pixel_per_mandelbrot * (height / 2));
    glUniform1i(glGetUniformLocation(shader, "max_iterations"), max_iterations);
    render_quad();
}

void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    width = w;
    height = h;
    glViewport(0, 0, width, height);
    redraw = true;
}

void scroll_callback(GLFWwindow*, double xoffset, double yoffset) {
    if (yoffset > 0)
        pixel_per_mandelbrot /= 1.5;
    else
        pixel_per_mandelbrot *= 1.5;
    redraw = true;
}

void key_callback(GLFWwindow*, int key, int, int action, int mods) {
    if (action != GLFW_PRESS)
        return;
    if (key == GLFW_KEY_PAGE_UP || key == GLFW_KEY_UP)
        max_iterations += 10;
    if (key == GLFW_KEY_PAGE_DOWN || key == GLFW_KEY_DOWN)
        max_iterations -= 10;
    redraw = true;
}

void print_usage() {
#if defined(__linux__)
    std::cout << "\033c";
#elif defined(_WIN32)
    system("cls");
#else
#error "Unknown platform"
#endif

std::cout << "OpenGL Mandelbrot Usage:\n";
std::cout << "Zoom: mouse scroll\n";
std::cout << "Move: drag while holding the left mouse button\n";
std::cout << "change max iterations: Arrow up and down\n";
std::cout << '\n';
std::cout << "max iterations: " << max_iterations << '\n';
}

glm::dvec2 cursor_pos() {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    return glm::dvec2(x, y);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create Window
    window = glfwCreateWindow(width, height, "Mandelbrot", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);

    // Load OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return 1;
    }

    // Set Viewport and resize callback
    glViewport(0, 0, width, height);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    init_shader();
    init_quad();

    glm::dvec2 last_cursor_pos = cursor_pos();
    glm::dvec2 cursor_pos_offset = glm::dvec2(0);

    // Mainloop
    while (!glfwWindowShouldClose(window)) {
        cursor_pos_offset = cursor_pos() - last_cursor_pos;
        last_cursor_pos = cursor_pos();
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            pos_middle.x -= cursor_pos_offset.x * pixel_per_mandelbrot;
            pos_middle.y += cursor_pos_offset.y * pixel_per_mandelbrot;
            redraw = true;
        }

        if (redraw) {
            redraw = false;
            redraw2 = true;
            print_usage();
            update_mandelbrot();
        } else if (redraw2) {
            redraw2 = false;
            update_mandelbrot();
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}
