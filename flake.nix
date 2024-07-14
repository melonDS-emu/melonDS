{
  description = "Nintendo DS emulator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { nixpkgs, flake-utils, ... }: flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs { inherit system; };
      melonDS = with pkgs; stdenv.mkDerivation {
        pname = "melonDS";
        version = "0.9.5";
        src = ./.;
        nativeBuildInputs = [
          cmake
          pkgconf
          kdePackages.wrapQtAppsHook
          ninja
        ];
        buildInputs = [
          kdePackages.qtbase
          kdePackages.qtmultimedia
          extra-cmake-modules
          wayland
          SDL2
          zstd
          libarchive
        ];
        cmakeFlags = [
          "-DUSE_QT6=ON"
        ];
      };
    in rec {
      apps.default = flake-utils.lib.mkApp { drv = packages.default; };
      packages.default = melonDS;
      devShells.default = pkgs.mkShell {
        inputsFrom = [ melonDS ];
      };
    }
  );
}
