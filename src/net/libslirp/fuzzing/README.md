# Fuzzing libslirp state and instructions

## Current state
We chose to use libFuzzer because of its custom mutator feature, which allows to keep coherent informations inside the packets being sent to libslirp. This ease the process of fuzzing as packets are less likely to be rejected early during processing them.

In the current state, the `meson.build` file is not compatible with the original one used by libSlirp main repository but it should be easy to merge them in a clean way. Also **in the current state, it seems that there is a memory leak inside the fuzzing code**, which make it run out of memory. The current goal is to find and get rid of this leak to allow fuzzing for longer without the process being interrupted because of it.

Six harness are currently available, more are to be added later to focus on other parts of the code :

- **fuzz-ip-header** : the mutator focuses on the ip header field informations,
- **fuzz-udp** : the mutator only work on udp packets, mutating the udp header and content, or only one or the other (-h,-d),
- **fuzz-tcp** : the mutator targets tcp packets, header+data or only one or the other, or only one or the other (-h,-d),
- **fuzz-icmp** : the mutator focuses on icmp packets,

These harness should be good starting examples on how to fuzz libslirp using libFuzzer.

## Running the fuzzer

Building the fuzzers/harness requires the use of clang as libFuzzer is part of LLVM.
You can build it running :

`CC=clang meson build && ninja -C build`

It will build the fuzzer in the ./build/fuzzing/ directory.

A script named `fuzzing/coverage.py` is available to generate coverage informations. **It makes a lot of assumptions on the directory structure** and should be read before use.

To run the fuzzer, simply run some of:

- `build/fuzzing/fuzz-ip-header fuzzing/IN_ip-header`
- `build/fuzzing/fuzz-udp fuzzing/IN_udp`
- `build/fuzzing/fuzz-udp-h fuzzing/IN_udp-h`
- `build/fuzzing/fuzz-tftp fuzzing/IN_tftp`
- `build/fuzzing/fuzz-dhcp fuzzing/IN_dhcp`
- `build/fuzzing/fuzz-icmp fuzzing/IN_icmp`
- `build/fuzzing/fuzz-tcp fuzzing/IN_tcp`

Your current directory should be a separate directory as crashes to it. New inputs found by the fuzzer will go directly in the `IN` folder.

# Adding new files to the corpus

In its current state, the fuzzing code is taking pcap files as input, we produced some using `tcpdump` on linux inside qemu with default settings.
Those files should be captured using the `EN10MB (Ethernet)` data link type, this can be set with the flag `-y` but it seems this can't be done while listening on all interfaces (`-i any`).
New files should give new coverage, to ensure a new file is usefull the `coverage.py` script (see next section) can be used to compare the coverage with and without that new file.

# Coverage

The `coverage.py` script allows to see coverage informations about the corpus. It makes a lot of assumptions on the directory structure so it should be read and probably modified before running it.
It must be called with the protocol to cover: `python coverage.py udp report`.
To generate coverage informations, the following flags are passed to the fuzzer and libslirp :

- g
- fsanitize-coverage=edge,indirect-calls,trace-cmp
- fprofile-instr-generate
- fcoverage-mapping

The last 2 arguments should also be passed to the linker.

Then the `llvm-profdata` and `llvm-cov` tools can be used to generate a report and a fancy set of HTML files with line-coverage informations.
