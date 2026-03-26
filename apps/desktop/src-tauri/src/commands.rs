use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::ffi::OsStr;
use std::path::Path;
use std::process::Command;
use walkdir::WalkDir;

// ---------------------------------------------------------------------------
// Type definitions — mirror the TypeScript interfaces in tauri-ipc.ts
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RomScanResult {
    pub file_path: String,
    pub file_name: String,
    pub system: String,
    pub inferred_title: String,
    pub file_size_bytes: u64,
    pub last_modified: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FilePickResult {
    pub path: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DirectoryPickResult {
    pub path: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LaunchResult {
    pub success: bool,
    pub pid: Option<u32>,
    pub error: Option<String>,
}

#[derive(Debug, Clone, Deserialize)]
pub struct FileFilter {
    pub name: String,
    pub extensions: Vec<String>,
}

// ---------------------------------------------------------------------------
// ROM extension → system mapping (mirrors emulator-bridge/src/scanner.ts)
// ---------------------------------------------------------------------------

fn rom_extensions() -> HashMap<&'static str, &'static str> {
    let mut map = HashMap::new();
    // NES
    map.insert("nes", "nes");
    map.insert("fds", "nes");
    map.insert("unf", "nes");
    // SNES
    map.insert("sfc", "snes");
    map.insert("smc", "snes");
    map.insert("fig", "snes");
    map.insert("gd3", "snes");
    // Game Boy
    map.insert("gb", "gb");
    map.insert("sgb", "gb");
    // Game Boy Color
    map.insert("gbc", "gbc");
    map.insert("cgb", "gbc");
    // Game Boy Advance
    map.insert("gba", "gba");
    map.insert("agb", "gba");
    // Nintendo 64
    map.insert("z64", "n64");
    map.insert("v64", "n64");
    map.insert("n64", "n64");
    // Nintendo DS
    map.insert("nds", "nds");
    map.insert("dsi", "nds");
    map.insert("ids", "nds");
    // Sega Master System
    map.insert("sms", "sms");
    map.insert("gg", "sms");
    map.insert("sg", "sms");
    // Sega Genesis / Mega Drive
    map.insert("md", "genesis");
    map.insert("gen", "genesis");
    map.insert("bin", "genesis");
    map.insert("smd", "genesis");
    // GameCube
    map.insert("gcm", "gc");
    map.insert("gcz", "gc");
    map.insert("rvz", "gc");
    map.insert("iso", "gc");
    map.insert("ciso", "gc");
    // Nintendo 3DS
    map.insert("3ds", "3ds");
    map.insert("cci", "3ds");
    map.insert("cia", "3ds");
    map.insert("3dsx", "3ds");
    map
}

/// Infer a human-readable title from a ROM filename.
/// Strips the extension, replaces separators with spaces, and removes
/// common region / dump tags like (USA), [!], etc.
fn infer_title(filename: &str) -> String {
    let stem = Path::new(filename)
        .file_stem()
        .and_then(OsStr::to_str)
        .unwrap_or(filename);
    // Replace common separators with spaces
    let title: String = stem
        .replace('_', " ")
        .replace('-', " ")
        .replace('.', " ");
    // Strip region / dump tags (e.g. "(USA)", "[!]", "(Rev 1)")
    let mut cleaned = String::with_capacity(title.len());
    let mut depth_round = 0i32;
    let mut depth_square = 0i32;
    for ch in title.chars() {
        match ch {
            '(' => depth_round += 1,
            ')' => {
                depth_round = (depth_round - 1).max(0);
                continue;
            }
            '[' => depth_square += 1,
            ']' => {
                depth_square = (depth_square - 1).max(0);
                continue;
            }
            _ if depth_round > 0 || depth_square > 0 => continue,
            _ => cleaned.push(ch),
        }
    }
    // Collapse whitespace and trim
    cleaned
        .split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
}

// ---------------------------------------------------------------------------
// Command: scan_rom_directory
// ---------------------------------------------------------------------------

#[tauri::command]
pub fn scan_rom_directory(dir: String, recursive: bool) -> Result<Vec<RomScanResult>, String> {
    let dir_path = Path::new(&dir);
    if !dir_path.is_dir() {
        return Err(format!("Directory not found: {}", dir));
    }

    let extensions = rom_extensions();
    let max_depth = if recursive { 4 } else { 1 };
    let mut results: Vec<RomScanResult> = Vec::new();

    let walker = WalkDir::new(dir_path)
        .max_depth(max_depth)
        .follow_links(true)
        .into_iter()
        .filter_map(|e| e.ok());

    for entry in walker {
        if !entry.file_type().is_file() {
            continue;
        }
        let path = entry.path();
        let ext = match path.extension().and_then(OsStr::to_str) {
            Some(e) => e.to_lowercase(),
            None => continue,
        };
        let system = match extensions.get(ext.as_str()) {
            Some(s) => (*s).to_string(),
            None => continue,
        };

        let file_name = path
            .file_name()
            .and_then(OsStr::to_str)
            .unwrap_or("")
            .to_string();

        let metadata = entry.metadata().ok();
        let file_size_bytes = metadata.as_ref().map(|m| m.len()).unwrap_or(0);
        let last_modified = metadata
            .and_then(|m| m.modified().ok())
            .map(|t| {
                let dt: chrono::DateTime<chrono::Utc> = t.into();
                dt.to_rfc3339()
            })
            .unwrap_or_default();

        results.push(RomScanResult {
            file_path: path.to_string_lossy().to_string(),
            file_name: file_name.clone(),
            system,
            inferred_title: infer_title(&file_name),
            file_size_bytes,
            last_modified,
        });
    }

    results.sort_by(|a, b| a.file_name.cmp(&b.file_name));
    Ok(results)
}

// ---------------------------------------------------------------------------
// Command: pick_file
// ---------------------------------------------------------------------------

#[tauri::command]
pub async fn pick_file(
    app: tauri::AppHandle,
    filters: Option<Vec<FileFilter>>,
) -> Result<FilePickResult, String> {
    use tauri_plugin_dialog::DialogExt;

    let mut builder = app.dialog().file();

    if let Some(filter_list) = filters {
        for f in &filter_list {
            let ext_refs: Vec<&str> = f.extensions.iter().map(|s| s.as_str()).collect();
            builder = builder.add_filter(&f.name, &ext_refs);
        }
    }

    let file_path = builder.blocking_pick_file();
    Ok(FilePickResult {
        path: file_path.map(|fp| fp.to_string_lossy().to_string()),
    })
}

// ---------------------------------------------------------------------------
// Command: pick_directory
// ---------------------------------------------------------------------------

#[tauri::command]
pub async fn pick_directory(app: tauri::AppHandle) -> Result<DirectoryPickResult, String> {
    use tauri_plugin_dialog::DialogExt;

    let dir_path = app.dialog().file().blocking_pick_folder();
    Ok(DirectoryPickResult {
        path: dir_path.map(|fp| fp.to_string_lossy().to_string()),
    })
}

// ---------------------------------------------------------------------------
// Command: launch_emulator (netplay)
// ---------------------------------------------------------------------------

#[tauri::command]
pub fn launch_emulator(
    rom_path: String,
    system: String,
    backend_id: String,
    save_directory: Option<String>,
    player_slot: Option<u32>,
    netplay_host: Option<String>,
    netplay_port: Option<u16>,
    session_token: Option<String>,
    fullscreen: Option<bool>,
) -> LaunchResult {
    launch_emulator_process(LaunchParams {
        rom_path,
        system,
        backend_id,
        emulator_path: None,
        save_directory,
        player_slot,
        netplay_host,
        netplay_port,
        session_token,
        fullscreen: fullscreen.unwrap_or(false),
    })
}

// ---------------------------------------------------------------------------
// Command: launch_local (single-player)
// ---------------------------------------------------------------------------

#[tauri::command]
pub fn launch_local(
    emulator_path: String,
    rom_path: String,
    system: String,
    backend_id: String,
    save_directory: Option<String>,
    fullscreen: Option<bool>,
) -> LaunchResult {
    launch_emulator_process(LaunchParams {
        rom_path,
        system,
        backend_id,
        emulator_path: Some(emulator_path),
        save_directory,
        player_slot: None,
        netplay_host: None,
        netplay_port: None,
        session_token: None,
        fullscreen: fullscreen.unwrap_or(false),
    })
}

// ---------------------------------------------------------------------------
// Command: check_file_exists
// ---------------------------------------------------------------------------

#[tauri::command]
pub fn check_file_exists(path: String) -> bool {
    Path::new(&path).exists()
}

// ---------------------------------------------------------------------------
// Internal launch helper
// ---------------------------------------------------------------------------

struct LaunchParams {
    rom_path: String,
    system: String,
    backend_id: String,
    emulator_path: Option<String>,
    save_directory: Option<String>,
    player_slot: Option<u32>,
    netplay_host: Option<String>,
    netplay_port: Option<u16>,
    session_token: Option<String>,
    fullscreen: bool,
}

/// Build the command-line arguments for the given backend and spawn the process.
fn launch_emulator_process(params: LaunchParams) -> LaunchResult {
    let rom = Path::new(&params.rom_path);
    if !rom.exists() {
        return LaunchResult {
            success: false,
            pid: None,
            error: Some(format!("ROM file not found: {}", params.rom_path)),
        };
    }

    // Resolve the emulator executable path.
    let exe = match &params.emulator_path {
        Some(p) if !p.is_empty() => p.clone(),
        _ => resolve_default_exe(&params.backend_id),
    };
    if exe.is_empty() {
        return LaunchResult {
            success: false,
            pid: None,
            error: Some(format!(
                "No emulator executable configured for backend '{}'",
                params.backend_id
            )),
        };
    }

    let exe_path = Path::new(&exe);
    if !exe_path.exists() {
        return LaunchResult {
            success: false,
            pid: None,
            error: Some(format!("Executable not found at path: {}", exe)),
        };
    }

    let mut cmd = Command::new(&exe);

    // Build backend-specific arguments (mirrors emulator-bridge adapters.ts).
    build_backend_args(&mut cmd, &params);

    // Inject session token as environment variable for relay authentication.
    if let Some(token) = &params.session_token {
        cmd.env("RETRO_OASIS_SESSION_TOKEN", token);
    }

    match cmd.spawn() {
        Ok(child) => LaunchResult {
            success: true,
            pid: Some(child.id()),
            error: None,
        },
        Err(e) => LaunchResult {
            success: false,
            pid: None,
            error: Some(format!("Failed to launch emulator: {}", e)),
        },
    }
}

/// Return the conventional executable name for a given backend.
/// Users will typically configure custom paths via Settings; this provides
/// a sensible default for backends that are commonly on `$PATH`.
fn resolve_default_exe(backend_id: &str) -> String {
    match backend_id {
        "fceux" => "fceux".into(),
        "snes9x" => "snes9x".into(),
        "mgba" => "mgba".into(),
        "vba-m" => "visualboyadvance-m".into(),
        "sameboy" => "sameboy".into(),
        "mupen64plus" => "mupen64plus".into(),
        "melonds" => "melonDS".into(),
        "dolphin" => "dolphin-emu".into(),
        "citra" | "lime3ds" => "citra".into(),
        "retroarch" => "retroarch".into(),
        "cemu" => "cemu".into(),
        "duckstation" => "duckstation".into(),
        "pcsx2" => "pcsx2".into(),
        "ppsspp" => "ppsspp".into(),
        _ => String::new(),
    }
}

/// Append backend-specific command-line arguments to the process command.
/// This mirrors the adapter logic in `packages/emulator-bridge/src/adapters.ts`.
fn build_backend_args(cmd: &mut Command, params: &LaunchParams) {
    let backend = params.backend_id.as_str();

    match backend {
        // ---- NES: FCEUX ----
        "fceux" => {
            cmd.arg(&params.rom_path);
            if let Some(ref host) = params.netplay_host {
                cmd.args(["--net", host]);
            }
            if let Some(port) = params.netplay_port {
                cmd.args(["--port", &port.to_string()]);
            }
        }

        // ---- SNES: Snes9x ----
        "snes9x" => {
            cmd.arg(&params.rom_path);
            if let Some(ref host) = params.netplay_host {
                cmd.args(["-netplayserver", host]);
            }
            if let Some(port) = params.netplay_port {
                cmd.args(["-netplayport", &port.to_string()]);
            }
        }

        // ---- GB/GBC/GBA: mGBA ----
        "mgba" => {
            cmd.arg(&params.rom_path);
            if params.fullscreen {
                cmd.arg("--fullscreen");
            }
        }

        // ---- GB/GBC/GBA: VBA-M (link cable via TCP) ----
        "vba-m" => {
            cmd.arg(&params.rom_path);
            if let Some(ref host) = params.netplay_host {
                cmd.args(["--link-host", host]);
            }
            if let Some(port) = params.netplay_port {
                cmd.args(["--link-port", &port.to_string()]);
            }
        }

        // ---- GB/GBC: SameBoy (link cable via TCP) ----
        "sameboy" => {
            cmd.arg(&params.rom_path);
            if let Some(ref host) = params.netplay_host {
                cmd.args(["--link-host", host]);
            }
            if let Some(port) = params.netplay_port {
                cmd.args(["--link-port", &port.to_string()]);
            }
        }

        // ---- N64: Mupen64Plus ----
        "mupen64plus" => {
            if params.fullscreen {
                cmd.arg("--fullscreen");
            }
            if let Some(ref host) = params.netplay_host {
                cmd.args(["--netplay-host", host]);
            }
            if let Some(port) = params.netplay_port {
                cmd.args(["--netplay-port", &port.to_string()]);
            }
            if let Some(slot) = params.player_slot {
                cmd.args(["--netplay-player", &slot.to_string()]);
            }
            cmd.arg(&params.rom_path);
        }

        // ---- NDS: melonDS ----
        "melonds" => {
            cmd.arg(&params.rom_path);
            if params.fullscreen {
                cmd.arg("--fullscreen");
            }
        }

        // ---- GameCube / Wii: Dolphin ----
        "dolphin" => {
            cmd.args(["-b", "-e", &params.rom_path]);
            if params.fullscreen {
                cmd.arg("--config=Graphics.Settings.BorderlessFullscreen=True");
            }
        }

        // ---- 3DS: Citra / Lime3DS ----
        "citra" | "lime3ds" => {
            cmd.arg(&params.rom_path);
            if params.fullscreen {
                cmd.arg("--fullscreen");
            }
            if let Some(ref host) = params.netplay_host {
                cmd.args(["--multiplayer-server", host]);
            }
            if let Some(port) = params.netplay_port {
                cmd.args(["--port", &port.to_string()]);
            }
        }

        // ---- RetroArch (multi-system) ----
        "retroarch" => {
            cmd.arg(&params.rom_path);
            if params.fullscreen {
                cmd.arg("--fullscreen");
            }
            if let Some(ref host) = params.netplay_host {
                cmd.args(["--connect", host]);
            }
            if let Some(port) = params.netplay_port {
                cmd.args(["--port", &port.to_string()]);
            }
        }

        // ---- Wii U: Cemu ----
        "cemu" => {
            cmd.args(["-g", &params.rom_path]);
            if params.fullscreen {
                cmd.arg("-f");
            }
        }

        // ---- PSX: DuckStation ----
        "duckstation" => {
            cmd.arg(&params.rom_path);
            if params.fullscreen {
                cmd.arg("--fullscreen");
            }
        }

        // ---- PS2: PCSX2 ----
        "pcsx2" => {
            cmd.arg(&params.rom_path);
            if params.fullscreen {
                cmd.arg("--fullscreen");
            }
        }

        // ---- PSP: PPSSPP ----
        "ppsspp" => {
            cmd.arg(&params.rom_path);
            if params.fullscreen {
                cmd.arg("--fullscreen");
            }
        }

        // ---- Unknown backend: pass ROM path only ----
        _ => {
            cmd.arg(&params.rom_path);
        }
    }

    // Append save directory if provided (for backends that support it).
    if let Some(ref save_dir) = params.save_directory {
        match backend {
            "mgba" => {
                cmd.args(["--save-dir", save_dir]);
            }
            "retroarch" => {
                cmd.args(["--appendconfig", &format!("savefile_directory={}", save_dir)]);
            }
            _ => {
                // Most backends don't have a standard save-dir flag;
                // save paths are handled via environment or config files.
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_infer_title_basic() {
        assert_eq!(infer_title("Super_Mario_World.sfc"), "Super Mario World");
    }

    #[test]
    fn test_infer_title_strips_tags() {
        assert_eq!(
            infer_title("Pokemon Red (USA) [!].gb"),
            "Pokemon Red"
        );
    }

    #[test]
    fn test_infer_title_dashes() {
        assert_eq!(
            infer_title("Legend-of-Zelda.nes"),
            "Legend of Zelda"
        );
    }

    #[test]
    fn test_rom_extensions_coverage() {
        let ext = rom_extensions();
        assert_eq!(ext.get("nes"), Some(&"nes"));
        assert_eq!(ext.get("sfc"), Some(&"snes"));
        assert_eq!(ext.get("gb"), Some(&"gb"));
        assert_eq!(ext.get("gbc"), Some(&"gbc"));
        assert_eq!(ext.get("gba"), Some(&"gba"));
        assert_eq!(ext.get("z64"), Some(&"n64"));
        assert_eq!(ext.get("nds"), Some(&"nds"));
        assert_eq!(ext.get("sms"), Some(&"sms"));
        assert_eq!(ext.get("md"), Some(&"genesis"));
        assert_eq!(ext.get("gcm"), Some(&"gc"));
        assert_eq!(ext.get("3ds"), Some(&"3ds"));
    }

    #[test]
    fn test_check_file_exists_false() {
        assert!(!check_file_exists("/nonexistent/path/to/file.rom".into()));
    }

    #[test]
    fn test_scan_rom_directory_missing_dir() {
        let result = scan_rom_directory("/nonexistent/dir".into(), false);
        assert!(result.is_err());
    }

    #[test]
    fn test_resolve_default_exe() {
        assert_eq!(resolve_default_exe("fceux"), "fceux");
        assert_eq!(resolve_default_exe("mgba"), "mgba");
        assert_eq!(resolve_default_exe("melonds"), "melonDS");
        assert_eq!(resolve_default_exe("dolphin"), "dolphin-emu");
        assert!(resolve_default_exe("unknown-backend").is_empty());
    }
}
