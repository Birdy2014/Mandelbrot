# Mandelbrot

These are just some experiments.

## C++ implementation

Requires a wayland compositor (except Mutter).

### Build and run using nix

```bash
nix run 'github:Birdy2014/Mandelbrot?dir=cpp-cpu'
```

### Build on other linux distros

Dependencies:
- cmake
- ninja
- pkg-config
- clang/gcc (tested with clang 18)
- libwayland
- wayland-scanner
- wayland-protocols

```bash
cd cpp-cpu
cmake -B build -G Ninja -D CMAKE_BUILD_TYPE=Release
cmake --build build
```
