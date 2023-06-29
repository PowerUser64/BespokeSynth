{
  description = "A flake for developing bespoke synth";

  # Flake outputs
  outputs = { self, nixpkgs }:
    let
      # Systems supported
      allSystems = [
        "x86_64-linux" # 64-bit Intel/AMD Linux
        "aarch64-linux" # 64-bit ARM Linux
        "x86_64-darwin" # 64-bit Intel macOS
        "aarch64-darwin" # 64-bit ARM macOS
      ];

      # Helper to provide system-specific attributes
      forAllSystems = f: nixpkgs.lib.genAttrs allSystems (system: f {
        pkgs = import nixpkgs { inherit system; };
      });
      scripts = pkgs: with pkgs; [
        (writeScriptBin "repo-path" ''
          ${pkgs.git}/bin/git rev-parse --show-toplevel
        '')
        (writeScriptBin "clean" ''
          rm -rf "$(repo-path)/build"
        '')
        (writeScriptBin "configure" ''
          mkdir -p "$(repo-path)/build"
          export out=/tmp/out
          source $stdenv/setup
          export NIX_ENFORCE_PURITY=0
          cd "$(repo-path)"
          phases="configurePhase" genericBuild
        '')
        (writeScriptBin "build" ''
          export out=/tmp/out
          source $stdenv/setup
          export NIX_ENFORCE_PURITY=0
          cd "$(repo-path)/build"
          phases="buildPhase" genericBuild
        '')
      ];
    in
      {
        # Development environment output
        devShells = forAllSystems ({ pkgs }:
            {
              # You can also create a mkShell by basically copying all fields of bespokesynth,
              # but it is easier to reuse stuff
              default = pkgs.bespokesynth.overrideAttrs (finalAttrs: previousAttrs: {
                # Bespokesynth should use this instead of patching LD_…
                NIX_LDFLAGS = "-rpath ${pkgs.lib.makeLibraryPath (with pkgs; with xorg; [ libX11 libXrandr libXinerama libXext libXcursor libXScrnSaver ])}";
                dontPatchELF = true; # needed or nix will try to optimize the binary by removing "useless" rpath   
                # We add some more packages here to quickly build the package
                buildInputs = previousAttrs.buildInputs ++ scripts pkgs;

                # Not needed after nixpkgs#232522 is merged
                dontFixCmake = true;

                # Run `export cmakeBuildType=Release` to change to release builds
                cmakeBuildType = "Debug";
              });
            });
          apps = forAllSystems ({ pkgs }:
          let
            runLocal = pkgs.writeScriptBin "run-local" ''
              echo 'Not yet implemented :('
            '';
          in
          {
            default = {
              type = "app";
              program = "${runLocal}/bin/run-local";
            };
          });
      };
}
