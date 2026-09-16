# Scoping & Design: #578 — Modernize EOS VM OC for modern LLVM (drop ORCv1, opaque pointers)

**Issue:** eosrio/spring #578 "use ORCv2 for compatibility with modern LLVM" (labels: `lgtm`, `tech-debt`)
**Component:** EOS VM OC — the optimizing WASM→native compiler tier. **Its output is the exact native machine code that executes consensus-affecting contract logic on-chain.**
**Branch line:** 1.2.x (fork of AntelopeIO/spring), C++20.
**Effort rating:** **L (Large)** — dominated by validation, not coding. Honest estimate: **~2–4 focused engineering weeks of code (P1–P6), then a hard-gated, non-parallelizable differential-correctness/determinism cycle (P7) that is the calendar long pole** before merge.

> One-line correction to the issue title: it says "use ORCv2," but the correct fix is to **bypass ORC entirely** (see §3.1). The title is a trap (§7, R6).

---

## 1. Summary & recommendation

**Recommendation: DO NOW — but isolate it.** This is the right fix, the technical direction is settled, a directly applicable reference implementation exists, and the payoff (native OC on modern distros + dropping a second full LLVM build) is real and recurring. **However, schedule it as its own release, not interleaved with in-flight consensus work (Savanna/finality)** — so the historical-replay integrity-hash gate (§6.5) has clean attribution if anything diverges.

Two non-negotiable conditions gate the merge, both elevated from the draft after code review:

1. **A blob self-containment (relocation) scan is a P1 BLOCKING gate, not a P7 nicety.** OC harvests a finalized PIC blob, copies it out of the process, and a *different* process later `mmap`s it `PROT_EXEC` — there is **no load-time relocation** to catch a bad blob. The single most probable failure mode is an LLVM-synthesized `memcpy`/`memset`/`__udivti3`-class builtin surviving as an *unresolved external relocation* (the same root cause as the ORCv2 dead-end, but it can bite the RuntimeDyld path too). You cannot trust *any* contract run on the new path until you have proven zero external builtin/GOT relocations per blob.
2. **The RTTI/PIC tension in the reproducible build must be resolved before the "stop building LLVM twice" payoff is claimed** (§4 / §7 R9). OC requires an RTTI-on LLVM and pinllvm built it PIC-off; the shared toolchain LLVM is RTTI-off and PIC-default-on. These must be reconciled together, and making the *shared* toolchain RTTI-on changes the clang/lld that builds **all** of spring.

If those two conditions cannot be met in the target window, **defer** — do not ship a partially-validated consensus codegen path. **Harvest-reference: already located** (Wire-Network/wire-sysio, §5) — no further search needed; harvest the modern code, add our own guards.

**Effort: L.** Coding confidence is high (all required type information is statically available; the file is fully analyzed; a working reference exists). Total-timeline confidence is medium — validation depth and limit-tuning surprises carry the unknowns.

---

## 2. Why it matters (the payoff)

OC compiles smart-contract WASM to native code; the build today **hard-requires LLVM 7–11** (`CMakeLists.txt:68` FATAL_ERRORs unless `7 <= LLVM_VERSION_MAJOR < 12`). Modern Ubuntu (24.04 ships llvm-14..20; 26.04 ships llvm-17..22), Debian, and Fedora no longer package llvm-11, so native builds there must disable OC. Verified in-tree: `.cicd/platforms/ubuntu24.Dockerfile:24` and `ubuntu26.Dockerfile:25` both `set(ENABLE_OC OFF CACHE BOOL "" FORCE)`, with comments citing #578 directly. Those nodes run only the slower interpreter / `eos-vm-jit` tiers, never OC.

- **Native OC re-enabled on modern distros.** Build OC against the distro-shipped, distro-signed `llvm-18-dev` instead of a hand-built EOL-2021 LLVM 11. Removes the standing `-DENABLE_OC=OFF` workaround and the whole "can't build with OC on a modern OS" class of friction. OC is the **fast, consensus-critical native tier** — modern-distro nodes stop being stuck on the interpreter/JIT.
- **Pinned reproducible build stops compiling LLVM twice** — *conditional on §4 resolving the RTTI/PIC tension*. The reproducible build (`tools/reproducible.Dockerfile`) today builds clang/LLVM **18.1.8** as the toolchain compiler **and** a separate **llvm 11.1.0 "pinllvm"** solely so OC's `find_package(LLVM)` resolves to 11. Fixing #578 *can* let OC reuse the toolchain LLVM and delete pinllvm — estimated ~30–40% reduction in the image's non-spring compile time (~20–40 min wall, ~1–2 GB intermediates, a ~70 MB source tarball + its GPG signature eliminated), shrinking build cost and trusted-input surface. **Caveat (honest):** if the RTTI/PIC reconciliation forces keeping a separate RTTI-on/PIC-off LLVM-libs-only build for OC, this headline payoff is **partially recovered, not fully** — we would still drop the duplicate *clang/lld* build but retain a slimmer LLVM-libs build. This is an open risk, not a guaranteed win.
- **Future-proofing.** Bypassing ORC removes OC from the line of ongoing ORC API churn; the stable `RuntimeDyld`/MC/`computeSymbolSizes` surface avoids a repeat migration on the next LLVM major. The only remaining future item is an eventual JITLink port (R8).
- **Codebase simplification.** Removes the `==7` ORCv1 alias shim and the per-file `gnu++17` override; sets up deletion of the legacy 7–11 `#ifdef` branches once the modern path is proven.

---

## 3. Technical scope

### 3.1 JIT layer — `LLVMJIT.cpp` (339 lines): bypass ORC, do NOT migrate to ORCv2 · effort ~3–5 days

OC does **not** use ORC as a JIT in any meaningful sense. Verified from the source:
- `NullResolver` ⇒ **zero** external-symbol resolution (intrinsics/host calls are reached at runtime via the control-block GS-segment offsets, not the dynamic linker).
- `emitAndFinalize` ⇒ **eager one-shot** compile, not lazy/on-demand.
- Output is **copied out of the process** (memfd → code cache → later `mmap PROT_EXEC` in a *different* process). The harvested buffer must be a **complete, relocated, self-contained PIC image** — there is no second relocation at load time.

ORC is doing only two real jobs: gluing `SimpleCompiler` to `RuntimeDyld`, and firing the symbol-harvest callback. Both are trivially replaced by calling `SimpleCompiler::operator()` + `llvm::RuntimeDyld` directly and running the harvest loop inline. `RuntimeDyld` + `RTDyldMemoryManager` + `computeSymbolSizes` are retained through LLVM 18–20 (they back MCJIT), so the custom `UnitMemoryManager` and the contiguous-PIC-blob strategy carry over unchanged.

**A full ORCv2 LLJIT port is the wrong target and is actively harmful** (R6): the reference impl tried ORCv2 first and abandoned it — a bare `JITDylib` fails to materialize the `memcpy`/`memset` symbols LLVM codegen injects ("Failed to materialize symbols"), surfacing as **compile failures on specific contracts.** ORCv2 wants to own and execute memory in-process — the opposite of OC's harvest-and-ship model.

**New object-emission idiom** (replaces ES/VModuleKey/CompileLayer/LegacyRTDyldObjectLinkingLayer):

```cpp
llvm::orc::SimpleCompiler compiler(*targetMachine);          // Orc/CompileUtils.h, retained LLVM 12–20
auto objBuffer = llvm::cantFail(compiler(*llvmModule));
auto objFile   = llvm::cantFail(llvm::object::ObjectFile::createObjectFile(objBuffer->getMemBufferRef()));
llvm::RuntimeDyld dyld(*unitmemorymanager, *unitmemorymanager);   // MemoryManager AND resolver
dyld.setProcessAllSections(true);                            // PRESERVE — needed so .stack_sizes is materialized
auto loadedObjInfo = dyld.loadObject(*objFile);
dyld.resolveRelocations();
unitmemorymanager->finalizeMemory();
// existing computeSymbolSizes harvest loop runs INLINE here,
// using loadedObjInfo->getSectionLoadAddress(*section) instead of the ORC NotifyFinalized arg.
```

**Preserved exactly** (semantically load-bearing): `EngineBuilder().selectTarget` with `PIC_` + `CodeModel::Small` + `EmitStackSizeSection=1`; the per-function legacy `FunctionPassManager`; `UnitMemoryManager`'s contiguous-blob layout; `setProcessAllSections(true)`; the `.stack_sizes` `DataExtractor` harvest + `stack_size_limit` `_exit(1)` enforcement; byte-offset (not pointer) harvesting.

**Mechanical API fixes:** drop `createConstantPropagationPass()` (removed 12; reference dropped it — codegen-determinism implication, R2); confirm/treat `createJumpThreadingPass` identically against 18; `Support/Host.h` → `TargetParser/Host.h` (17+); `F_Text` → `OF_Text`; `reserveAllocationSpace` alignment `u32` → `llvm::Align` (12+); delete the `==7` alias block; drop the per-file `gnu++17` override.

**TWO NEW SYMMETRIC COUNT ASSERTIONS REQUIRED (net-new, both unguarded silent-failure paths under a compiler change):**

- **`function_to_offsets.size() == module.functions.defs.size()`.** Verified: the symbol harvest at `LLVMJIT.cpp:191` matches by symbol *name* via `getFunctionIndexFromExternalName` and has **no count check** — unlike the stack-sizes path which *does* (`if(num_functions_stack_size_found != module.functions.defs.size()) _exit(1)`, verified at `LLVMJIT.cpp:327–328`). If LLVM 18 internalizes/relocalizes emitted functions or strips them from the object symbol table, the harvest silently finds **fewer** functions → missing `function_to_offsets` entries → calls jump to offset 0/garbage. Add the symmetric assertion.
- **Re-validate the `.stack_sizes` binary encoding/offset-width against LLVM 18's actual emission.** The `DataExtractor` parse (`LLVMJIT.cpp:313`, the `<10` uint32-vs-uint64 `#ifdef`) is an LLVM-version-coupled binary parse. If 18 emits the section differently, or `setProcessAllSections(true)` is dropped, the count check at :327–328 fires and **every contract aborts the node** — a node-availability cliff, not just a correctness issue.

### 3.2 IR emitter / opaque pointers — `LLVMEmitIR.cpp` (~62KB): the bulk of the work · effort ~1–1.5 weeks (~150–300 lines)

55 typed-pointer API calls break under opaque pointers (default LLVM 15, only mode 17+): `getPointerElementType ×3`, `CreateLoad ×17`, `CreateInBoundsGEP ×7`, `CreateBitCast ×10`, `getPointerTo ×15`, `CreateStore ×10`, `CreateCall ×2`, plus `getBasicBlockList()` (removed 16). Core principle: `getPointerElementType()` is **gone with no recovery** — element types must be **tracked** out-of-band. The WASM validator already statically guarantees every needed type, so this is **disciplined plumbing, not redesign.**

- **Pointer construction:** `T->getPointerTo(256)` → `llvm::PointerType::get(context, 256)`. **Address space 256 survives and MUST be preserved** — it is the GS-segment sandbox (11 sites; do an explicit per-site AS256 audit).
- **`getPointerElementType ×3` (set_local, tee_local, set_global):** track, don't recover. Locals: `cast<AllocaInst>(localPointers[i])->getAllocatedType()`. Globals: new parallel `std::vector<llvm::Type*> globalValueTypes` populated in the globals loop, read in get/set_global.
- **`CreateLoad(ptr) → CreateLoad(Ty, ptr)`:** types known per-site (intrinsic/control-block reads = `i64`; current_memory = `i32`; WASM loads = the `EMIT_LOAD_OP` macro's `llvmMemoryType`; locals/globals = tracked type).
- **`CreateInBoundsGEP ×7`:** memory GEPs use `i8` (the explicit `CreatePointerCast` to `memoryType->getPointerTo(256)` is **deleted** — GEP with `i8`, load with the explicit type); table GEPs are the most error-prone.
- **`createCall` helper — the single highest-severity site.** Verified at `LLVMEmitIR.cpp:351–354` it does the now-illegal `cast<PointerType>(Callee->getType())->getElementType()`. Rewrite as two overloads `createCall(FunctionType*, Value* Callee, Args)` and `createCall(Function*, Args)`; change `getLLVMIntrinsic` return type to `Function*`. **At the `call_indirect` site this is critical:** the callee is an `IntToPtr` of a *computed* offset (verified: `CreateIntToPtr(offset, functionPointerType)` at the indirect path), so there is **literally no pointee to recover** — the `FunctionType` MUST be threaded in explicitly from `asLLVMType(calleeType)` (the typed `functionPointerType` that vanishes under opaque pointers). Getting this wrong produces a wrong-ABI native call that mis-reads the stack.
- **`call_indirect` type-check is POINTER-IDENTITY, not structural — preserve byte-for-byte.** Verified: the runtime type-check stores the WASM function type as a raw pointer literal (`emitLiteralPointer(calleeType, llvmI8PtrType)`) and compares with `CreateICmpNE(...)` (verified at the indirect path). This token MUST remain an `i8*` identity value; if the opaque-pointer rewrite changes its type/representation it silently breaks indirect-call type checking — a consensus trap path.
- **Misc:** `getBasicBlockList().push_back(bb)` → `bb->insertInto(fn)`; `setAlignment` takes `llvm::Align`.
- **Security invariants preserved byte-for-byte:** the `coerceByteIndexToPointer` 32→64 `CreateZExt` (prevents sign-extension sandbox escape); `setVolatile(true)` on guest memory load/store (prevents reorder/elimination); softfloat intrinsic dispatch through the AS256 table.

### 3.3 Build / CMake · effort ~1 day + reproducible/CI follow-ons

- **Three EXACT-version gates must move together** or EosioTester consumers break: `CMakeLists.txt:68`, `EosioTester.cmake.in`, `EosioTesterBuild.cmake.in`. Change atomically; tester smoke-build in CI.
- Drop `orcjit`+`passes` from the component list → `(support core mcjit native)` (+ `DebugInfoDWARF` for testers).
- Remove `gnu++17` per-file overrides.
- **Cache-version bump is a HARD correctness requirement, and it is cheap — one byte.** Verified: `current_codegen_version = 2` is a single `constexpr` (`eos-vm-oc.hpp:47`), serialized into the per-contract `code_descriptor`, and **already drives automatic per-node invalidation** (`code_cache.cpp:343`: `if(cd.codegen_version != current_codegen_version)` drops the descriptor). So changing `2` → `3` makes stale LLVM-11 blobs auto-invalidate per-node with zero migration story — caches are **not consensus state** and are per-node-local. **Forgetting this = mixing LLVM-11 and LLVM-18 blobs in one cache = divergence.** Do it in P1 alongside the codegen change so no test run ever mixes versions. (This retires the draft's worry about a "rollout/migration story for node code caches" — moot; the mechanism is automatic.)

---

## 4. Build/reproducible-build hazard: RTTI **and** PIC must be reconciled together

The draft flagged RTTI; the more complete picture (verified):

- pinllvm is built `-DLLVM_ENABLE_RTTI=On ... -DLLVM_ENABLE_PIC=Off` (verified `reproducible.Dockerfile:101`).
- The toolchain LLVM build (verified `reproducible.Dockerfile:75–76`) has neither flag — i.e. **RTTI-off and PIC default-on**.
- OC links LLVM **static libs into `eosio_chain`** (the consensus library) and requires RTTI-on LLVM.

Mixing a PIC-off expectation with a PIC-on LLVM, **and** linking RTTI-on LLVM libs against an otherwise-RTTI-off codebase, risks ODR/typeinfo and relocation-model surprises at link time. **P5 must reconcile PIC and RTTI together, not RTTI alone.**

**Unresolved tension (state it honestly):** adding `-DLLVM_ENABLE_RTTI=On` to the *shared* toolchain build changes the **clang/lld that build all of spring**, not just OC's link — a reproducible-build-input change beyond a hash re-baseline that could alter the produced compiler's behavior/ABI. The two ways out:
- **(a)** Accept and validate the toolchain-wide RTTI change (re-baseline hash, regression-test the produced binaries), or
- **(b)** Build a **separate RTTI-on / PIC-reconciled LLVM-libs-only install for OC** — which re-introduces *some* of the duplication P5 wants to remove and **partially undercuts the headline "stop building LLVM twice" payoff.**

This must be decided before merge (§8, decision 5). The honest framing: the payoff is real but its *magnitude* depends on this decision.

---

## 5. Reference implementation status: HARVEST (located — do not go greenfield)

A complete, directly applicable reference exists: **Wire-Network/wire-sysio** migrated the *identical WAVM-derived* OC compiler from LLVM 11 → 18.1.6. That codebase merged Spring+Savanna consensus in Nov 2025, so it tracks the Spring 1.x line closely.

- Single migration commit **`bf54efd10`** (2026-02-08): opaque pointers throughout `LLVMEmitIR.cpp`; `createCall`/`CreateLoad`/`CreateInBoundsGEP` adaptations; `globalValueTypes`; "Refactored JIT to use RuntimeDyld, removing deprecated ORC v1 API."
- Write-up `LLVM_18_MIGRATION.md` documents the failed-ORCv2 / chose-RuntimeDyld decision, limit re-tuning, and test results (`contracts_unit_test` 71/0 errors, `unit_test --sys-vm` 871/0 errors).
- Files to diff: `.../sys-vm-oc/LLVMJIT.cpp` (322 lines) and `LLVMEmitIR.cpp` (1370 lines). Directory/intrinsic prefixes renamed (`eos-vm-oc`→`sys-vm-oc`, `eosvmoc_internal`→`sysvmoc_internal`) but codegen is structurally identical.

**Caveat:** wire-sysio targets LLVM 18 **only** (FATAL if `< 18`); it *deleted* the old ORCv1 + `LLVM<10` shims rather than straddling. So it is a reference for "what the modern code looks like," **not** a drop-in for our guarded transition (§9). Harvest the modern code, add our own guards. **No upstream AntelopeIO PR resolves #578** (open since 2025-06-26) — there is nothing better to wait for.

**Why pin LLVM 18 (single target, not a 12–18 range).** eosrio's concrete need is to build OC on Ubuntu 24.04 (llvm-14..20) and 26.04 (llvm-17..22); the apt-installable intersection on **both** is llvm-18 (arguably -19/-20). A *range* buys eosrio nothing it needs and multiplies the consensus-validation surface — **every `#if LLVM_VERSION_MAJOR` branch is a distinct codegen path requiring its own differential + replay validation**, which for consensus code is the expensive part. Pin **one** compiler tier per release. Drop any "validated window 14–20" language from anything implying *release support*: **release-certify exactly 18; treat others as best-effort-builds.**

---

## 6. Consensus-correctness testing strategy (the safety net)

OC output is the exact native bytes executed on-chain. The invariant is **not** "bit-identical machine code" — OC output may differ from prior OC output across an LLVM bump (OC results are not consensus-persisted; pin the LLVM version per release). The invariant is: **observable WASM behavior — results, trap kind/point, linear-memory final state — is identical to the interpreter tier, and all nodes on a given release agree.**

**Re-scoped from the draft (a per-runtime matrix ALREADY exists — do not "stand up a harness").** Verified: `unittests/CMakeLists.txt:98` loops `foreach(RUNTIME ${EOSIO_WASM_RUNTIMES})` and creates `<suite>_unit_test_eos-vm-oc` **and** `<suite>_unit_test_eos-vm` for every suite; `wasm-spec-tests/generated-tests/CMakeLists.txt:24` does the same for the WASM spec conformance suite. Identical fixtures already run under OC and the interpreter today. The genuinely **net-new** work is narrower: (a) cross-comparing OC-vs-interpreter on **non-fixture / fuzzed** inputs, (b) the **blob reloc-scan**, (c) the **historical-replay integrity-hash** tooling.

1. **Differential OC vs interpreter — primary gate (extend the existing matrix).** Assert identical **observable behavior**: return values, trap kind/point, linear-memory final state. **CORRECTED:** do **NOT** assert CPU-billing equality across tiers. The interpreter bounds CPU by counting executed WASM ops against a deadline; OC bounds CPU by a wall-clock checktime timer/trap injected into native code — different mechanisms **by design**. `checktime_tests` already run under each runtime and assert deadline/cpu-exceeded *behavior*, not billing equality. Asserting billing equivalence would produce false failures; it is out of scope.
   - Corpus: `unittests/wasm-spec-tests` (the canonical semantics oracle), full `unit_test` + `contracts_unit_test` under `--runtime eos-vm-oc` vs `--runtime eos-vm`, plus `eosvmoc_limits_tests`, `wasm_tests`, `wasm_config_tests`.
2. **Cross-LLVM IR byte-diff (isolate semantic change from version noise).** On a common LLVM where both old and new emitter code compile, byte-diff the emitted IR pre/post migration. Any delta must be an *intended* opaque-pointer/pass change. This is the single most powerful tool for catching a wrong tracked element type in `LLVMEmitIR.cpp`.
3. **Determinism / pass-pipeline check.** Dropping `createConstantPropagationPass`/`createJumpThreadingPass` changes optimized IR → codegen. Prove via (1)+(2) that no *observable* semantics changed. Bump the codegen version (§3.3) so old blobs invalidate.
4. **Self-contained-PIC-blob / builtin-reloc scan — ELEVATED TO A P1 BLOCKING GATE.** `objdump`/reloc-scan every emitted blob to assert **no GOT and zero external `memcpy`/`memset`/`__udivti3`-class builtin relocations** survive, on the **exact pinned LLVM 18**, across a corpus of contracts that do **bulk memory ops** (the likely trigger). Verified, this gate is genuinely net-new — no existing tree test scans relocations. The concrete trigger is compiler-rt builtins: the small-constant `memcpy` fast-path falls through to a real call for non-constant sizes, and LLVM codegen can *independently* synthesize `@llvm.memcpy`/`@llvm.memset` (struct-copy, loop-idiom recognition) inside any function; with no resolver, RuntimeDyld leaves these as unresolved external relocs → the harvested blob jumps to garbage with **no load-time relocation to catch it.** Also assert `.stack_sizes` count == `module.functions.defs.size()` and the new `function_to_offsets` count assertion (§3.1). **This must pass before any contract is trusted on the new path** — it is a prerequisite for trusting P1, not a P7 checklist item.
5. **Historical chain replay with integrity-hash compare — strongest end-to-end gate, but it does NOT stand alone.** Replay real chain history on an LLVM-18 OC node; compare the **state integrity hash** against a known-good interpreter/legacy-OC node at matching heights. A divergent hash at any height = a semantics bug; do not merge until clean over a long real window. **CAVEAT:** replay only exercises opcode/type combinations that *actually occurred* in that window. It **must be paired** with the `wasm-spec-tests` conformance corpus **and** a targeted **synthetic** corpus for high-risk paths that may never appear in mainnet history — `call_indirect` type-token mismatches, exotic table layouts, the `memcpy` intrinsic path, OOB/trap/div0/overflow edges — or it gives false confidence.
6. **Targeted security/sandbox tests.** Deep-recursion still trips `stack_size_limit` (`_exit(1)`); OOB access still traps (verifies AS256 + the `coerceByteIndexToPointer` zext survived); softfloat ops (`min`/`max`/`ceil`/`floor`/`div0`) still route to host softfloat intrinsics via the AS256 table (cross-platform FP determinism).

**CORRECTED noise triage.** The `eosvmoc_interrupt_tests` interrupt-counter assertion is **likely waivable**, not "consensus-adjacent." Verified: it asserts `post_count == pre_count + 1` on `get_eos_vm_oc_compile_interrupt_count()` — it counts **async compile-monitor** interruptions, and the test's own comment warns it can spuriously fail if the 5000ms window "was not long enough for oc compile to complete." It is a background-compiler timing/flakiness test, **not** a per-instruction checktime/consensus counter; the reference's regression here is almost certainly timing. The **genuinely** consensus-adjacent path — the in-code checktime **trap** on long-running contracts — is covered by `checktime_tests` / deep-recursion and is where the scrutiny belongs. (The `deep_mind` log-comparison test may fail on whitespace/encoding and is likely cosmetic.)

---

## 7. Phased plan

| Phase | Scope | Effort | Gate to advance |
|---|---|---|---|
| **P0 — Extend the existing differential matrix** (re-scoped) | The per-runtime OC-vs-interpreter matrix **already exists** (`unittests/CMakeLists.txt:98`, `wasm-spec-tests :24`). Net-new only: (a) cross-input/fuzz OC-vs-interpreter compare, (b) the **reloc-scan tool**, (c) historical-replay integrity-hash tooling. Archive golden artifacts (OC `.code` blob sizes/hashes, baseline reproducible-build hash). | **~2–3 days** (down from "3–4 days to stand up a harness") | Extended matrix + tooling green on the unchanged LLVM-11 tree; goldens archived. |
| **P1 — `LLVMJIT.cpp`: bypass ORC + the gates that make P1 trustworthy** | Replace ORCv1 with `SimpleCompiler` + `RuntimeDyld` direct (§3.1); inline harvest loop; mechanical API fixes; add `#if` guards so the 7–11 path still compiles. **In P1 (moved earlier): bump `current_codegen_version` 2→3; add `function_to_offsets` count assertion; re-validate `.stack_sizes` parse on 18.** | **~40–70 lines; ~3–5 days** | **BLOCKING: reloc/builtin scan (§6.4) green** — zero external `memcpy`/`memset`/GOT relocs across the bulk-mem corpus on pinned LLVM 18. Smoke contract compiles & runs; `.stack_sizes` + `stack_size_limit _exit(1)` fire on deep recursion; both count assertions hold. |
| **P2 — `LLVMEmitIR.cpp`: opaque pointers** | The heavy item. Thread explicit element types through all 55 sites (§3.2); add `globalValueTypes`/local-type tracking; rewrite `createCall` (two overloads); preserve AS256, the call_indirect i8* identity token, zext, volatile. | **~150–300 lines; ~1–1.5 weeks** | Zero `getPointerElementType` remain; full unittests pass on 18; IR byte-diff vs reference shows only intended changes. |
| **P3 — Limits re-tuning** | LLVM 18 emits larger blobs (reference saw ~60 B/function larger). Re-tune `generated_code_size_limit` / `stack_size_limit`; re-run `eosvmoc_limits_tests`. | **~1–2 days** | `eosvmoc_limits_tests` green; no spurious `_exit(1)` under load. |
| **P4 — CMake / build plumbing** | Move all three EXACT gates atomically (`CMakeLists.txt:68` + both `EosioTester*.cmake.in`); component list → `(support core mcjit native)` (+`DebugInfoDWARF` testers); remove `gnu++17` overrides. | **~20–30 lines; ~1 day** | Native build with `llvm-18-dev`; EosioTester consumers configure (CI smoke). |
| **P5 — Reproducible build: drop 2nd LLVM + reconcile RTTI/PIC** (can run in parallel with P7; does NOT block correctness) | Resolve §4: choose (a) toolchain-wide RTTI-on (validate the clang/lld change) or (b) a separate RTTI-on/PIC-reconciled LLVM-libs install for OC. Delete pinllvm if (a). Re-baseline reproducible-build hash. | **~15–25 lines + decision; ~1–3 days** | Reproducible image builds; new hash recorded; RTTI **and** PIC reconciled; link clean into `eosio_chain`. |
| **P6 — Platform Dockerfiles & CI matrix** (**MUST follow P7 sign-off**) | Remove `ENABLE_OC OFF` from `ubuntu24.Dockerfile:24` & `ubuntu26.Dockerfile:25` (keep 26's `CMAKE_POLICY_VERSION_MINIMUM 3.5` — unrelated cmake-4 fix, do not entangle); `apt install llvm-18-dev`; re-point `.github/workflows/llvm.yaml` matrix to ~18. | **~1 day** | OC-enabled native CI green on 24/26 — **only after** P7 sign-off, never before (or CI gates merges on an unvalidated consensus path). |
| **P7 — Consensus validation (the long pole)** | Full §6: extended differential corpus + fuzz, IR byte-diff, synthetic high-risk corpus, **historical replay integrity-hash compare**. | **Calendar-dominant; not parallelizable with merge.** | All §6 gates green; sign-off. |

**Sequencing notes:** (1) the reloc-scan is a **P1 exit gate** (prerequisite for trusting any run), not P7; (2) the codegen-version bump is a **P1** correctness prerequisite so no run mixes v2/v18 blobs; (3) P5 is build-infra and runs in parallel with/after P7; (4) P6 (flip `ENABLE_OC ON` in CI) comes **after** P7 only. **Isolate this migration to its own release** — do not interleave with Savanna/finality consensus work, so the replay integrity-hash gate has clean attribution.

**Total realistic estimate:** ~2–4 weeks coding (P1–P6), then a hard-gated, non-parallelizable P7 cycle before merge. Coding confidence **high**; total-timeline confidence **medium**.

---

## 8. Risks & mitigations

| # | Risk | Severity | Mitigation |
|---|---|---|---|
| **R1** | **Consensus divergence** — any codegen/typing bug makes OC nodes disagree with interpreter/other nodes → chain split. | **Critical** | Entire §6, gated. Differential corpus + IR byte-diff + historical-replay integrity-hash compare paired with synthetic high-risk corpus. Non-negotiable merge gate. |
| **R2** | Dropping `createConstantPropagationPass`/`createJumpThreadingPass` changes optimized IR → different codegen. | High | Prove semantics-neutral via §6(1)+(2). Bump codegen version (auto-invalidates stale blobs). |
| **R3** | Wrong tracked element type in opaque-pointer rewrite silently mis-loads/mis-GEPs on specific contracts. | High | Per-callsite review vs reference; IR byte-diff (§6.2); broad corpus. Highest-leverage: `createCall` helper, **`call_indirect` (no pointee to recover — FunctionType MUST be threaded from `asLLVMType`)**, `coerceByteIndexToPointer`. |
| **R4** | LLVM 18 emits larger blobs → spurious `generated_code_size_limit`/`stack_size_limit` `_exit(1)` node aborts. | High | P3 re-tune limits; re-run `eosvmoc_limits_tests`; load-test. (AntelopeIO hit this before, PRs #902/#1016.) |
| **R5** | Losing AS256, the zext, `setVolatile`, **or the call_indirect i8\* identity token** during the rewrite silently breaks the sandbox / OOB protection / indirect-call type check. | High | Preserve byte-for-byte (confirmed in reference); §6(4)+(6) verify AS256 relocs and OOB trapping; explicit per-site AS256 audit (11 sites); the `CreateICmpNE` token stays i8* identity, not structural. |
| **R6** | Following the issue title → ORCv2 → bare `JITDylib` can't materialize `memcpy`/`memset` → compile failures on specific contracts. | High (avoidable) | **Bypass ORC** (§3.1). Documented dead-end; do not attempt LLJIT. |
| **R7** | **LLVM-synthesized `memcpy`/`memset`/builtin survives as an unresolved external reloc** in the harvested blob (more likely than a generic GOT reloc; same root cause as R6, also bites RuntimeDyld). | **High** | **P1 BLOCKING reloc/builtin scan (§6.4)** on bulk-mem corpus, exact LLVM 18. No load-time relocation exists to catch this. |
| **R8** | RuntimeDyld is in maintenance mode (JITLink is the successor); a future LLVM could drop it. | Low (near-term) | Acceptable in the 12–20 window. Log as follow-up tech-debt: a later JITLink port. |
| **R9** | RTTI/PIC mismatch: OC needs RTTI-on (PIC-reconciled) LLVM libs; making the shared toolchain RTTI-on changes the clang/lld that builds **all** of spring → reproducible-input change; may force keeping a 2nd LLVM-libs build (undercuts payoff). | **Medium-High** | §4 decision before merge: (a) validate toolchain-wide RTTI change + re-baseline hash, or (b) separate RTTI-on/PIC-reconciled LLVM-libs install for OC. Reconcile PIC **and** RTTI together. |
| **R10** | Silent harvest under-count: LLVM 18 internalizes/strips emitted function symbols → `function_to_offsets` missing entries → calls jump to garbage (currently unguarded, unlike the stack_sizes check). | High | **New `function_to_offsets.size()==defs.size()` assertion (P1)**; re-validate `.stack_sizes` encoding/offset-width on 18. |
| **R11** | Three EXACT-version gates must move together or EosioTester consumers break. | Medium | P4 changes `CMakeLists.txt:68` + both `EosioTester*.cmake.in` atomically; tester smoke-build in CI. |
| **R12** | Dropping the 2nd pinned LLVM changes reproducible-build inputs → released binary hash changes. | Medium | P5 re-baseline build hash deliberately; handle with R9 reconciliation. |

---

## 9. Open decisions for eosrio (sign-off)

1. **Permanent floor after transition:** keep a guarded 7–11 legacy path for one release, or cut straight to 18-only like the reference? **Recommendation:** guard for one release **as build-only/dev convenience, explicitly NOT release-certified** (else you recreate the multi-codegen validation cost you are avoiding), then delete.
2. **Validated ceiling:** **Recommendation — release-certify exactly 18**; treat 14..20 as best-effort-builds, not pinned/supported. Drop "validated window" language from anything implying release support.
3. **Replay window:** how many blocks of real history constitute "enough" for the integrity-hash gate (R1)? Decide the floor; remember replay must be **paired** with the synthetic high-risk corpus (§6.5).
4. **ubuntu20/22:** move to `llvm-18-dev` now, or leave on `llvm-11-dev` short-term under the widened gate?
5. **RTTI/PIC strategy (blocks the headline payoff, §4):** (a) toolchain-wide RTTI-on (accept the clang/lld input change + re-baseline) or (b) a separate RTTI-on/PIC-reconciled LLVM-libs build for OC (partially keeps the duplication). Decide before claiming "stop building LLVM twice."
6. **Release isolation:** confirm this ships as its own release, not interleaved with Savanna/finality work, so the replay integrity-hash gate has clean attribution.