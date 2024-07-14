{
  description = "Nintendo DS emulator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { nixpkgs, flake-utils, ... }: flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs { inherit system; };
      lib = pkgs.lib;
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
          SDL2
          zstd
          libarchive
        ] ++ lib.optionals stdenv.isLinux [
          wayland
        ];
        cmakeFlags = [
          (lib.cmakeBool "USE_QT6" true)
        ];
        passthru = {
          exePath = if stdenv.isDarwin then
            "/Applications/melonDS.app/Contents/MacOS/melonDS"
            else "/bin/melonDS";
        };
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
