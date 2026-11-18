{
  description = "NASA cFS on the Atmosphere microkernel";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    # The Verus that the Atmosphere kernel is verified with. flake.lock must
    # name the revision that atmosphere/Cargo.lock pins for vstd;
    # tools/verify-atmosphere.sh checks. It keeps its own nixpkgs, because it
    # builds with an older Rust.
    verus.url = "github:mars-research/verus/mars";
  };

  outputs = { self, nixpkgs, verus }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      # `nix build .#verus` builds Verus without std and alloc, as the
      # Atmosphere flake does. It is not part of the shell, since it takes a
      # while to build and only verification needs it.
      packages = forAllSystems (pkgs:
        let
          v = verus.packages.${pkgs.stdenv.hostPlatform.system}.verus-no-std;
        in
        {
          # rust_verify loads the compiler libraries of the Rust it was built
          # with; the upstream package leaves finding them to its dev shell.
          verus = pkgs.writeShellScriptBin "rust_verify" ''
            export DYLD_LIBRARY_PATH=${v.rust}/lib''${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}
            export LD_LIBRARY_PATH=${v.rust}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
            exec ${v}/bin/rust_verify "$@"
          '';
        });

      devShells = forAllSystems (pkgs:
        let
          # GNU binutils for x86-64 ELF. The kernel's build script calls `ld`
          # and `objcopy` by their plain names, so wrap them under those names.
          binutils = pkgs.pkgsCross.x86_64-embedded.buildPackages.binutils;
          x86Tool = name: pkgs.writeShellScriptBin name ''
            exec ${binutils}/bin/x86_64-elf-${name} "$@"
          '';

          # clang as a cross compiler for freestanding x86-64 code. The
          # wrapped clang adds flags for the host; the bare one needs to be
          # told where its own headers (stddef.h, stdint.h) are.
          llvm = pkgs.llvmPackages;
          atmoCC = pkgs.writeShellScriptBin "atmo-cc" ''
            exec ${llvm.clang-unwrapped}/bin/clang \
              -resource-dir ${llvm.clang-unwrapped.lib}/lib/clang/${nixpkgs.lib.versions.major llvm.clang-unwrapped.version} \
              "$@"
          '';
        in
        {
          default = pkgs.mkShell {
            packages = [
              # Rust comes from rustup, which reads atmosphere/rust-toolchain.toml.
              # Verus pins a different toolchain the same way.
              pkgs.rustup
              atmoCC
              pkgs.cmake
              pkgs.gnumake
              pkgs.nasm
              pkgs.qemu
              pkgs.xxd
              pkgs.python3
              pkgs.git
            ] ++ map x86Tool [ "ld" "objcopy" "objdump" "nm" "readelf" ];

            # tools/env.sh and the CMake toolchain file read this.
            ATMO_CC = "atmo-cc";
          };
        });
    };
}
