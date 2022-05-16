#!/usr/bin/env ruby

require "open3"
require "fileutils"

$app_name = "melonDS"
$build_dmg = false
$build_dir = ""
$bundle = ""
$fallback_rpaths = ["/usr/local/lib", "/opt/local/lib"]

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

def system_path?(path)
  path.match(/^\/usr\/lib|^\/System/) != nil
end

def system_lib?(lib)
  system_path? File.dirname(lib)
end

def install_name_tool(exec, action, path1, path2 = nil)
  args = ["-#{action.to_s}", path1]
  args << path2 if path2 != nil

  out, status = Open3.capture2e("install_name_tool", *args, exec)
  if status != 0
    puts out
    exit status
  end
end

def strip(lib)
  out, _ = Open3.capture2("strip", "-Sx", lib)
  print out
end

def fixup_libs(prog, orig_path)
  throw "fixup_libs: #{prog} doesn't exist" unless File.exist? prog

  libs = get_load_libs(prog).map { |it| expand_load_path(orig_path, it) }.select { |it| not system_lib? it[0] }

  FileUtils.chmod("u+w", prog)
  strip prog

  libs.each do |lib|
    libpath, libtype = lib
    if File.basename(libpath) == File.basename(prog)
      if libtype == :absolute
        install_name_tool prog, :change, libpath, File.join("@rpath", File.basename(libpath))
      end
      next
    end
    
    framework = libpath.match(/(.*).framework/)
    framework = framework.to_s if framework

    if framework
      fwlib = libpath.sub(framework + "/", "")
      fwname = File.basename(framework)

      unless libtype == :rpath
        install_name_tool prog, :change, libpath, File.join("@rpath", fwname, fwlib)
      end
      
      next if File.exist? File.join(frameworks_dir, fwname)
      expath, _ = expand_load_path(orig_path, framework)
      FileUtils.cp_r(expath, frameworks_dir, preserve: true)
      FileUtils.chmod_R("u+w", File.join(frameworks_dir, fwname))
      fixup_libs File.join(frameworks_dir, fwname, fwlib), libpath
    else
      libname = File.basename(libpath)
      dest = File.join(frameworks_dir, libname)

      if libtype == :absolute
        install_name_tool prog, :change, libpath, File.join("@rpath", libname)
      end

      next if File.exist? dest
      expath, _ = expand_load_path(orig_path, libpath)
      FileUtils.copy expath, frameworks_dir
      FileUtils.chmod("u+w", dest)
      fixup_libs dest, libpath
    end
  end
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

File.read(File.join($build_dir, "CMakeCache.txt"))
  .split("\n")
  .find { |it| it.match /Qt(.)_DIR:PATH=(.*)/ }

qt_major = $1
qt_dir = $2
qt_dir = File.absolute_path("#{qt_dir}/../../..")

$fallback_rpaths << File.join(qt_dir, "lib")

plugin_paths = [
  File.join(qt_dir, "libexec", "qt#{qt_major}", "plugins"),
  File.join(qt_dir, "plugins"),
  File.join(qt_dir, "share", "qt", "plugins")
]

qt_plugins = plugin_paths.find { |file| File.exist? file }

if qt_plugins == nil
  puts "Couldn't find Qt plugins, tried looking for:"
  plugin_paths.each { |path| puts " - #{path}" }
  exit 1
end

FileUtils.mkdir_p(frameworks_dir)
fixup_libs(executable, executable)

bundle_plugins = File.join($bundle, "Contents", "PlugIns")

want_plugins = ["styles/libqmacstyle.dylib", "platforms/libqcocoa.dylib", "imageformats/libqsvg.dylib"]
want_plugins.each do |plug|
  destdir = File.join(bundle_plugins, File.dirname(plug))
  FileUtils.mkdir_p(destdir)
  FileUtils.copy(File.join(qt_plugins, plug), destdir)
  fixup_libs File.join(bundle_plugins, plug), File.join(qt_plugins, plug)
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
