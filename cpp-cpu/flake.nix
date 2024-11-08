{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs = { self, nixpkgs }:
    let pkgs = nixpkgs.legacyPackages.x86_64-linux;
    in rec {
      formatter.x86_64-linux = pkgs.nixfmt-classic;

      packages.x86_64-linux.default = pkgs.clang18Stdenv.mkDerivation {
        name = "Mandelbrot";
        src = ./.;
        nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
        buildInputs = with pkgs; [ wayland wayland-protocols wayland-scanner ];
      };

      devShells.x86_64-linux.default =
        packages.x86_64-linux.default.overrideAttrs {
          ASAN_OPTIONS = "symbolize=1";
          ASAN_SYMBOLIZER_PATH = "${pkgs.llvm}/bin/llvm-symbolizer";
          CMAKE_BUILD_TYPE = "Debug";
          hardeningDisable = [ "fortify" ];
        };
    };
}
