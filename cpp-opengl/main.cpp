#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <fstream>
#include <string>

GLFWwindow* window;
GLuint vao, vbo;
GLuint shader;
glm::vec2 pos_middle = glm::vec2(0);
double pixel_per_mandelbrot = 0.003;

int width = 1290;
int height = 720;

std::string read_file(const std::string& path, bool convertEOL) {
    std::ifstream file_stream;
    std::string content;
    try {
        file_stream.open(path);
        content.assign(std::istreambuf_iterator<char>(file_stream), std::istreambuf_iterator<char>());
        if (convertEOL)
            content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
    } catch (std::ifstream::failure& e) {
        std::cerr << "Failed to read file " << path << std::endl;
    }
    return content;
}

void init_shader() {
    int success;
    char info_log[1024];
    std::string vertexShaderSource = read_file("mandelbrot.vert", true);
    const char* vsSource = vertexShaderSource.c_str();
    std::string fragmentShaderSource = read_file("mandelbrot.frag", true);
    const char* fsSource = fragmentShaderSource.c_str();
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vsSource, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 1024, nullptr, info_log);
        std::cerr << "shader compilation error: name: mandelbrot.vert\n" << info_log << std::endl;
        exit(1);
    }
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fsSource, NULL);
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
        // positions        // Mandelbrot Coords
        -1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f,  1.0f,  0.0f, 1.0f, 1.0f,
        1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
    };
    // setup plane VAO
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
}

void update_quad(glm::vec2 bottom_left, glm::vec2 top_right) {
    float quad_vertices[] = {
        // positions        // Mandelbrot Coords
        -1.0f, 1.0f,  0.0f, bottom_left.x, top_right.y,
        -1.0f, -1.0f, 0.0f, bottom_left.x, bottom_left.y,
        1.0f,  1.0f,  0.0f, top_right.x,   top_right.y,
        1.0f,  -1.0f, 0.0f, top_right.x,   bottom_left.y,
    };
    glBindVertexArray(vao);
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
        pixel_per_mandelbrot /= 2;
    else
        pixel_per_mandelbrot *= 2;
    update_mandelbrot_pos();
}

glm::vec2 cursor_pos() {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    return glm::vec2(x, y);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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
