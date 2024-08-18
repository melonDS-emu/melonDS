{
  description = "Nintendo DS emulator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }: flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs { inherit system; };
      inherit (pkgs.lib) cmakeBool optionals makeLibraryPath;
      inherit (pkgs.stdenv) isLinux isDarwin;

      revision = with self; if sourceInfo?dirtyRev
        then sourceInfo.dirtyRev
        else sourceInfo.rev;
      shortRevision = with self; if sourceInfo?dirtyShortRev
        then sourceInfo.dirtyShortRev
        else sourceInfo.shortRev;

      melonDS = pkgs.stdenv.mkDerivation {
        pname = "melonDS";
        version = "0.9.5-${shortRevision}";
        src = ./.;

        nativeBuildInputs = with pkgs; [
          cmake
          ninja
          pkg-config
          kdePackages.wrapQtAppsHook
        ];

        buildInputs = (with pkgs; [
          kdePackages.qtbase
          kdePackages.qtmultimedia
          extra-cmake-modules
          SDL2
          zstd
          libarchive
          libGL
          libslirp
          enet
        ]) ++ optionals isLinux [
          pkgs.wayland
          pkgs.kdePackages.qtwayland
        ];

        cmakeFlags = [
          (cmakeBool "USE_QT6" true)
          (cmakeBool "USE_SYSTEM_LIBSLIRP" true)
          (cmakeBool "MELONDS_EMBED_BUILD_INFO" true)
        ];

        env.MELONDS_GIT_HASH = revision;
        env.MELONDS_GIT_BRANCH = "(unknown)";
        env.MELONDS_BUILD_PROVIDER = "Nix";

        qtWrapperArgs = optionals isLinux [
          "--prefix LD_LIBRARY_PATH : ${makeLibraryPath [ pkgs.libpcap pkgs.wayland ]}"
        ] ++ optionals isDarwin [
          "--prefix DYLD_LIBRARY_PATH : ${makeLibraryPath [ pkgs.libpcap ]}"
        ];

        passthru = {
          exePath = if isDarwin then
            "/Applications/melonDS.app/Contents/MacOS/melonDS"
            else "/bin/melonDS";
        };
      };
    in {
      packages.default = melonDS;
      apps.default = flake-utils.lib.mkApp {
        drv = self.packages.${system}.default;
      };
      devShells.default = pkgs.mkShell {
        inputsFrom = [ self.packages.${system}.default ];
      };
    }
  );
}
