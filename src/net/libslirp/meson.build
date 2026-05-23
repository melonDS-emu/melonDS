project('libslirp', 'c',
  version : '4.9.1',
  license : 'BSD-3-Clause',
  default_options : ['warning_level=1', 'c_std=gnu99'],
  meson_version : '>= 0.60',
)

version = meson.project_version()
varr = version.split('.')
major_version = varr[0]
minor_version = varr[1]
micro_version = varr[2]

conf = configuration_data()
conf.set('SLIRP_MAJOR_VERSION', major_version)
conf.set('SLIRP_MINOR_VERSION', minor_version)
conf.set('SLIRP_MICRO_VERSION', micro_version)

want_ossfuzz = get_option('oss-fuzz')
want_libfuzzer = get_option('llvm-fuzz')
fuzz_reproduce = get_option('fuzz-reproduce')
target_winver = get_option('target_winver')
if want_ossfuzz and want_libfuzzer
  error('only one of oss-fuzz and llvm-fuzz can be specified')
endif
fuzzer_build = want_ossfuzz or want_libfuzzer
if fuzzer_build and fuzz_reproduce
  error('fuzzer build and reproducer build are mutually exclusive')
endif

cc = meson.get_compiler('c')
add_languages('cpp', required : fuzzer_build, native : false)

glib_static = get_option('static')

if glib_static == true
  add_global_arguments('-static', language : 'c')
endif

if cc.get_argument_syntax() != 'msvc'
  r = run_command('build-aux/git-version-gen',
                  '@0@/.tarball-version'.format(meson.current_source_dir()),
                  check : false)

  full_version = r.stdout().strip()
  if r.returncode() != 0 or full_version.startswith('UNKNOWN')
    full_version = meson.project_version()
  elif not full_version.startswith(meson.project_version())
    error('meson.build project version @0@ does not match git-describe output @1@'
          .format(meson.project_version(), full_version))
  endif
else
  full_version = meson.project_version()
endif
conf.set_quoted('SLIRP_VERSION_STRING', full_version + get_option('version_suffix'))

# libtool versioning - this applies to libslirp
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91 for details
#
# - If interfaces have been changed or added, but binary compatibility
#   has been preserved, change:
#      CURRENT += 1
#      REVISION = 0
#      AGE += 1
# - If binary compatibility has been broken (eg removed or changed
#   interfaces), change:
#      CURRENT += 1
#      REVISION = 0
#      AGE = 0
# - If the interface is the same as the previous version, but bugs are
#   fixed, change:
#      REVISION += 1
lt_current = 4
lt_revision = 0
lt_age = 4
lt_version = '@0@.@1@.@2@'.format(lt_current - lt_age, lt_age, lt_revision)

host_system = host_machine.system()

# libslirp_deps collects both the libslirp dependenc(y|ies) and the
# platform dependencies.
libslirp_deps = [ dependency('glib-2.0', static : glib_static) ]

if host_system == 'windows'
  # Windows: pkg-config fails us here because the static version of glib-2.0
  # requires iconv, which is not emitted by 'pkg-config --libs --static'.
  libslirp_deps += [ dependency('iconv', static : glib_static) ]
endif

add_project_arguments(cc.get_supported_arguments('-Wmissing-prototypes', '-Wstrict-prototypes',
                                                 '-Wredundant-decls', '-Wundef', '-Wwrite-strings'),
                      language: 'c', native: false)

cargs = [
  '-DG_LOG_DOMAIN="Slirp"',
  '-DBUILDING_LIBSLIRP',
]

if glib_static == true
  # This likely only applies to Windows... but... need to ensure that
  # glib-2.0 doesn't set import/export defines.
  cargs += [
    '-DGLIB_STATIC_COMPILATION',
    '-DGIO_STATIC_COMPILATION',
    '-DGMODULE_STATIC_COMPILATION',
    '-DGI_STATIC_COMPILATION',
    '-DGOBJECT_STATIC_COMPILATION'
  ]
endif

if host_system == 'windows'
  libslirp_deps += [
    cc.find_library('ws2_32'),
    cc.find_library('iphlpapi')
  ]

  if target_winver != ''
    cargs += '-DTARGET_WINVER=@0@'.format(target_winver)
  endif
elif host_system == 'darwin'
  libslirp_deps += [
    cc.find_library('resolv')
  ]
elif host_system == 'haiku'
  libslirp_deps += [
    cc.find_library('network')
  ]
endif

if cc.check_header('valgrind/valgrind.h')
  cargs += [ '-DHAVE_VALGRIND=1' ]
endif

sources = [
  'src/arp_table.c',
  'src/bootp.c',
  'src/cksum.c',
  'src/dhcpv6.c',
  'src/dnssearch.c',
  'src/if.c',
  'src/ip6_icmp.c',
  'src/ip6_input.c',
  'src/ip6_output.c',
  'src/ip_icmp.c',
  'src/ip_input.c',
  'src/ip_output.c',
  'src/mbuf.c',
  'src/misc.c',
  'src/ncsi.c',
  'src/ndp_table.c',
  'src/sbuf.c',
  'src/slirp.c',
  'src/socket.c',
  'src/state.c',
  'src/stream.c',
  'src/tcp_input.c',
  'src/tcp_output.c',
  'src/tcp_subr.c',
  'src/tcp_timer.c',
  'src/tftp.c',
  'src/udp.c',
  'src/udp6.c',
  'src/util.c',
  'src/version.c',
  'src/vmstate.c',
]

mapfile = 'src/libslirp.map'
vflag = []
libvflag = []
if cc.has_link_argument('-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), 'src/libslirp.test.map'))
  libvflag += '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
endif

if fuzzer_build
  cargs += '-fsanitize-coverage=edge,indirect-calls,trace-cmp'
  cargs += '-fsanitize=fuzzer-no-link,address'
  cargs += '-fprofile-instr-generate'
  cargs += '-fcoverage-mapping'
  cargs += '-g'
  cargs += '-DSLIRP_DEBUG'
  vflag += '-fsanitize=fuzzer-no-link,address'
  vflag += '-fsanitize-coverage=edge,indirect-calls,trace-cmp'
  vflag += '-fprofile-instr-generate'
  vflag += '-fcoverage-mapping'
endif
if fuzz_reproduce
  cargs += '-DSLIRP_DEBUG'
  cargs += '-g'
endif

install_devel = not meson.is_subproject()

configure_file(
  input : 'src/libslirp-version.h.in',
  output : 'libslirp-version.h',
  install : install_devel,
  install_dir : join_paths(get_option('includedir'), 'slirp'),
  configuration : conf
)

if glib_static or fuzzer_build or fuzz_reproduce
  lib = static_library('slirp', sources,
    c_args : cargs,
    link_args : libvflag + vflag,
    link_depends : mapfile,
    dependencies : libslirp_deps,
  )
else
  lib = library('slirp', sources,
    version : lt_version,
    c_args : cargs,
    link_args : libvflag + vflag,
    link_depends : mapfile,
    dependencies : libslirp_deps,
    install : install_devel or get_option('default_library') == 'shared',
  )
endif

pingtest = executable('pingtest', 'test/pingtest.c',
  link_with: [ lib ],
  c_args : cargs,
  link_args : vflag,
  include_directories: [ 'src' ],
  dependencies : libslirp_deps
)

test('ping', pingtest)

ncsitest = executable('ncsitest', 'test/ncsitest.c',
  link_with: [lib],
  c_args : cargs,
  link_args : vflag,
  include_directories: ['src'],
  dependencies: libslirp_deps
)

test('ncsi', ncsitest)

if install_devel
  install_headers(['src/libslirp.h'], subdir : 'slirp')

  pkg = import('pkgconfig')

  pkg.generate(
    version : version,
    libraries : lib,
    requires : [
      'glib-2.0',
    ],
    name : 'slirp',
    description : 'User-space network stack',
    filebase : 'slirp',
    subdirs : 'slirp',
  )
else
  if get_option('default_library') == 'both'
    lib = lib.get_static_lib()
  endif
endif

libslirp_libdep = declare_dependency(
  link_with : lib,
  include_directories : [include_directories('src'), include_directories('.')],
)
meson.override_dependency('slirp', libslirp_libdep)

subdir('fuzzing')
