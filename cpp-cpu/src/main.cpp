#include "wayland.hpp"

#include "../vendor/font8x8_basic.h"
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <immintrin.h>
#include <iostream>
#include <map>
#include <optional>
#include <queue>
#include <span>
#include <thread>
#include <unordered_map>
#include <vector>

// TODO: Smooth shading: https://linas.org/art-gallery/escape/smooth.html
// TODO: Anti-Aliasing
// TODO: Vulkan compute: https://bakedbits.dev/posts/vulkan-compute-example
// TODO: wayland: use wp_cursor_shape_manager_v1 instead of wayland-cursor

struct Color {
    union {
        uint32_t color;
        struct {
            uint8_t b;
            uint8_t g;
            uint8_t r;
            uint8_t a;
        };
    };

    Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0)
        : b{b}
        , g{g}
        , r{r}
        , a{255}
    { }

    bool operator==(Color const& other) const
    {
        return color == other.color;
    }
};

// Parameters
int64_t constexpr chunk_size = 32 * 8;
int64_t max_iterations = 1000;
bool constexpr use_avx = true;
int32_t constexpr thread_count = 8;
int32_t constexpr max_queue_size = thread_count;
std::size_t constexpr max_chunk_memory = 1024 * 1024 * 1024; // 1GiB
std::size_t constexpr color_function = 1; // 0: black_white, 1: hsl
Color const default_color{100, 100, 100};
int64_t constexpr text_scale = 2;
uint32_t const message_display_duration = 4000; // ms

// Global variables
bool info_text_visible = true;
bool help_text_visible = true;
uint32_t last_message_time = 0;
std::string last_message;
uint32_t global_time = 0;

std::size_t frame_number = 0;

struct QOIImage {
    static int encode_to_file(char const* filepath, Color* data, int width, int height)
    {
        FILE* out_file = fopen(filepath, "w");
        if (!out_file) {
            return 0;
        }

        struct Header header = {
            .magic = {'q', 'o', 'i', 'f'},
            .width = htobe32(width),
            .height = htobe32(height),
            .channels = 3,
            .colorspace = 0,
        };

        fwrite(&header, 1, 14, out_file);

        uint8_t op_buffer[5];
        Color prev_pixels[64];
        for (auto& pixel : prev_pixels) {
            pixel.color = 0;
        }
        Color last_pixel;

        int run_length = 0;
        for (int pixel_index = 0; pixel_index < width * height; ++pixel_index) {
            if (data[pixel_index] == last_pixel && run_length < 62 && pixel_index < width * height - 1) {
                ++run_length;
            } else {
                if (run_length > 0) {
                    // Generate QOI_OP_RUN
                    op_buffer[0] = 192 | (run_length - 1);
                    fwrite(&op_buffer, 1, 1, out_file);
                    run_length = 0;
                }

                if (data[pixel_index] == prev_pixels[index_position(data[pixel_index])]) {
                    // Generate QOI_OP_INDEX
                    op_buffer[0] = index_position(data[pixel_index]);
                    fwrite(&op_buffer, 1, 1, out_file);
                } else {
                    // Generate QOI_OP_RGB
                    op_buffer[0] = 254;
                    op_buffer[1] = data[pixel_index].r;
                    op_buffer[2] = data[pixel_index].g;
                    op_buffer[3] = data[pixel_index].b;
                    fwrite(&op_buffer, 1, 4, out_file);
                }
            }

            last_pixel = data[pixel_index];
            prev_pixels[index_position(data[pixel_index])] = data[pixel_index];
        }

        fclose(out_file);

        return 1;
    }

private:
    struct Header {
        char magic[4];
        uint32_t width;
        uint32_t height;
        uint8_t channels;
        uint8_t colorspace;
    };

    static uint8_t index_position(Color color)
    {
        return (color.r * 3 + color.g * 5 + color.b * 7 + color.a * 11) % 64;
    }
};

#define CONCAT(a, b) a##b

struct ScreenPosition {
    int64_t x;
    int64_t y;
};

struct Complex {
    double real;
    double imag;
};

struct ChunkGridPosition {
    int64_t real;
    int64_t imag;

    bool operator==(ChunkGridPosition const& other) const = default;
};

template <>
struct std::hash<ChunkGridPosition> {
    std::size_t operator()(ChunkGridPosition const& value) const
    {
        return (hash<int64_t>()(value.real) ^ hash<int64_t>()(value.imag));
    }
};

struct HSLColor {
    uint16_t hue; // 0-359
    uint8_t saturation; // 0-100
    uint8_t lightness; // 0-100

    [[nodiscard]] Color to_rgb() const
    {
        if (saturation == 0) {
            return Color{
                lightness,
                lightness,
                lightness,
            };
        }

        auto const h = std::clamp<uint16_t>(hue, 0, 359);
        auto const s = std::clamp<uint8_t>(saturation, 0, 100) / 100.0;
        auto const l = std::clamp<uint8_t>(lightness, 0, 100) / 100.0;

        auto const chroma = (1 - std::abs(2 * l - 1)) * s;
        auto const h1 = h / 60.0;
        auto const x = chroma * (1.0 - std::abs(std::fmod(h1, 2.0) - 1));

        auto const [r1, g1, b1] = ([&]() {
            switch (static_cast<int32_t>(std::floor(h1))) {
            case 0:
                return std::make_tuple(chroma, x, 0.0);
            case 1:
                return std::make_tuple(x, chroma, 0.0);
            case 2:
                return std::make_tuple(0.0, chroma, x);
            case 3:
                return std::make_tuple(0.0, x, chroma);
            case 4:
                return std::make_tuple(x, 0.0, chroma);
            case 5:
                return std::make_tuple(chroma, 0.0, x);
            default:
                return std::make_tuple(0.0, 0.0, 0.0);
            }
        })();

        auto const m = l - (chroma / 2.0);

        auto const r = r1 + m;
        auto const g = g1 + m;
        auto const b = b1 + m;

        return Color{
            static_cast<uint8_t>(r * 255),
            static_cast<uint8_t>(g * 255),
            static_cast<uint8_t>(b * 255),
        };
    }
};

struct Chunk;

struct Buffer {
    static Buffer init(int64_t width, int64_t height)
    {
        return Buffer{
            width,
            height,
            std::vector<int32_t>(width * height),
        };
    }

    Buffer(int64_t width, int64_t height, std::vector<int32_t> buffer)
        : m_width{width}
        , m_height{height}
        , m_buffer{buffer}
    { }

    void set(ScreenPosition position, Color color)
    {
        m_buffer[position.y * m_width + position.x] = color.color;
    }

    void set(int64_t position, Color color)
    {
        m_buffer[position] = color.color;
    }

    std::span<int32_t> buffer()
    {
        return m_buffer;
    }

    [[nodiscard]] int64_t width() const
    {
        return m_width;
    }

    [[nodiscard]] int64_t height() const
    {
        return m_height;
    }

    void resize(int64_t width, int64_t height)
    {
        m_width = width;
        m_height = height;
        m_buffer.resize(width * height);
    }

    void fill(Color color)
    {
        std::fill(m_buffer.begin(), m_buffer.end(), color.color);
    }

    void blit(Chunk const&, ScreenPosition);

private:
    int64_t m_width;
    int64_t m_height;
    std::vector<int32_t> m_buffer;
};

struct Chunk {
    static Chunk create(Complex position, double complex_size, int64_t max_iterations_local)
    {
        return Chunk{
            position,
            complex_size,
            max_iterations_local,
        };
    };

    static Chunk create_dummy()
    {
        return Chunk{};
    }

    void compute()
    {
        if (m_ready) {
            return;
        }

        if (use_avx) {
            compute_avx_double();
        } else {
            compute_double();
        }

        switch (color_function) {
        case 0:
            colorize_black_white();
            break;
        case 1:
            colorize_hsl();
            break;
        }

        m_ready = true;
    }

    [[nodiscard]] Color const* buffer() const
    {
        return m_buffer.data();
    }

    [[nodiscard]] bool is_ready() const
    {
        return m_ready;
    }

    [[nodiscard]] double complex_size() const
    {
        return m_complex_size;
    }

    void update_last_access_time()
    {
        m_last_access_time = frame_number;
    }

    [[nodiscard]] std::size_t last_access_time() const
    {
        return m_last_access_time;
    }

private:
    bool m_ready{false};
    Complex m_position{0, 0};
    double m_complex_size{0};
    std::array<Color, chunk_size * chunk_size> m_buffer;
    std::size_t m_last_access_time{0};
    int64_t m_max_iterations_local{0};

    Chunk(Complex position, double complex_size, int64_t max_iterations_local)
        : m_position{position}
        , m_complex_size{complex_size}
        , m_max_iterations_local{max_iterations_local}
    { }

    Chunk()
        : m_ready{true}
    {
        m_buffer.fill(default_color);
    }

    void compute_double()
    {
        Complex c = m_position;
        double const pixel_delta = m_complex_size / chunk_size;

        for (int64_t buffer_position = 0; buffer_position < static_cast<int64_t>(m_buffer.size()); ++buffer_position) {
            if (buffer_position > 0 && buffer_position % chunk_size == 0) {
                c.real = m_position.real;
                c.imag += pixel_delta;
            }

            Complex z = {0, 0};
            Complex z_tmp = {0, 0};

            int32_t iteration = 0;
            for (; iteration < m_max_iterations_local; ++iteration) {
                auto abs = z.real * z.real + z.imag * z.imag;
                if (abs >= 4)
                    break;
                z_tmp.real = z.real * z.real - z.imag * z.imag + c.real;
                z_tmp.imag = z.real * z.imag * 2 + c.imag;
                z = z_tmp;
            }
            m_buffer[buffer_position].color = iteration;

            c.real += pixel_delta;
        }
    }

    void compute_avx_double()
    {
        auto const pixel_delta_single = m_complex_size / chunk_size;

        auto const pixel_delta_imag = _mm256_set_pd(
            pixel_delta_single,
            pixel_delta_single,
            pixel_delta_single,
            pixel_delta_single);

        auto const pixel_delta_real = _mm256_set_pd(
            pixel_delta_single * 4,
            pixel_delta_single * 4,
            pixel_delta_single * 4,
            pixel_delta_single * 4);

        // Why do they have to be ordered like this?
        auto const c_real_start = _mm256_set_pd(
            m_position.real + pixel_delta_single * 3,
            m_position.real + pixel_delta_single * 2,
            m_position.real + pixel_delta_single * 1,
            m_position.real + pixel_delta_single * 0);

        auto c_real = c_real_start;

        auto c_imag = _mm256_set_pd(
            m_position.imag,
            m_position.imag,
            m_position.imag,
            m_position.imag);

        auto const const_0 = _mm256_set_pd(0, 0, 0, 0);
        auto const const_2 = _mm256_set_pd(2, 2, 2, 2);
        auto const const_4 = _mm256_set_pd(4, 4, 4, 4);

        {
            Color color_max_iterations;
            color_max_iterations.color = m_max_iterations_local;
            m_buffer.fill(color_max_iterations);
        }

        for (int64_t buffer_position = 0; buffer_position < static_cast<int64_t>(m_buffer.size()); buffer_position += 4) {
            if (buffer_position > 0 && buffer_position % chunk_size == 0) {
                c_real = c_real_start;
                c_imag = _mm256_add_pd(c_imag, pixel_delta_imag);
            }

            auto z_real = const_0;
            auto z_imag = const_0;
            auto z_tmp_real = const_0;
            auto z_tmp_imag = const_0;

            for (int32_t iteration = 0; iteration < m_max_iterations_local; ++iteration) {
                auto abs = _mm256_add_pd(_mm256_mul_pd(z_real, z_real), _mm256_mul_pd(z_imag, z_imag));
                auto comparison_mask = reinterpret_cast<__m256i>(_mm256_cmp_pd(abs, const_4, _CMP_GE_OS));
                int32_t done_count = 0;

#define CHECK_FIELD_64(N)                                                          \
    {                                                                              \
        auto CONCAT(field_is_done_, N) = _mm256_extract_epi64(comparison_mask, N); \
        if (CONCAT(field_is_done_, N)) {                                           \
            if (m_buffer[buffer_position + N].color == m_max_iterations_local) {   \
                m_buffer[buffer_position + N].color = iteration;                   \
            }                                                                      \
            ++done_count;                                                          \
        }                                                                          \
    }
                CHECK_FIELD_64(0)
                CHECK_FIELD_64(1)
                CHECK_FIELD_64(2)
                CHECK_FIELD_64(3)

                if (done_count == 4) {
                    break;
                }

                z_tmp_real = _mm256_add_pd(_mm256_sub_pd(_mm256_mul_pd(z_real, z_real), _mm256_mul_pd(z_imag, z_imag)), c_real);
                z_tmp_imag = _mm256_add_pd(_mm256_mul_pd(_mm256_mul_pd(z_real, z_imag), const_2), c_imag);
                z_real = z_tmp_real;
                z_imag = z_tmp_imag;
            }

            c_real = _mm256_add_pd(c_real, pixel_delta_real);
        }
    }

    void scale()
    {
        for (int32_t buffer_position = m_buffer.size() - 1; buffer_position > 0; --buffer_position) {
            auto target_x = buffer_position % chunk_size;
            auto target_y = buffer_position / chunk_size;
            auto source_x = target_x;
            auto source_y = target_y;
            auto source_buffer_position = source_x + source_y * chunk_size;
            m_buffer[buffer_position] = m_buffer[source_buffer_position];
        }
    }

    void colorize_black_white()
    {
        for (uint32_t buffer_position = 0; buffer_position < m_buffer.size(); ++buffer_position) {
            auto const iterations = m_buffer[buffer_position].color;
            if (iterations == m_max_iterations_local) {
                m_buffer[buffer_position] = Color{};
            } else {
                m_buffer[buffer_position] = Color{
                    255,
                    255,
                    255,
                };
            }
        }
    }

    void colorize_hsl()
    {
        for (uint32_t buffer_position = 0; buffer_position < m_buffer.size(); ++buffer_position) {
            auto const iterations = m_buffer[buffer_position].color;
            auto const iterations_ratio = static_cast<double>(iterations) / static_cast<double>(m_max_iterations_local);
            if (iterations == m_max_iterations_local) {
                m_buffer[buffer_position] = Color{};
            } else {
                m_buffer[buffer_position] = HSLColor{
                    100,
                    static_cast<uint8_t>(iterations_ratio * 100),
                    std::clamp<uint8_t>(static_cast<uint8_t>(iterations_ratio * 100), 20, 80),
                } /* wtf, clang-format? */
                                                .to_rgb();
            }
        }
    }
};

void Buffer::blit(Chunk const& chunk, ScreenPosition position)
{
    auto const buffer_col_start = std::clamp(position.y, 0l, m_height);
    auto const buffer_col_end = std::clamp(position.y + chunk_size, 0l, m_height);
    auto const col_height = buffer_col_end - buffer_col_start;
    auto const chunk_col_start = std::clamp(-position.y, 0l, chunk_size);

    auto const buffer_line_start = std::clamp(position.x, 0l, m_width);
    auto const buffer_line_end = std::clamp(position.x + chunk_size, 0l, m_width);
    auto line_width = buffer_line_end - buffer_line_start;
    auto const chunk_line_start = std::clamp(-position.x, 0l, chunk_size);

    if (col_height == 0 || line_width == 0) {
        return;
    }

    for (int64_t y = 0; y < col_height; ++y) {
        auto* dest = &m_buffer.data()[(buffer_col_start + y) * m_width + buffer_line_start];
        auto const* src = &chunk.buffer()[(chunk_col_start + y) * chunk_size + chunk_line_start];
        std::memcpy(dest, src, line_width * sizeof(Color));
    }
}

Complex screen_space_to_mandelbrot_space(ScreenPosition screen_position, double chunk_resolution)
{
    // chunk_resolution: width and height of a chunk in mandelbrot space
    // chunk_size: width and height of a chunk in screen space
    return Complex{
        .real = (chunk_resolution / chunk_size) * screen_position.x,
        .imag = (chunk_resolution / chunk_size) * screen_position.y,
    };
}

ScreenPosition mandelbrot_space_to_screen_space(Complex mandelbrot_position, double chunk_resolution)
{
    return ScreenPosition{
        .x = static_cast<int64_t>((chunk_size / chunk_resolution) * mandelbrot_position.real),
        .y = static_cast<int64_t>((chunk_size / chunk_resolution) * mandelbrot_position.imag),
    };
}

struct Mandelbrot {
    ScreenPosition top_left_global = ScreenPosition{-100, -100};
    int32_t zoom_level = 1;

    void render(Buffer& buffer)
    {
        auto const chunk_resolution = get_chunk_resolution();
        auto const top_left_mandelbrot_space = screen_space_to_mandelbrot_space(top_left_global, chunk_resolution);

        auto const chunk_x_count = static_cast<int32_t>(std::ceil(static_cast<double>(buffer.width()) / chunk_size)) + 1;
        auto const chunk_y_count = static_cast<int32_t>(std::ceil(static_cast<double>(buffer.height()) / chunk_size)) + 1;

        // Grid starts at 0+0i, with step width of chunk_resolution
        auto const top_left_chunk_position = ChunkGridPosition{
            static_cast<int64_t>(std::floor(top_left_mandelbrot_space.real / chunk_resolution)),
            static_cast<int64_t>(std::floor(top_left_mandelbrot_space.imag / chunk_resolution)),
        };

        auto const top_left_chunk_global_screen_position = ScreenPosition{
            .x = top_left_chunk_position.real * chunk_size,
            .y = top_left_chunk_position.imag * chunk_size,
        };

        auto const top_left_local_screen_chunk_offset = ScreenPosition{
            .x = top_left_chunk_global_screen_position.x - top_left_global.x,
            .y = top_left_chunk_global_screen_position.y - top_left_global.y,
        };

        for (auto chunk_grid_x = 0; chunk_grid_x < chunk_x_count; ++chunk_grid_x) {
            for (auto chunk_grid_y = 0; chunk_grid_y < chunk_y_count; ++chunk_grid_y) {
                auto const chunk_grid_position = ChunkGridPosition{
                    .real = top_left_chunk_position.real + chunk_grid_x,
                    .imag = top_left_chunk_position.imag + chunk_grid_y,
                };

                auto const local_screen_chunk_offset = ScreenPosition{
                    .x = top_left_local_screen_chunk_offset.x + chunk_grid_x * chunk_size,
                    .y = top_left_local_screen_chunk_offset.y + chunk_grid_y * chunk_size,
                };

                auto* chunk = get_or_create_chunk(chunk_resolution, chunk_grid_position);
                if (chunk && chunk->is_ready()) {
                    chunk->update_last_access_time();
                    buffer.blit(*chunk, local_screen_chunk_offset);
                } else {
                    buffer.blit(dummy_chunk, local_screen_chunk_offset);
                }
            }
        }
    }

    double get_chunk_resolution()
    {
        return 2 * std::pow(0.9, zoom_level);
    }

    void create_thread_pool()
    {
        for (int32_t i = 0; i < thread_count; ++i) {
            m_threads.emplace_back([&]() {
                while (m_threads_running) {
                    std::unique_lock<std::mutex> queue_lock{m_queue_mutex};
                    m_queue_convar.wait(queue_lock, [&]() { return !m_chunk_queue.empty() || !m_threads_running; });

                    if (!m_chunk_queue.empty()) {
                        auto chunk = m_chunk_queue.front();
                        m_chunk_queue.pop();
                        queue_lock.unlock();
                        chunk.get().compute();
                    }
                }
            });
        }
    }

    void destroy_thread_pool()
    {
        m_threads_running = false;
        m_queue_convar.notify_all();
        for (auto& thread : m_threads) {
            thread.join();
        }
        m_threads.clear();
    }

    void invalidate_cache()
    {
        auto const single_chunk_memory = chunk_size * chunk_size * sizeof(Color);

        std::size_t cache_memory = m_chunks.size() * single_chunk_memory;

        if (cache_memory <= max_chunk_memory) {
            return;
        }

        auto const memory_to_delete = cache_memory - max_chunk_memory;
        auto const chunk_amount_to_delete = memory_to_delete / single_chunk_memory;

        std::cout << "Removing " << chunk_amount_to_delete << " chunks\n";

        std::vector<ChunkCacheListItem> chunks;
        for (auto const& [identifier, chunk] : m_chunks) {
            if (chunk.is_ready())
                chunks.push_back(ChunkCacheListItem{identifier, &chunk});
        }
        std::sort(std::begin(chunks), std::end(chunks), [](auto const& lhs, auto const& rhs) -> bool {
            return lhs.chunk->last_access_time() < rhs.chunk->last_access_time();
        });

        for (std::size_t i = 0; i < chunk_amount_to_delete; ++i) {
            auto to_delete = chunks.at(i);
            if (!to_delete.chunk->is_ready()) {
                --i;
                continue;
            }
            m_chunks.erase(to_delete.identifier);
        }
    }

private:
    struct ChunkIdentifier {
        double chunk_resolution;
        ChunkGridPosition chunk_grid_position;
        int64_t max_iterations;

        bool operator==(ChunkIdentifier const& other) const = default;
    };

    struct HashChunkIdentifier {
        std::size_t operator()(ChunkIdentifier const& id) const
        {
            return ((std::hash<double>()(id.chunk_resolution)
                        ^ (std::hash<ChunkGridPosition>()(id.chunk_grid_position) << 1))
                       >> 1)
                ^ (std::hash<int64_t>()(id.max_iterations) << 1);
        }
    };

    struct ChunkCacheListItem {
        ChunkIdentifier identifier;
        Chunk const* chunk;
    };

    std::unordered_map<ChunkIdentifier, Chunk, HashChunkIdentifier> m_chunks;

    std::queue<std::reference_wrapper<Chunk>> m_chunk_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_convar;
    std::vector<std::thread> m_threads;
    bool m_threads_running{true};
    Chunk const dummy_chunk = Chunk::create_dummy();

    Chunk* get_or_create_chunk(double chunk_resolution, ChunkGridPosition position)
    {
        auto chunk_identifier = ChunkIdentifier{
            .chunk_resolution = chunk_resolution,
            .chunk_grid_position = position,
            .max_iterations = max_iterations,
        };

        if (m_chunks.contains(chunk_identifier) && m_chunks.at(chunk_identifier).is_ready()) {
            return &m_chunks.at(chunk_identifier);
        }

        enqueue_chunk(chunk_identifier);
        return nullptr;
    };

    bool enqueue_chunk(ChunkIdentifier identifier)
    {
        if (m_chunk_queue.size() > max_queue_size) {
            return false;
        }

        if (m_chunks.contains(identifier)) {
            return false;
        }

        auto const complex_chunk_position = Complex{
            .real = identifier.chunk_grid_position.real * identifier.chunk_resolution,
            .imag = identifier.chunk_grid_position.imag * identifier.chunk_resolution,
        };

        m_chunks.insert(std::make_pair(identifier, Chunk::create(complex_chunk_position, identifier.chunk_resolution, identifier.max_iterations)));

        auto& new_chunk = m_chunks.at(identifier);
        {
            std::lock_guard<std::mutex> lock{m_queue_mutex};
            m_chunk_queue.push(new_chunk);
        }
        m_queue_convar.notify_one();
        return true;
    }
};

void render_text_to_buffer(Buffer* buffer, ScreenPosition position, std::string_view text)
{
    int64_t const advance = 8 * text_scale;
    unsigned char const fallback_glyph[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    for (size_t n = 0; n < text.length(); ++n) {
        uint8_t character = text[n];
        auto const* glyph = character < 128 ? font8x8_basic[character] : fallback_glyph;
        for (int64_t y = 0; y < 8; ++y) {
            auto const glyph_row = glyph[y];
            for (int64_t x = 0; x < 8; ++x) {
                if ((glyph_row >> x) & 1) {
                    for (int64_t i = 0; i < text_scale * text_scale; ++i) {
                        auto pixel_position = ScreenPosition{
                            .x = position.x + static_cast<int64_t>(n) * advance + x * text_scale + i % text_scale,
                            .y = position.y + y * text_scale + i / text_scale,
                        };

                        buffer->set(pixel_position, Color(255, 255, 255));
                    }
                }
            }
        }
    }
}

std::optional<Buffer> buffer = {};

auto mandelbrot = Mandelbrot{};

auto cursor_position = ScreenPosition{0, 0};
auto cursor_start_local_position = ScreenPosition{0, 0};
auto cursor_start_global_position = ScreenPosition{0, 0};
auto lmb_pressed = false;

int main()
{
    auto window = Window::open("Mandelbrot", 600, 500);

    buffer = Buffer::init(800, 600);

    window->callback_window_resize = [](int width, int height) {
        buffer->resize(width, height);
    };

    window->callback_pointer_motion = [](int x, int y) {
        cursor_position.x = x / 250;
        cursor_position.y = y / 250;

        if (!lmb_pressed) {
            return;
        }

        auto const local_offset = ScreenPosition{
            cursor_position.x - cursor_start_local_position.x,
            cursor_position.y - cursor_start_local_position.y,
        };

        mandelbrot.top_left_global.x = cursor_start_global_position.x - local_offset.x;
        mandelbrot.top_left_global.y = cursor_start_global_position.y - local_offset.y;
    };

    window->callback_pointer_button = [](uint32_t button, wl_pointer_button_state state) {
        if (button != BTN_LEFT) {
            return;
        }

        auto const is_pressed = state == WL_POINTER_BUTTON_STATE_PRESSED;

        if (!lmb_pressed && is_pressed) {
            cursor_start_local_position.x = cursor_position.x;
            cursor_start_local_position.y = cursor_position.y;
            cursor_start_global_position = mandelbrot.top_left_global;
        }

        lmb_pressed = is_pressed;
    };

    window->callback_pointer_axis = [](wl_pointer_axis axis, int value) {
        if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) {
            return;
        }

        value = -value / 500;

        auto const cursor_position_local_screen_space = ScreenPosition{
            .x = cursor_position.x,
            .y = cursor_position.y,
        };

        auto const cursor_position_global_screen_space = ScreenPosition{
            .x = mandelbrot.top_left_global.x + cursor_position_local_screen_space.x,
            .y = mandelbrot.top_left_global.y + cursor_position_local_screen_space.y,
        };

        auto const cursor_position_mandelbrot_space = screen_space_to_mandelbrot_space(cursor_position_global_screen_space, mandelbrot.get_chunk_resolution());

        mandelbrot.zoom_level = std::max<int32_t>(mandelbrot.zoom_level + value, 1);

        auto const new_cursor_position_global_screen_space = mandelbrot_space_to_screen_space(cursor_position_mandelbrot_space, mandelbrot.get_chunk_resolution());

        mandelbrot.top_left_global.x = new_cursor_position_global_screen_space.x - cursor_position_local_screen_space.x;
        mandelbrot.top_left_global.y = new_cursor_position_global_screen_space.y - cursor_position_local_screen_space.y;
    };

    window->callback_keyboard_key = [](Scancodes scancode, wl_keyboard_key_state state) {
        if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
            return;
        }

        switch (scancode) {
        case Scancodes::S:
            if (buffer.has_value()) {
                std::string filename;
                for (int image_number = 0;; ++image_number) {
                    filename = "mandelbrot-" + std::to_string(image_number) + ".qoi";
                    if (!std::filesystem::exists(filename)) {
                        break;
                    }
                }
                QOIImage::encode_to_file(filename.c_str(), reinterpret_cast<Color*>(buffer->buffer().data()), buffer->width(), buffer->height());
                last_message = "Saved screenshot to " + std::filesystem::current_path().string() + "/" + filename;
                last_message_time = global_time;
            }
            break;
        case Scancodes::I:
            info_text_visible = !info_text_visible;
            break;
        case Scancodes::PLUS:
            max_iterations += 50;
            break;
        case Scancodes::H:
            help_text_visible = !help_text_visible;
            break;
        case Scancodes::MINUS:
            max_iterations -= 50;
            break;
        default:
            std::cout << "Scancode: " << std::to_string(static_cast<uint32_t>(scancode)) << "\n";
        }
    };

    mandelbrot.create_thread_pool();

    window->callback_draw = [](uint32_t* data, [[maybe_unused]] int width, [[maybe_unused]] int height, uint32_t time) {
        global_time = time;
        mandelbrot.render(*buffer);

        mandelbrot.invalidate_cache();

        int line = 0;
        auto render_next_line = [&](auto text) {
            auto position = ScreenPosition{
                .x = 10,
                .y = 10 + line++ * 8 * text_scale,
            };
            render_text_to_buffer(&*buffer, position, text);
        };

        if (info_text_visible) {
            render_next_line("max iterations: " + std::to_string(max_iterations));
            render_next_line("zoom: " + std::to_string(mandelbrot.zoom_level));
            auto const top_left_mandelbrot_space = screen_space_to_mandelbrot_space(mandelbrot.top_left_global, mandelbrot.get_chunk_resolution());
            render_next_line("mandelbrot real: " + std::to_string(top_left_mandelbrot_space.real));
            render_next_line("mandelbrot imag: " + std::to_string(top_left_mandelbrot_space.imag));
            ++line;
        }

        if (help_text_visible) {
            render_next_line("Keybindings:");
            render_next_line("H: Toggle this help text");
            render_next_line("I: Toggle informations");
            render_next_line("S: Screenshot");
            render_next_line("+: Increase max iterations");
            render_next_line("-: Decrease max iterations");
            ++line;
        }

        if (last_message_time + message_display_duration >= time) {
            render_next_line(last_message);
        }

        memcpy(data, reinterpret_cast<uint32_t*>(buffer->buffer().data()), buffer->buffer().size() * 4);

        ++frame_number;
    };

    window->mainloop();

    mandelbrot.destroy_thread_pool();
}
