# Notes for agent sessions

NASA cFS ported to the Atmosphere microkernel. `README.md` has the design, the
layout, and the limits; read it first. This file is what a session needs to
build, test, and change things without relearning it.

## Repositories

- This repository holds the port only.
- `atmosphere/` is a submodule: `mars-research/atmosphere`, branch `main`.
  It carries the kernel, the C runtime (`user/atmo-rt`), and the verified
  kernel (`kernel/verified`).
- `cFS/` is a submodule: `nasa/cFS`, unmodified. Do not edit it. The only
  thing the build puts inside it is the symlink `cFS/psp/fsw/pc-atmosphere`,
  which `tools/build-cfs.sh` creates.

## Environment

Use the nix shell. It is tested on macOS (Apple silicon); Linux is declared
but not tested.

    nix develop                 # interactive
    nix develop -c <command>    # one command

- Nix sees only files that git tracks. `git add` a new file before
  `nix develop` or `nix build`, or the flake will not find it.
- Rust comes from rustup, not from nix. It reads
  `atmosphere/rust-toolchain.toml`.
- The scripts set `RUSTC_BOOTSTRAP=1` through `tools/env.sh`. Running `cargo`
  by hand in `atmosphere/` without it fails with "`.json` target specs
  require -Zjson-target-spec".
- Do not use Atmosphere's own flake shell on Apple silicon; it does not
  evaluate there.
- Verus is the flake package `.#verus`, not part of the shell. The first
  build takes about 20 minutes.

## Build and test

Run from the repository root, inside the shell.

    git submodule update --init --recursive
    tools/build-atmosphere.sh      # loader, kernel, runtime; about 30 s clean
    tools/build-cfs.sh             # out/cfs/cfs-cpu1.img
    tools/test-cfs.sh              # boots cFS and checks the console; about 25 s

    tests/hello/build.sh   && tools/run-qemu.sh out/hello/hello.elf      # exits 42
    tests/rt/build.sh      && tools/run-qemu.sh out/rt/rt_test.elf       # 4190 checks
    tests/preempt/build.sh && tools/run-qemu.sh out/preempt/preempt_test.elf

    cmake -S tests/osal -B out/osal-tests \
          -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain-x86_64-atmosphere.cmake \
          -DCMAKE_BUILD_TYPE=Release
    cmake --build out/osal-tests -j8
    tools/run-osal-tests.sh        # 19 of 19 programs

    tools/verify-atmosphere.sh     # Verus: "524 verified, 0 errors"

`tools/run-qemu.sh <image> [seconds]` exits with the image's exit code, 124 on
a timeout, and 125 if QEMU stopped before the image exited (usually a kernel
fault; `ATMO_QEMU_RAW=1` shows the kernel log).

`out/` is build output. After a change of paths or toolchain, delete it; the
CMake caches hold absolute paths.

## Things that cost time before

- **Timing checks flake under load.** Two checks in `tests/rt` bound
  wall-clock time. They fail now and then when the host is busy, on any
  kernel. Do not run QEMU tests next to Verus or a build, and rerun on an idle
  host before calling a failure a regression.
- **QEMU here is software emulation** of x86-64 on an Arm host. Nothing has
  run under KVM or on hardware.
- **General-purpose registers only.** The kernel does not save vector or
  floating-point state. C code is built with `-mgeneral-regs-only`, Rust for
  a target without SSE.
- **`syscall`/`sysret` do not read the GDT, `iret` does.** A wrong selector
  shows up only on the first interrupt taken in user mode.

## Changing the kernel

- After any change under `atmosphere/kernel`: build, run
  `tools/verify-atmosphere.sh`, then the preemption test, the runtime test,
  and `tools/test-cfs.sh`.
- Code under `kernel/verified` must verify. Do not add `assume`, `admit`, or
  `external_body` to get a proof through without asking.
- Check that a new proof is not vacuous: put `assert(false)` at the end of the
  function and confirm that Verus rejects it.
- To iterate on one function:
  `tools/verify-atmosphere.sh --verify-module process_manager::impl_base --verify-function ProcessManager::NAME`.
  Add `--profile` when it runs out of resource limit.
- Proofs are sensitive to the size of the `Thread` type. One more field made
  an unrelated proof (`set_container_mem_quota_mem_4k`) exceed the default
  limit. Run the whole verification after a change to a type, not only the
  function you touched.
- The Verus revision in `flake.lock` must be the one `atmosphere/Cargo.lock`
  pins for vstd. The verify script warns; fix with `nix flake update verus`.
- The verified kernel is synced into `mars-research/atmosphere` from another
  repository. A change under `kernel/verified` has to land there too, or the
  next sync overwrites it.

## Commits and pushes

- Commit only when asked. The author is the user's git identity, and the
  message ends with the Claude co-author trailer.
- A change in `atmosphere/` is committed there first, on `main`, and
  pushed. Then the submodule pointer is committed here. In the other order
  this repository names a commit that GitHub does not have.
- The submodule's `origin` is the HTTPS URL, for anonymous clones. Pushing
  from it needs
  `git remote set-url --push origin git@github.com:mars-research/atmosphere.git`.
- Agent sessions so far had no GitHub credentials. Check before promising a
  push; otherwise hand the user the commands.
