#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>

const char* vertexShaderSource = R"(
#version 440 core
layout (location = 0) in vec2 posIn;
layout (location = 1) in vec2 mandelbrotIn;

out vec2 mandelbrotPos;

void main() {
    gl_Position = vec4(posIn, 0.0, 1.0);
    mandelbrotPos = mandelbrotIn;
}
)";

const char* fragmentShaderSource = R"(
#version 440 core
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
    int n = mandelbrot(mandelbrotPos, 2000);
    float a = 0.1;
    FragColor = vec4(0.5 * sin(a * float(n)) + 0.5, 0.5 * sin(a * float(n) + 2.094) + 0.5, 0.5 * sin(a * float(n) + 4.188) + 0.5, 255);
}
)";

GLFWwindow* window;
GLuint vao, vbo_pos, vbo_mandelbrot;
GLuint shader;
glm::vec2 pos_middle = glm::vec2(0);
double pixel_per_mandelbrot = 0.003;

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


    glGenBuffers(1, &vbo_mandelbrot);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_mandelbrot);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
}

void update_quad(glm::vec2 bottom_left, glm::vec2 top_right) {
    float quad_vertices[] = {
        // Mandelbrot Coords
        bottom_left.x, top_right.y,
        bottom_left.x, bottom_left.y,
        top_right.x,   top_right.y,
        top_right.x,   bottom_left.y,
    };
    glBindBuffer(GL_ARRAY_BUFFER, vbo_mandelbrot);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad_vertices), &quad_vertices);
}

void render_quad() {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void update_mandelbrot_pos() {
    glm::vec2 bottom_left, top_right;
    bottom_left.x = pos_middle.x - (width / 2) * pixel_per_mandelbrot;
    bottom_left.y = pos_middle.y - (height / 2) * pixel_per_mandelbrot;
    top_right.x = pos_middle.x + (width / 2) * pixel_per_mandelbrot;
    top_right.y = pos_middle.y + (height / 2) * pixel_per_mandelbrot;
    //std::cout << "bottom_left: " << glm::to_string(bottom_left) << std::endl;
    //std::cout << "top_right: " << glm::to_string(top_right) << std::endl;
    update_quad(bottom_left, top_right);
}

void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    width = w;
    height = h;
    glViewport(0, 0, width, height);
    update_mandelbrot_pos();
}

void scroll_callback(GLFWwindow*, double xoffset, double yoffset) {
    if (yoffset > 0)
        pixel_per_mandelbrot /= 1.5;
    else
        pixel_per_mandelbrot *= 1.5;
    update_mandelbrot_pos();
}

glm::vec2 cursor_pos() {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    return glm::vec2(x, y);
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

    init_shader();
    init_quad();
    update_mandelbrot_pos();

    glm::vec2 last_cursor_pos = cursor_pos();
    glm::vec2 cursor_pos_offset = glm::vec2(0);

    // Mainloop
    while (!glfwWindowShouldClose(window)) {
        cursor_pos_offset = cursor_pos() - last_cursor_pos;
        last_cursor_pos = cursor_pos();
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            pos_middle.x -= cursor_pos_offset.x * pixel_per_mandelbrot;
            pos_middle.y += cursor_pos_offset.y * pixel_per_mandelbrot;
            update_mandelbrot_pos();
        }

        render_quad();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}
