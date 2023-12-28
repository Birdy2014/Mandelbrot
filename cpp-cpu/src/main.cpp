#include <MiniFB.h>
#include <cassert>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <immintrin.h>
#include <iostream>
#include <map>
#include <optional>
#include <queue>
#include <span>
#include <thread>
#include <vector>

// Parameters
static int32_t constexpr chunk_size = 32 * 8;
static int32_t constexpr max_iterations = 1000;
static bool constexpr use_avx2 = true;
static int32_t constexpr thread_count = 8;
static int32_t constexpr max_queue_size = thread_count;
static std::size_t constexpr max_chunk_memory = 1024 * 1024 * 1024; // 1GiB
static std::size_t constexpr color_function = 1; // 0: black_white, 1: hsl

std::size_t frame_number = 0;

#define CONCAT(a, b) a##b

struct ScreenPosition {
    int32_t x;
    int32_t y;
};

struct Complex {
    double real;
    double imag;
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

        auto const h = std::clamp<uint16_t>(hue, 0, 359) / 100.0;
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

    void resize(int32_t width, int32_t height)
    {
        m_width = width;
        m_height = height;
        m_buffer.resize(width * height);
    }

    void blit(Chunk const&, ScreenPosition);

private:
    int32_t m_width;
    int32_t m_height;
    std::vector<int32_t> m_buffer;
};

struct Chunk {
    static Chunk create(Complex position, double complex_size)
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

        if (use_avx2) {
            compute_avx2_double();
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
    Complex m_position;
    double m_complex_size;
    std::array<Color, chunk_size * chunk_size> m_buffer;
    std::size_t m_last_access_time{0};

    Chunk(Complex position, double complex_size)
        : m_position{position}
        , m_complex_size{complex_size}
    { }

    void compute_double()
    {
        Complex c = m_position;
        double const pixel_delta = m_complex_size / chunk_size;

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

    void compute_avx2_double()
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
            color_max_iterations.color = max_iterations;
            m_buffer.fill(color_max_iterations);
        }

        for (int32_t buffer_position = 0; buffer_position < static_cast<int32_t>(m_buffer.size()); buffer_position += 4) {
            if (buffer_position > 0 && buffer_position % chunk_size == 0) {
                c_real = c_real_start;
                c_imag = _mm256_add_pd(c_imag, pixel_delta_imag);
            }

            auto z_real = const_0;
            auto z_imag = const_0;
            auto z_tmp_real = const_0;
            auto z_tmp_imag = const_0;

            for (int32_t iteration = 0; iteration < max_iterations; ++iteration) {
                auto abs = _mm256_add_pd(_mm256_mul_pd(z_real, z_real), _mm256_mul_pd(z_imag, z_imag));
                auto comparison_mask = _mm256_cmp_pd(abs, const_4, _CMP_GE_OS);
                if (!_mm256_testz_si256(comparison_mask, comparison_mask)) {
                    int32_t done_count = 0;

#define CHECK_FIELD_64(N)                                                          \
    {                                                                              \
        auto CONCAT(field_is_done_, N) = _mm256_extract_epi64(comparison_mask, N); \
        if (CONCAT(field_is_done_, N)) {                                           \
            if (m_buffer[buffer_position + N].color == max_iterations) {           \
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
                }

                z_tmp_real = _mm256_add_pd(_mm256_sub_pd(_mm256_mul_pd(z_real, z_real), _mm256_mul_pd(z_imag, z_imag)), c_real);
                z_tmp_imag = _mm256_add_pd(_mm256_mul_pd(_mm256_mul_pd(z_real, z_imag), const_2), c_imag);
                z_real = z_tmp_real;
                z_imag = z_tmp_imag;
            }

            c_real = _mm256_add_pd(c_real, pixel_delta_real);
        }
    }

    void colorize_black_white()
    {
        for (uint32_t buffer_position = 0; buffer_position < m_buffer.size(); ++buffer_position) {
            auto const iterations = m_buffer[buffer_position].color;
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
    }

    void colorize_hsl()
    {
        for (uint32_t buffer_position = 0; buffer_position < m_buffer.size(); ++buffer_position) {
            auto const iterations = m_buffer[buffer_position].color;
            auto const iterations_ratio = static_cast<double>(iterations) / static_cast<double>(max_iterations);
            if (iterations == max_iterations) {
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
        .x = static_cast<int32_t>((chunk_size / chunk_resolution) * mandelbrot_position.real),
        .y = static_cast<int32_t>((chunk_size / chunk_resolution) * mandelbrot_position.imag),
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

                auto* chunk = get_or_create_chunk(chunk_resolution, chunk_grid_position);
                if (chunk && chunk->is_ready()) {
                    chunk->update_last_access_time();
                    buffer.blit(*chunk, local_screen_chunk_offset);
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

    struct ChunkCacheListItem {
        Chunk const* chunk;
        double resolution;
        ChunkGridPosition position;
    };

    void invalidate_cache()
    {
        auto const single_chunk_memory = chunk_size * chunk_size * sizeof(Color);

        std::size_t cache_memory = 0;
        for (auto const& [resolution, chunks_at_res] : m_chunks) {
            cache_memory += chunks_at_res.size() * single_chunk_memory;
        }

        if (cache_memory <= max_chunk_memory) {
            return;
        }

        auto const memory_to_delete = cache_memory - max_chunk_memory;
        auto const chunk_amount_to_delete = memory_to_delete / single_chunk_memory;

        std::cout << "Removing " << chunk_amount_to_delete << " chunks\n";

        std::vector<ChunkCacheListItem> chunks;
        for (auto const& [resolution, chunks_at_res] : m_chunks) {
            for (auto const& [position, chunk] : chunks_at_res) {
                if (chunk.is_ready())
                    chunks.push_back(ChunkCacheListItem{&chunk, resolution, position});
            }
        }
        std::sort(std::begin(chunks), std::end(chunks), [](auto const& lhs, auto const& rhs) -> bool {
            return lhs.chunk->last_access_time() < rhs.chunk->last_access_time();
        });

        for (std::size_t i = 0; i < chunk_amount_to_delete; ++i) {
            auto to_delete = chunks.at(i);
            m_chunks[to_delete.resolution].erase(to_delete.position);
        }
    }

private:
    std::map<double, std::unordered_map<ChunkGridPosition, Chunk>> m_chunks;

    std::queue<std::reference_wrapper<Chunk>> m_chunk_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_convar;
    std::vector<std::thread> m_threads;
    bool m_threads_running{true};

    Chunk* get_or_create_chunk(double chunk_resolution, ChunkGridPosition position)
    {
        auto& chunks_at_res = m_chunks[chunk_resolution];
        if (!chunks_at_res.contains(position)) {
            if (m_chunk_queue.size() <= max_queue_size) {
                auto const complex_chunk_position = Complex{
                    .real = position.real * chunk_resolution,
                    .imag = position.imag * chunk_resolution,
                };
                chunks_at_res.insert(std::make_pair(position, Chunk::create(complex_chunk_position, chunk_resolution)));

                auto& new_chunk = chunks_at_res.at(position);
                {
                    std::lock_guard<std::mutex> lock{m_queue_mutex};
                    m_chunk_queue.push(new_chunk);
                }
                m_queue_convar.notify_one();
            }
        }
        if (chunks_at_res.contains(position))
            return &chunks_at_res.at(position);
        return nullptr;
    };
};

std::optional<Buffer> buffer = {};

auto mandelbrot = Mandelbrot{};

auto cursor_start_local_position = ScreenPosition{0, 0};
auto cursor_start_global_position = ScreenPosition{0, 0};
auto lmb_pressed = false;

int main()
{
    struct mfb_window* window = mfb_open_ex("Mandelbrot", 800, 600, WF_RESIZABLE);
    if (!window)
        return 0;

    mfb_set_resize_callback(window, [](mfb_window* window, int width, int height) {
        mfb_set_viewport(window, 0, 0, width, height);
        buffer->resize(width, height);
    });

    // FIXME: Sometimes to view jumps while zooming or panning

    mfb_set_mouse_button_callback(window, [](struct mfb_window* window, mfb_mouse_button button, mfb_key_mod, bool is_pressed) {
        if (button != MOUSE_BTN_1) {
            return;
        }

        if (!lmb_pressed && is_pressed) {
            cursor_start_local_position.x = mfb_get_mouse_x(window);
            cursor_start_local_position.y = mfb_get_mouse_y(window);
            cursor_start_global_position = mandelbrot.top_left_global;
        }

        lmb_pressed = is_pressed;
    });

    mfb_set_mouse_move_callback(window, [](struct mfb_window*, int x, int y) {
        if (!lmb_pressed) {
            return;
        }

        auto const local_offset = ScreenPosition{
            x - cursor_start_local_position.x,
            y - cursor_start_local_position.y,
        };

        mandelbrot.top_left_global.x = cursor_start_global_position.x - local_offset.x;
        mandelbrot.top_left_global.y = cursor_start_global_position.y - local_offset.y;
    });

    mfb_set_mouse_scroll_callback(window, [](struct mfb_window* window, mfb_key_mod, [[maybe_unused]] float delta_x, float delta_y) {
        auto const cursor_position_local_screen_space = ScreenPosition{
            .x = mfb_get_mouse_x(window),
            .y = mfb_get_mouse_y(window),
        };

        auto const cursor_position_global_screen_space = ScreenPosition{
            .x = mandelbrot.top_left_global.x + cursor_position_local_screen_space.x,
            .y = mandelbrot.top_left_global.y + cursor_position_local_screen_space.y,
        };

        auto const cursor_position_mandelbrot_space = screen_space_to_mandelbrot_space(cursor_position_global_screen_space, mandelbrot.get_chunk_resolution());

        mandelbrot.zoom_level = std::max<int32_t>(mandelbrot.zoom_level + delta_y, 1);

        auto const new_cursor_position_global_screen_space = mandelbrot_space_to_screen_space(cursor_position_mandelbrot_space, mandelbrot.get_chunk_resolution());

        mandelbrot.top_left_global.x = new_cursor_position_global_screen_space.x - cursor_position_local_screen_space.x;
        mandelbrot.top_left_global.y = new_cursor_position_global_screen_space.y - cursor_position_local_screen_space.y;
    });

    int state;

    buffer = Buffer::init(800, 600);

    mandelbrot.create_thread_pool();

    do {
        mandelbrot.render(*buffer);

        mandelbrot.invalidate_cache();

        state = mfb_update_ex(window, buffer->buffer().data(), buffer->width(), buffer->height());

        if (state < 0) {
            window = nullptr;
            break;
        }

        ++frame_number;
    } while (mfb_wait_sync(window));

    mandelbrot.destroy_thread_pool();
}
