extra_sources = []
extra_cargs = []
extra_ldargs = []
fuzzing_engine = []


extra_cargs += '-g'
if fuzzer_build
  extra_cargs += '-fsanitize=fuzzer,address'
  extra_cargs += '-fsanitize-coverage=edge,indirect-calls,trace-cmp'
  extra_cargs += '-DCUSTOM_MUTATOR'
  extra_cargs += '-fprofile-instr-generate'
  extra_cargs += '-fcoverage-mapping'

  extra_ldargs += '-fsanitize=fuzzer,address'
  extra_ldargs += '-fprofile-instr-generate'
  extra_ldargs += '-fcoverage-mapping'
endif

deps = [glib_dep, libslirp_dep, platform_deps]

exes = [
        ['fuzz-arp', ['slirp_fuzz_arp.c', 'helper.c']],
        ['fuzz-ip-header', ['slirp_fuzz_ip_header.c', 'helper.c']],
        ['fuzz-udp', ['slirp_fuzz_udp.c', 'helper.c']],
        ['fuzz-udp-h', ['slirp_fuzz_udp_header.c', 'helper.c']],
        ['fuzz-udp-d', ['slirp_fuzz_udp_data.c', 'helper.c']],
        ['fuzz-tftp', ['slirp_fuzz_udp_data.c', 'helper.c']],
        ['fuzz-dhcp', ['slirp_fuzz_udp_data.c', 'helper.c']],
        ['fuzz-tcp', ['slirp_fuzz_tcp.c', 'helper.c']],
        ['fuzz-tcp-h', ['slirp_fuzz_tcp_header.c', 'helper.c']],
        ['fuzz-tcp-d', ['slirp_fuzz_tcp_data.c', 'helper.c']],
        ['fuzz-icmp', ['slirp_fuzz_icmp.c', 'helper.c']],

        ['fuzz-ndp', ['slirp_fuzz_icmp6.c', 'helper.c']],
        ['fuzz-ip6-header', ['slirp_fuzz_ip6_header.c', 'helper.c']],
        ['fuzz-udp6', ['slirp_fuzz_udp6.c', 'helper.c']],
        ['fuzz-udp6-h', ['slirp_fuzz_udp6_header.c', 'helper.c']],
        ['fuzz-udp6-d', ['slirp_fuzz_udp6_data.c', 'helper.c']],
        ['fuzz-tftp6', ['slirp_fuzz_udp6_data.c', 'helper.c']],
        ['fuzz-tcp6', ['slirp_fuzz_tcp6.c', 'helper.c']],
        ['fuzz-tcp6-h', ['slirp_fuzz_tcp6_header.c', 'helper.c']],
        ['fuzz-tcp6-d', ['slirp_fuzz_tcp6_data.c', 'helper.c']],
        ['fuzz-icmp6', ['slirp_fuzz_icmp6.c', 'helper.c']],
      ]

if fuzzer_build
  foreach exe : exes
    executable(
      exe[0], exe[1],
      dependencies : deps,
      c_args: extra_cargs,
      link_args: extra_ldargs,
    )
  endforeach
endif

if fuzz_reproduce
  executable(['reproducer', ['reproducer.c', 'helper.c']],
    dependencies: deps,
    c_args: extra_cargs,
    link_args: extra_ldargs,
  )
endif
