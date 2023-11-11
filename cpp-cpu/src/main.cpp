#include <MiniFB.h>
#include <cassert>
#include <cmath>
#include <cstring>
#include <immintrin.h>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <vector>

// Parameters
typedef float mandelbrot_float_t;
static int32_t constexpr chunk_size = 64 * 8;
static int32_t constexpr max_iterations = 100;
static bool constexpr enable_avx2 = true;

struct ScreenPosition {
    int32_t x;
    int32_t y;
};

struct Complex {
    mandelbrot_float_t real;
    mandelbrot_float_t imag;
};

struct ChunkGridPosition {
    int32_t real;
    int32_t imag;

    bool operator==(ChunkGridPosition const& other) const = default;
};

template <>
struct std::hash<ChunkGridPosition> {
    std::size_t operator()(ChunkGridPosition const& value) const
    {
        return (hash<int32_t>()(value.real) ^ hash<int32_t>()(value.imag));
    }
};

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
        , a{0}
    { }
};

struct Chunk;

struct Buffer {
    static Buffer init(int32_t width, int32_t height)
    {
        return Buffer{
            width,
            height,
            std::vector<int32_t>(width * height),
        };
    }

    Buffer(int32_t width, int32_t height, std::vector<int32_t> buffer)
        : m_width{width}
        , m_height{height}
        , m_buffer{buffer}
    { }

    void set(ScreenPosition position, Color color)
    {
        m_buffer[position.y * m_width + position.x] = color.color;
    }

    void set(int32_t position, Color color)
    {
        m_buffer[position] = color.color;
    }

    std::span<int32_t> buffer()
    {
        return m_buffer;
    }

    [[nodiscard]] int32_t width() const
    {
        return m_width;
    }

    [[nodiscard]] int32_t height() const
    {
        return m_height;
    }

    void blit(Chunk const&, ScreenPosition);

private:
    int32_t m_width;
    int32_t m_height;
    std::vector<int32_t> m_buffer;
};

struct Chunk {
    static Chunk create(Complex position, mandelbrot_float_t complex_size)
    {
        return Chunk{
            position,
            complex_size};
    };

    void compute()
    {
        if (m_ready) {
            return;
        }

        if (enable_avx2) {
            compute_avx2();
        } else {
            compute_normal();
        }

        for (int32_t buffer_position = 0; buffer_position < static_cast<int32_t>(m_buffer.size()); ++buffer_position) {
            auto iterations = m_buffer[buffer_position].color;
            if (iterations == max_iterations) {
                m_buffer[buffer_position] = Color{};
            } else {
                m_buffer[buffer_position] = Color{
                    255,
                    255,
                    255,
                };
            }
        }

        m_ready = true;
    }

    [[nodiscard]] Color const* buffer() const
    {
        return m_buffer.data();
    }

private:
    bool m_ready{false};
    Complex m_position;
    mandelbrot_float_t m_complex_size;
    std::array<Color, chunk_size * chunk_size> m_buffer;

    Chunk(Complex position, mandelbrot_float_t complex_size)
        : m_position{position}
        , m_complex_size{complex_size}
    { }

    void compute_normal()
    {
        Complex c = m_position;
        mandelbrot_float_t const pixel_delta = m_complex_size / chunk_size;

        for (int32_t buffer_position = 0; buffer_position < static_cast<int32_t>(m_buffer.size()); ++buffer_position) {
            if (buffer_position > 0 && buffer_position % chunk_size == 0) {
                c.real = m_position.real;
                c.imag += pixel_delta;
            }

            Complex z = {0, 0};
            Complex z_tmp = {0, 0};

            int32_t iteration = 0;
            for (; iteration < max_iterations; ++iteration) {
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

    void compute_avx2()
    {
        assert(sizeof(mandelbrot_float_t) == 4);

        mandelbrot_float_t const pixel_delta_single = m_complex_size / chunk_size;

        auto const pixel_delta_imag = _mm256_set_ps(
            pixel_delta_single,
            pixel_delta_single,
            pixel_delta_single,
            pixel_delta_single,
            pixel_delta_single,
            pixel_delta_single,
            pixel_delta_single,
            pixel_delta_single);

        auto const pixel_delta_real = _mm256_set_ps(
            pixel_delta_single * 8,
            pixel_delta_single * 8,
            pixel_delta_single * 8,
            pixel_delta_single * 8,
            pixel_delta_single * 8,
            pixel_delta_single * 8,
            pixel_delta_single * 8,
            pixel_delta_single * 8);

        // Why do they have to be ordered like this?
        auto const c_real_start = _mm256_set_ps(
            m_position.real + pixel_delta_single * 7,
            m_position.real + pixel_delta_single * 6,
            m_position.real + pixel_delta_single * 5,
            m_position.real + pixel_delta_single * 4,
            m_position.real + pixel_delta_single * 3,
            m_position.real + pixel_delta_single * 2,
            m_position.real + pixel_delta_single * 1,
            m_position.real + pixel_delta_single * 0);

        auto c_real = c_real_start;

        auto c_imag = _mm256_set_ps(
            m_position.imag,
            m_position.imag,
            m_position.imag,
            m_position.imag,
            m_position.imag,
            m_position.imag,
            m_position.imag,
            m_position.imag);

        auto const const_0 = _mm256_set_ps(0, 0, 0, 0, 0, 0, 0, 0);
        auto const const_2 = _mm256_set_ps(2, 2, 2, 2, 2, 2, 2, 2);
        auto const const_4 = _mm256_set_ps(4, 4, 4, 4, 4, 4, 4, 4);

        {
            Color color_max_iterations;
            color_max_iterations.color = max_iterations;
            m_buffer.fill(color_max_iterations);
        }

        for (int32_t buffer_position = 0; buffer_position < static_cast<int32_t>(m_buffer.size()); buffer_position += 8) {
            if (buffer_position > 0 && buffer_position % chunk_size == 0) {
                c_real = c_real_start;
                c_imag = _mm256_add_ps(c_imag, pixel_delta_imag);
            }

            auto z_real = const_0;
            auto z_imag = const_0;
            auto z_tmp_real = const_0;
            auto z_tmp_imag = const_0;

            int32_t iteration = 0;
            for (; iteration < max_iterations; ++iteration) {
                auto abs = _mm256_add_ps(_mm256_mul_ps(z_real, z_real), _mm256_mul_ps(z_imag, z_imag));
                auto comparison_mask = _mm256_cmp_ps(abs, const_4, _CMP_GE_OS);
                if (!_mm256_testz_si256(comparison_mask, comparison_mask)) {
                    int32_t done_count = 0;

#define CONCAT(a, b) a##b

#define CHECK_FIELD(N)                                                             \
    {                                                                              \
        auto CONCAT(field_is_done_, N) = _mm256_extract_epi32(comparison_mask, N); \
        if (CONCAT(field_is_done_, N)) {                                           \
            if (m_buffer[buffer_position + N].color == max_iterations) {           \
                m_buffer[buffer_position + N].color = iteration;                   \
            }                                                                      \
            ++done_count;                                                          \
        }                                                                          \
    }
                    CHECK_FIELD(0)
                    CHECK_FIELD(1)
                    CHECK_FIELD(2)
                    CHECK_FIELD(3)
                    CHECK_FIELD(4)
                    CHECK_FIELD(5)
                    CHECK_FIELD(6)
                    CHECK_FIELD(7)

                    if (done_count == 8) {
                        break;
                    }
                }

                z_tmp_real = _mm256_add_ps(_mm256_sub_ps(_mm256_mul_ps(z_real, z_real), _mm256_mul_ps(z_imag, z_imag)), c_real);
                z_tmp_imag = _mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(z_real, z_imag), const_2), c_imag);
                z_real = z_tmp_real;
                z_imag = z_tmp_imag;
            }

            c_real = _mm256_add_ps(c_real, pixel_delta_real);
        }
    }
};

void Buffer::blit(Chunk const& chunk, ScreenPosition position)
{
    auto const buffer_col_start = std::clamp(position.y, 0, m_height);
    auto const buffer_col_end = std::clamp(position.y + chunk_size, 0, m_height);
    auto const col_height = buffer_col_end - buffer_col_start;
    auto const chunk_col_start = std::clamp(-position.y, 0, chunk_size);

    auto const buffer_line_start = std::clamp(position.x, 0, m_width);
    auto const buffer_line_end = std::clamp(position.x + chunk_size, 0, m_width);
    auto line_width = buffer_line_end - buffer_line_start;
    auto const chunk_line_start = std::clamp(-position.x, 0, chunk_size);

    if (col_height == 0 || line_width == 0) {
        return;
    }

    for (int32_t y = 0; y < col_height; ++y) {
        auto* dest = &m_buffer.data()[(buffer_col_start + y) * m_width + buffer_line_start];
        auto const* src = &chunk.buffer()[(chunk_col_start + y) * chunk_size + chunk_line_start];
        std::memcpy(dest, src, line_width * sizeof(Color));
    }
}

Complex screen_space_to_mandelbrot_space(ScreenPosition screen_position, mandelbrot_float_t chunk_resolution)
{
    // chunk_resolution: width and height of a chunk in mandelbrot space
    // chunk_size: width and height of a chunk in screen space
    return Complex{
        .real = (chunk_resolution / chunk_size) * screen_position.x,
        .imag = (chunk_resolution / chunk_size) * screen_position.y,
    };
}

struct Mandelbrot {
    void render(Buffer& buffer, ScreenPosition top_left_global, mandelbrot_float_t chunk_resolution)
    {
        auto const top_left_mandelbrot_space = screen_space_to_mandelbrot_space(top_left_global, chunk_resolution);

        auto const chunk_x_count = static_cast<int32_t>(std::ceil(static_cast<double>(buffer.width()) / chunk_size)) + 1;
        auto const chunk_y_count = static_cast<int32_t>(std::ceil(static_cast<double>(buffer.height()) / chunk_size)) + 1;

        // Grid starts at 0+0i, with step width of chunk_resolution
        auto const top_left_chunk_position = ChunkGridPosition{
            static_cast<int32_t>(std::floor(top_left_mandelbrot_space.real / chunk_resolution)),
            static_cast<int32_t>(std::floor(top_left_mandelbrot_space.imag / chunk_resolution)),
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

                auto& chunk = get_or_create_chunk(chunk_resolution, chunk_grid_position);
                buffer.blit(chunk, local_screen_chunk_offset);
            }
        }
    }

private:
    std::map<mandelbrot_float_t, std::unordered_map<ChunkGridPosition, Chunk>> m_chunks;

    Chunk& get_or_create_chunk(mandelbrot_float_t chunk_resolution, ChunkGridPosition position)
    {
        auto& chunks_at_res = m_chunks[chunk_resolution];
        if (!chunks_at_res.contains(position)) {
            auto const complex_chunk_position = Complex{
                .real = position.real * chunk_resolution,
                .imag = position.imag * chunk_resolution,
            };
            chunks_at_res.insert(std::make_pair(position, Chunk::create(complex_chunk_position, chunk_resolution)));

            // TODO: Enqueue chunk and compute it asynchronously
            chunks_at_res.at(position).compute();
        }
        return chunks_at_res.at(position);
    };
};

std::optional<Buffer> buffer = {};

void resize_callback([[maybe_unused]] mfb_window* window, [[maybe_unused]] int width, [[maybe_unused]] int height)
{
    // TODO: Implement resize callback
}

int main()
{
    struct mfb_window* window = mfb_open_ex("Mandelbrot", 800, 600, WF_RESIZABLE);
    if (!window)
        return 0;

    mfb_set_resize_callback(window, resize_callback);
    int state;

    buffer = Buffer::init(800, 600);

    auto mandelbrot = Mandelbrot{};

    int x = -200;
    bool move_left = false;

    do {
        mandelbrot.render(*buffer, ScreenPosition{x, -200}, 2);
        if (move_left) {
            --x;
            if (x < -1000) {
                move_left = false;
            }
        } else {
            ++x;
            if (x > 200) {
                move_left = true;
            }
        }

        state = mfb_update_ex(window, buffer->buffer().data(), buffer->width(), buffer->height());

        if (state < 0) {
            window = nullptr;
            break;
        }
    } while (mfb_wait_sync(window));
}
