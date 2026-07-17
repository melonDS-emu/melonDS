#!/usr/bin/env ruby

require "open3"
require "fileutils"

$app_name = "melonDS"
$build_dmg = false
$build_dir = ""
$bundle = ""
$fallback_rpaths = []

def frameworks_dir
  File.join($bundle, "Contents", "Frameworks")
end

def executable
  File.join($bundle, "Contents", "MacOS", $app_name)
end

def get_rpaths(lib)
  out, _ = Open3.capture2("otool", "-l", lib)
  out = out.split("\n")
  rpaths = []

  out.each_with_index do |line, i|
    if line.match(/^ *cmd LC_RPATH$/)
      rpaths << out[i + 2].strip.split(" ")[1]
    end
  end

  return rpaths
end

def get_load_libs(lib)
  out, _ = Open3.capture2("otool", "-L", lib)
  out.split("\n")
    .drop(1)
    .map { |it| it.strip.gsub(/ \(.*/, "") }
end

def expand_load_path(lib, path)
  if path.match(/@(rpath|loader_path|executable_path)/) 
    path_type = $1
    file_name = path.gsub(/^@#{path_type}\//, "")

    case path_type
      when "rpath"
        get_rpaths(lib).each do |rpath|
          if (loader_relative = rpath.match(/^@loader_path(.*)/))
            rpath = File.expand_path(File.join(File.dirname(lib), loader_relative[1]))
          end
          file = File.join(rpath, file_name)
          return file, :rpath if File.exist? file
          if rpath.match(/^@executable_path(.*)/) != nil
            relative = rpath.sub(/^@executable_path/, "")
            return "#{$bundle}/Contents/MacOS#{relative}/#{file_name}", :executable_path
          end
        end
        file = $fallback_rpaths
          .map { |it| File.join(it, file_name) }
          .find { |it| File.exist? it }
        if file == nil
          path = File.join(File.dirname(lib), file_name)
          file = path if File.exist? path
        end
        return file, :rpath if file
      when "executable_path"
        file = File.join(File.dirname(executable), file_name)
        return file, :executable_path if File.exist? file
      when "loader_path"
        file = File.join(File.dirname(lib), file_name)
        return file, :loader_path if File.exist? file
      else
        throw "Unknown @path type"
    end
  else
    return File.absolute_path(path), :absolute
  end

  return nil
end

def detect_framework(lib)
  framework = lib.match(/(.*).framework/)
  framework = framework.to_s if framework

  if framework
    fwname = File.basename(framework)
    fwlib = lib.sub(framework + "/", "")
    return true, framework, fwname, fwlib
  else
    return false
  end
end

def system_path?(path)
  path.match(/^\/usr\/lib|^\/System/) != nil
end

def system_lib?(lib)
  system_path? File.dirname(lib)
end

def install_name_tool(exec, *options)
  args = options.map do |it|
    if it.is_a? Symbol then "-#{it.to_s}" else it end
  end

  Open3.popen3("install_name_tool", *args, exec) do |stdin, stdout, stderr, thread|
    print stdout.read
    err = stderr.read
    unless err.match? "code signature"
      print err
    end
  end
end

def strip(lib)
  out, _ = Open3.capture2("xcrun", "strip", "-no_code_signature_warning", "-Sx", lib)
  print out
end

def fixup_libs(prog, orig_path)
  throw "fixup_libs: #{prog} doesn't exist" unless File.exist? prog

  # keep the original load command string around: install_name_tool -change
  # must be given exactly that string, not the resolved path
  libs = get_load_libs(prog).map do |ref|
    expanded = expand_load_path(orig_path, ref)
    raise "fixup_libs: couldn't resolve #{ref} referenced by #{prog}" if expanded == nil
    [ref, expanded]
  end
  libs = libs.select { |ref, expanded| not system_lib? expanded[0] }

  FileUtils.chmod("u+w", prog)
  strip prog

  changes = []

  isfw, _, fwname, fwlib = detect_framework(prog)
  if isfw then
    changes += [:id, File.join("@rpath", fwname, fwlib)]
  else
    changes += [:id, File.join("@rpath", File.basename(prog))]
  end

  libs.each do |ref, lib|
    libpath, libtype = lib
    if File.basename(libpath) == File.basename(prog)
      newref = File.join("@rpath", File.basename(libpath))
      changes += [:change, ref, newref] unless ref == newref
      next
    end

    is_framework, fwpath, fwname, fwlib = detect_framework(libpath)

    if is_framework
      newref = File.join("@rpath", fwname, fwlib)
      changes += [:change, ref, newref] unless ref == newref

      next if File.exist? File.join(frameworks_dir, fwname)
      expath, _ = expand_load_path(orig_path, fwpath)
      FileUtils.cp_r(expath, frameworks_dir, preserve: true)
      FileUtils.chmod_R("u+w", File.join(frameworks_dir, fwname))
      fixup_libs File.join(frameworks_dir, fwname, fwlib), libpath
    else
      # copy under the real file name; the reference may use a symlink name
      # (e.g. Homebrew's @loader_path/libicudata.78.dylib -> libicudata.78.3.dylib)
      reallibpath = File.realpath(libpath)
      libname = File.basename(reallibpath)
      dest = File.join(frameworks_dir, libname)

      newref = File.join("@rpath", libname)
      changes += [:change, ref, newref] unless ref == newref

      next if File.exist? dest
      expath, _ = expand_load_path(orig_path, reallibpath)
      FileUtils.copy expath, frameworks_dir
      FileUtils.chmod("u+w", dest)
      fixup_libs dest, reallibpath
    end
  end

  install_name_tool(prog, *changes)
end

if ARGV[0] == "--dmg"
  $build_dmg = true
  ARGV.shift
end

if ARGV.length != 1
  puts "Usage: #{Process.argv0} [--dmg] <build-dir>"
  return
end

$build_dir = ARGV[0]
unless File.exist? $build_dir
  puts "#{$build_dir} doesn't exist"
end


$bundle = File.join($build_dir, "#{$app_name}.app")

unless File.exist? $bundle and File.exist? File.join($build_dir, "CMakeCache.txt")
  puts "#{$build_dir} doesn't look like a valid build directory"
  exit 1
end

for lib in get_load_libs(executable) do
  next if system_lib? lib

  path = File.dirname(lib)

  if path.match? ".framework"
    path = path.sub(/\/[^\/]+\.framework.*/, "")
  end

  $fallback_rpaths << path unless $fallback_rpaths.include? path
end

$qt_major = nil

qt_dirs = File.read(File.join($build_dir, "CMakeCache.txt"))
  .split("\n")
  .select { |it| it.match /^Qt([\w]+)_DIR:PATH=.*/ }
  .map { |dir|
    dir.match /^Qt(5|6).*\=(.*)/
    throw "Inconsistent Qt versions found." if $qt_major != nil && $qt_major != $1
    $qt_major = $1
    File.absolute_path("#{$2}/../../..")
  }.uniq


def locate_plugin(dirs, plugin)
  plugin_paths = [
    File.join("plugins", plugin),
    File.join("lib", "qt-#{$qt_major}", "plugins", plugin),
    File.join("libexec", "qt-#{$qt_major}", "plugins", plugin),
    File.join("share", "qt", "plugins", plugin)
  ]

  dirs.each do |dir|
    plugin_paths.each do |plug|
      path = File.join(dir, plug)
      return path if File.exists? path
    end
  end
  puts "Couldn't find the required Qt plugin: #{plugin}"
  puts "Tried the following prefixes: "
  puts dirs.map { |dir| "- #{dir}"}.join("\n")
  puts "With the following plugin paths:"
  puts plugin_paths.map { |path| "- #{path}"}.join("\n")
  exit 1
end

FileUtils.mkdir_p(frameworks_dir)
fixup_libs(executable, executable)

bundle_plugins = File.join($bundle, "Contents", "PlugIns")

want_plugins = [
  "styles/libqmacstyle.dylib",
  "platforms/libqcocoa.dylib",
  "imageformats/libqsvg.dylib"
]

want_plugins.each do |plug|
  pluginpath = locate_plugin(qt_dirs, plug)

  destdir = File.join(bundle_plugins, File.dirname(plug))
  FileUtils.mkdir_p(destdir)
  FileUtils.copy(pluginpath, destdir)
  fixup_libs File.join(bundle_plugins, plug), pluginpath
end

# sdl2-compat locates SDL3 with dlopen at runtime instead of linking it, so
# the dependency walk above never sees it; stage it next to the bundled SDL2
# where the @loader_path lookup will find it
sdl2 = Dir.glob(File.join(frameworks_dir, "libSDL2*.dylib")).first
if sdl2 && File.binread(sdl2).include?("libSDL3.dylib") &&
   !File.exist?(File.join(frameworks_dir, "libSDL3.dylib"))
  search_dirs = get_rpaths(executable) + $fallback_rpaths
  sdl3 = search_dirs
    .map { |dir| File.join(dir, "libSDL3.dylib") }
    .find { |file| File.exist? file }
  if sdl3
    sdl3 = File.realpath(sdl3)
    dest = File.join(frameworks_dir, "libSDL3.dylib")
    FileUtils.copy sdl3, dest
    FileUtils.chmod("u+w", dest)
    fixup_libs dest, sdl3
  else
    puts "Warning: sdl2-compat needs libSDL3.dylib but it couldn't be found; the bundle won't be self-contained"
  end
end

want_rpath = "@executable_path/../Frameworks"
exec_rpaths = get_rpaths(executable)
exec_rpaths.select { |path| path != want_rpath }.each do |path|
  install_name_tool executable, :delete_rpath, path
end

unless exec_rpaths.include? want_rpath
  install_name_tool executable, :add_rpath, want_rpath
end

exec_rpaths = get_rpaths(executable)

Dir.glob("#{frameworks_dir}/**/Headers").each do |dir|
  FileUtils.rm_rf dir
end

out, _ = Open3.capture2("codesign", "-s", "-", "-f", "--deep", $bundle)
print out

if $build_dmg
    dmg_dir = File.join($build_dir, "dmg")
    FileUtils.mkdir_p(dmg_dir)
    FileUtils.cp_r($bundle, dmg_dir, preserve: true)
    FileUtils.ln_s("/Applications", File.join(dmg_dir, "Applications"))

    `hdiutil create -fs HFS+ -volname melonDS -srcfolder "#{dmg_dir}" -ov -format UDBZ "#{$build_dir}/melonDS.dmg"`
    FileUtils.rm_rf(dmg_dir)
end
