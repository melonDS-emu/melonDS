mod commands;

pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_shell::init())
        .invoke_handler(tauri::generate_handler![
            commands::scan_rom_directory,
            commands::pick_file,
            commands::pick_directory,
            commands::launch_emulator,
            commands::launch_local,
            commands::check_file_exists,
        ])
        .run(tauri::generate_context!())
        .expect("error while running RetroOasis");
}
