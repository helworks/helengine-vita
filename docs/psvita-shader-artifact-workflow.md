# PS Vita Shader Artifact Workflow

The shipped Vita renderer does not compile shaders during startup. The authoritative compiler runs on a real Vita, and the complete `SceShaccCgCompileOutput::programData` payload is exported as a `.pvsa` artifact.

## Export

Install the PSM Runtime compiler module at `ur0:/data/libshacccg.suprx` before exporting; the module is not included in the VPK. Build the Vita development image and copy an empty marker file named `ux0:data/helengine/export-forward-lambert.flag` onto the Vita with VitaShell or FTP. Launch the normal VPK once; it detects the marker, starts the installed compiler module, attempts both stages, and exits. On success it removes the marker and writes both artifacts to `ux0:data/helengine/shaders/`. On failure it preserves the marker and writes setup/stage result codes to `ux0:data/helengine/shader-export.log`. Retrieve the log, marker, and shader directory through the normal Vita file-transfer workflow. For a custom shader variant, provide:

- source text and source filename;
- `SCE_SHACCCG_PROFILE_VP` or `SCE_SHACCCG_PROFILE_FP`;
- entry point (`VS` or `PS`);
- optimization and warning settings;
- output path under `cooked/shaders/`.

The exporter writes `PVSA` version 1 artifacts containing compiler identity, source/options identity, complete program bytes, and a content hash. Export one vertex and one fragment artifact for each shader variant.

## Host import and cook

Copy the exported `.pvsa` files into the project’s staged `cooked/shaders/` directory. The host builder validates the artifact header and hash, then cooks a material containing the vertex artifact hash, fragment artifact hash, shader variant, and parameter-contract version. The artifact hashes are returned as cooked dependencies and are packaged by the existing CMake cooked-content glob.

## Runtime verification

On Vita, verify that:

1. The material reader accepts `PVMT` version 2.
2. The renderer resolves both artifact hashes to `app0:/cooked/shaders/<hash>.pvsa`.
3. The runtime does not load the shader compiler module.
4. `sceGxmProgramCheck`, program registration, parameter lookup, and GXM patching succeed.
5. Repeated draws reuse the initialized program rather than compiling or patching per draw.

If an artifact is missing, malformed, stale, or has the wrong stage profile, initialization fails with the material/program identity. The engine does not silently convert a production GPU material into CPU Lambert rendering.
