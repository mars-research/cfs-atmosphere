# Notes for agent sessions

NASA cFS ported to the Atmosphere microkernel. `README.md` has the design, the
layout, and the limits; read it first. This file is what a session needs to
build, test, and change things without relearning it.

## Repositories

- This repository holds the port: the OSAL and PSP layers, the C library, the
  C runtime (`rt/`), the tests, and the scripts.
- `atmosphere-smp/` is a submodule: `mars-research/atmosphere-smp`, branch
  `main`, the concurrent Atmosphere kernel. It is our clone of
  `FeizaiYiHao/VeriFlat`, which is the remote `upstream` there and moves
  daily. The sources, the crates, and the notes inside still say VeriFlat.
  We work off the clone and commit to it. Its `main` is upstream `main`
  (`516f22f5`) plus two commits: the `no-pcid` build feature and
  `NR_DEBUG_TSC_HZ`.
- `cFS/` is a submodule: `nasa/cFS`, unmodified. Do not edit it. The only
  thing the build puts inside it is the symlink `cFS/psp/fsw/pc-atmosphere`,
  which `tools/build-cfs.sh` creates.
- Until November 2026 the port ran on the earlier kernel,
  `mars-research/atmosphere`; the runtime of that time is in its
  `user/atmo-rt`. The last commit of this repository on that kernel is
  `3e399dc`.

## Environment

Use the nix shell. It is tested on macOS (Apple silicon); Linux is declared
but not tested.

    nix develop                 # interactive
    nix develop -c <command>    # one command

- Nix sees only files that git tracks. `git add` a new file before
  `nix develop`, or the flake will not find it.
- Rust comes from rustup, not from nix. `tools/env.sh` exports
  `RUSTUP_TOOLCHAIN` with the channel of `atmosphere-smp/boot/rust-toolchain.toml`.
- The kernel build needs `RUSTC_BOOTSTRAP=1`, which
  `atmosphere-smp/boot/.cargo/config.toml` sets when cargo runs in `atmosphere-smp/boot`.
- `tools/get-submodules.sh` fetches the submodules, with a shallow Verus
  inside `atmosphere-smp/`. The kernel build needs only the `vstd` sources of it.

## Build and test

Run from the repository root, inside the shell.

    tools/get-submodules.sh
    tools/build-atmosphere.sh      # loader, kernel, runtime; about 30 s clean
    tools/build-cfs.sh             # out/cfs/cfs-cpu1.img
    tools/test-cfs.sh              # boots cFS and checks the console; about 25 s

    tests/hello/build.sh   && tools/run-qemu.sh out/hello/hello.elf      # exits 42
    tests/rt/build.sh      && tools/run-qemu.sh out/rt/rt_test.elf       # 4191 checks
    tests/preempt/build.sh && tools/run-qemu.sh out/preempt/preempt_test.elf   # 20 checks

    cmake -S tests/osal -B out/osal-tests \
          -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain-x86_64-atmosphere.cmake \
          -DCMAKE_BUILD_TYPE=Release
    cmake --build out/osal-tests -j8
    tools/run-osal-tests.sh        # 19 of 19 programs

    (cd atmosphere-smp/boot && ./run.sh) # the kernel's own self-test, 4 processors
    tools/verify-atmosphere.sh     # Verus: "5017 verified, 0 errors", about 80 s

`tools/run-qemu.sh <image> [seconds]` exits with the image's exit code, 124 on
a timeout, and 125 if QEMU stopped before the image exited (a kernel panic).
It prints the lines of the image and the kernel's warnings; `ATMO_QEMU_RAW=1`
shows the whole serial log. A fault in user code does not stop the kernel:
the thread exits, the log says "the thread exits", and the run times out.

`out/` is build output. After a change of paths or toolchain, delete it; the
CMake caches hold absolute paths.

## Things that cost time before

- **QEMU here has no PCID and no INVPCID.** Software emulation lacks both and
  the default kernel build panics at boot. The scripts build the kernel with the
  feature `kernel/no-pcid` unless `ATMO_QEMU_ACCEL=kvm`. A kernel built by
  hand without the feature does not boot on this host.
- **No I/O ports in user code.** `in` and `out` fault. The clock rate comes
  from the kernel (`NR_DEBUG_TSC_HZ`), the exit from `sys_debug_exit`.
- **Memory needs quota.** `mmap_4k` wants four times the page count in the
  thread's quota before it starts. `rt/src/sys.rs` moves quota from the
  container to the process to the thread and keeps count, since no call
  reads it.
- **Timing checks flake under load.** One check in `tests/rt` bounds a
  wall-clock time. Do not run QEMU tests next to Verus or a build, and rerun
  on an idle host before calling a failure a regression.
- **General-purpose registers only.** Floating point is off for user code;
  the instruction faults and the thread exits. C code is built with
  `-mgeneral-regs-only`, Rust for `x86_64-unknown-none`.
- **The kernel's `dom0` self-test needs 4 processors.** It fails with
  `-smp 1` at `cpu_online`, by design.

## Changing the kernel

- Read `atmosphere-smp/AGENTS.md` first. It sets that repository's rules: no new
  lemmas or model changes without approval, its proof style, delegation of
  edits to subagents, and commits without AI attribution lines.
- So far our commits change trusted code only (`atmosphere-smp/boot`, bodies of
  `external_body` functions). A change to verified code is a bigger step:
  it has to verify, and it should be agreed with the user first. Record a
  need for one in `docs/open-questions.md`.
- After a change: build, run `tools/verify-atmosphere.sh`, the kernel's
  self-test (`atmosphere-smp/boot/run.sh`), the preemption test, the runtime test,
  and `tools/test-cfs.sh`. The first verify run builds Verus in
  `atmosphere-smp/verus/source/target-verus` and downloads Z3.
- The kernel repository's tool tests
  (`python3 -B -m unittest discover -s tests -p 'test_*.py'`) fail in part on
  this Mac at the upstream commit too: two on the invariant matrix, the rest
  on `flock`. Compare with upstream before blaming a change.
- To take newer upstream work: `git -C atmosphere-smp fetch upstream`, then merge
  or rebase `main` onto `upstream/main`, rebuild, and rerun everything. The
  syscall numbers are in `atmosphere-smp/boot/vsys/src/lib.rs`;
  `tests/preempt/preempt_test.c` repeats a few of them.

## Commits and pushes

- Commit only when asked. The author is the user's git identity.
- In this repository the message ends with the Claude co-author trailer. In
  `atmosphere-smp/` it does not: that repository's rules forbid attribution lines.
- A change in `atmosphere-smp/` is committed there first, on `main`, and pushed.
  Then the submodule pointer is committed here. In the other order this
  repository names a commit that GitHub does not have.
- The submodule's `origin` fetches over HTTPS, for anonymous clones, and
  pushes over SSH (`git remote set-url --push origin
  git@github.com:mars-research/atmosphere-smp.git`).
- Agent sessions so far had no GitHub credentials. Check before promising a
  push; otherwise hand the user the commands.
