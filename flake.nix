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

      melonDS = pkgs.qt6.qtbase.stdenv.mkDerivation {
        pname = "melonDS";
        version = "0.9.5-${shortRevision}";
        src = ./.;

        nativeBuildInputs = with pkgs; [
          cmake
          ninja
          pkg-config
          qt6.wrapQtAppsHook
        ];

        buildInputs = (with pkgs; [
          qt6.qtbase
          qt6.qtmultimedia
          SDL2
          zstd
          libarchive
          libGL
          libslirp
          enet
        ]) ++ optionals (!isDarwin) (with pkgs; [
          kdePackages.extra-cmake-modules
          qt6.qtwayland
          wayland
        ]);

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
      devShells = {
        default = pkgs.mkShell.override { stdenv = pkgs.qt6.qtbase.stdenv; } {
          inputsFrom = [ self.packages.${system}.default ];
        };

        # Shell for building static melonDS release builds with vcpkg
        # Use mkShellNoCC to ensure Nix's gcc/clang and stdlib isn't used
        vcpkg = pkgs.mkShellNoCC {
          packages = with pkgs; [
            autoconf
            autoconf-archive
            automake
            cmake
            cups.dev # Needed by qtbase despite not enabling print support
            git
            iconv.dev
            libtool
            ninja
            pkg-config
          ];
        };
      };
    }
  );
}
