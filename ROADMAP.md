# eosrio/spring — Maintenance ROADMAP (1.2.x stable line)

> **Governing law (decided by eosrio, non-negotiable).** Continue the **1.2.x stable line** — base = `release/1.2` @ **v1.2.2**, the version every production Antelope/Vaulta chain actually runs — as 1.2.3, 1.2.4, … with **ZERO compatibility breaks**: no consensus/protocol-rule change, no ABI/serialization/wire change, no protocol-feature activation, and no breaking change to RPC/SHiP response shapes existing consumers depend on. Backward compatibility for node operators and integrators is the entire point of the fork.
>
> The **v2.0 line was never put into production and is OUT OF SCOPE** (Sync Calls, Events, the 2.0 Deprecations/Removals epic). Issues/PRs that apply ONLY to `release/2.0` are excluded **unless the same defect also affects the 1.2.x code path**.

---

## 1. Context

- **The fork.** eosrio is a production Antelope/Vaulta node operator. `eosrio/spring` is a hard fork of `AntelopeIO/spring`. eosrio's default branch `main` is today a clean, byte-identical mirror of upstream `main` (ahead 0 / behind 0, last pushed 2025-11-12); divergence has not yet started. **The maintenance base, however, is `release/1.2`, not `main`** (see §2).
- **The upstream freeze.** Upstream's last merge was **#1870 on 2025-09-05**; last stable release **v1.2.2 (2025-08-19)**; the 2.0 line stalled at pre-release **v2.0.0-dev1.3 (2025-08-27)**. As of 2026-06, upstream has been **~9 months frozen**. There is a large, clean body of post-freeze production-correctness work (the heifner p2p/finality cluster, supply-chain hardening, test de-flaking) sitting in open PRs against `release/1.2` and `main` that has never been released.
- **The governing decision.** Base on **v1.2.2**, **zero compat breaks**, **2.0 out of scope**. This roadmap is an operating plan to (a) take ownership of the fork, (b) ship the harvestable production fixes as 1.2.3, and (c) keep the line buildable, releasable, and trustworthy for operators — without ever breaking an operator or integrator.

**Priority order (every item is triaged against this):**
1. **Production crashes / data-loss / finality / p2p stability** for operators.
2. **Keep the fork buildable/releasable** on current toolchains, with working CI and a trustworthy supply chain.
3. **Safe, compatible** operability / perf / quality-of-life.
4. **Tech-debt.**

Anything compatibility-breaking is rejected or parked in the **2.0-fork appendix (§9)**.

---

## 2. State of upstream & branch topology

| Branch | Version | Position vs `main` | Role for the fork |
|---|---|---|---|
| `main` | **1.3.0-dev** (CMake VERSION 1.3.0-dev) | — (896 commits ahead of `release/1.2`) | Reference / integration only. **Not** the production base. |
| `release/1.2` | **v1.2.2** (VERSION_PATCH 2) | **2 ahead / 896 behind `main`** | **THE production base.** Cut eosrio's stable line from here; tag next release **1.2.3**. |
| `release/2.0` ≈ `release/2.0.0-dev2` | 2.0.0-dev2 | 653 ahead / 16 behind | **Archive/reference only.** Sync Calls + Events feature work; never in production. |
| `release/2.0.0-dev1` | 2.0.0-dev1 | 561 ahead / 154 behind | Archive only. |
| `release-1.2-vaulta-rename` | — | 1 ahead / 1061 behind (stale, 2025-05-07) | Archive only; the Vaulta rename is a break (see §6, §9). |

**Base-drift correction (important — do not repeat the draft's error).** `release/1.2` is **not** a "byte-identical mirror of upstream `main`." Only eosrio's *default* branch `main` is a clean mirror of upstream `main`. The **maintenance base is `release/1.2` @ v1.2.2**, which already carries **2 commits beyond its fork point and is 896 commits behind `main`** (because `main` = 1.3.0-dev has moved on). A deliberate decision is required on whether the fork ever pulls any of `main`'s 896 commits onto the stable spine (recommendation: **no** — backport individually; see §6).

**What was in flight at the freeze:** Savanna finality/liveness hardening (#1887/#1888), p2p connection-churn and sync-stall fixes (#1879/#1881/#1883/#1884/#1885), EOS VM OC replay/interrupt correctness (#1880/#1881/#1882), supply-chain hardening (#1841/#1845, reproducible-build work #1842/#1840), and CI de-flaking (#1873/#1876/#1877). The 2.0 feature line (Sync Calls = 16 items, Events = 33 items) **never shipped to production** and is excluded wholesale.

---

## 3. How to read this

**Effort:** `S` ≤ a day · `M` a few days · `L` ≥ a week / cross-cutting.

**Priority phase tags:** `P0-unfreeze` (Phase 0 gate) · `P1-stability` (Phase 1) · `P2-operability` (Phase 2) · `P3-techdebt` (Phase 3).

**Compatibility scope tags (the load-bearing classification):**
- **in-scope** — provably no consensus/ABI/wire/RPC-shape change; landable on 1.2.x.
- **gated** — in-scope *only* behind a stated gate (preserve a response shape, prove bit-equivalence, opt-in flag, snapshot-version decision). Must not land without satisfying the gate.
- **out-2.0** — applies only to the never-shipped 2.0 feature line. Appendix §9.
- **out-breaking** — would break consensus/ABI/wire/RPC-shape or activate a protocol feature. Rejected on 1.2.x; appendix §9.

> **Coverage guarantee.** Every open PR (21) and every P0 (4) / P1 (21) issue from triage is accounted for here — in a phase table, gated, referenced via its fixing PR, or in the appendix. The appendix totals at the end are exact counts, not estimates.

---

## 4. Ready-to-land PRs (harvested clean production work)

Clean, compat-verified work harvested from upstream's post-freeze effort. All confirmed safe for 1.2.x (no consensus/ABI/wire/RPC-shape break). Anchors spot-verified against the `release/1.2` checkout.

> **✅ STATUS (validated locally, 2026-06).** This entire cluster is already cherry-picked onto branch `eosrio-1.2` (v1.2.2 + 11 commits) and validated with the pinned reproducible toolchain: clean build, **zero real test regressions** (2103/2110; remaining failures are the 6 pre-existing env/flaky baseline tests + transient timing/FS flakes that pass in isolation). #1888's `locks_out_branch_of_test` and `test_net_plugin` both pass. Landed: #1888, #1881 (→#1880), #1879 (→#1878), #1875, #1818, #1742, #1885a, plus net_plugin #1883/#1884/#1885b. This is the harvested production batch ready for a 1.2.3 release candidate once CI exists (§0.1).
>
> **⚠️ Backport rule — net_plugin logger divergence.** `main` split net_plugin logging into categorized loggers (`p2p_conn_log`/`p2p_blk_log`/`p2p_trx_log`/`p2p_log`) that **do not exist on release/1.2** (single `logger`; `auto_bp_peering.hpp` uses `self()->get_logger()`). A clean `git cherry-pick` of any `net_plugin` change from `main` can still **compile-fail** by introducing an undeclared logger (hit on #1879, #1885b). Backport policy: after cherry-picking any net_plugin change, `grep 'p2p_[a-z]\+_log'` the touched files, remap to `logger`/`self()->get_logger()`, and compile before trusting it.

| PR | What | Effort | Compat | Backport target | Action |
|----|------|:--:|:--:|---|---|
| **#1888** | **[1.2.3]** Apply blocks mid-production when branch locked out by strong QC | S | none | **`release/1.2` (clean)** | **Land.** This IS the 1.2.3 backport of the #1887 finality fix; helper `locks_out_branch_of()` + gate adapted to 1.2; all finality APIs present at `producer_plugin.cpp:922`; applies clean. |
| **#1887** | Apply-blocks-mid-production (main-line parent) | S | none | **n/a — base=`main`; review only** | **Review the consensus/safety argument here once, ship via #1888.** #1887 is **never** "landed" on `release/1.2`. |
| **#1881** | Forbid EOS VM OC tier-up interrupt during replay (resolves **#1880**) | S | none | `release/1.2` (clean) | **Land.** Fixes WAX/aaroncox replay corruption under disable-replay-opts (auto-on with chain-state-history). Anchors: `replaying` atomic `controller.cpp:994`; OC guard `wasm_interface_private.hpp:173`. |
| **#1879** | Signal controller to apply blocks if sync interrupted (resolves **#1878**) | S | none | `release/1.2` (clean) | **Land.** Real WAX sync-stall fix; one idempotent `process_blocks()` post (no recursion). `process_blocks()` confirmed `producer_plugin.hpp:149`. |
| **#1875** | Avoid UB (`static_cast` in WASM `fix_call_index` injector) | S | none | `release/1.2` (clean) | **Land.** UB removal in consensus-critical contract-injection path; reads the same field, output unchanged. File: `libraries/chain/include/eosio/chain/wasm_eosio_injection.hpp` (the table's `wasm_interface_private.hpp:173` anchor was a copy-paste error). Matters under clang21/c++20. |
| **#1818** | Avoid invalidated `flat_map` iterator in `get_supported_protocol_features` | S | none | **`release/1.2` — cherry-pick REQUIRED, then close PR** | **Cherry-pick `fd2b9d544` (1 line), then close as superseded.** Verified: fix is **absent** on `release/1.2` and the UB (`res.first->second` after recursive `emplace`) **is present**. The cherry-pick is **not optional**. |
| **#1742** | Value-init atomic `timer_state_t` (clang-20 / libstdc++15) | S | none | `release/1.2` (apply 1 line by hand) | **Land manually.** Build-portability; semantically identical to existing init. Struct grew 2→3 members; touched line unchanged. heifner+greg7mdp approved. |
| **#1845** | Pin SHA-256 of LLVM/clang/cmake tarballs in reproducible build | S | none | `release/1.2` (cherry-pick from `main`) | **Land.** Supply-chain defense-in-depth; build image only (`reproducible.Dockerfile`). Prereq #1843 merged. Bump hashes in lockstep with version ARGs. |
| **#1841** | **[1.2.3]** Validate hash of Debian pinned repo (`pinned.pl` apt method) | S | none | **`release/1.2` (clean)** | **Land after focused security review of `pinned.pl`** (it sits in the build trust chain). Already targets `release/1.2`; builds on merged #1834. |
| **#1884** | P2P: BP-gossip disconnect (stop stale reconnect churn) | S | none | `release/1.2` (clean) | **Land.** *Correction to earlier triage:* gossip auto-peering machinery IS on `release/1.2` (verified `auto_bp_peering.hpp:477/507/591`). One atomic flag; helps 1.2.x BP operators. |
| **#1883** | P2P: skip reconnect on existing duplicate; add `connection_id` | S | low | `release/1.2` (clean) | **Land.** `connection_id` is a **purely additive** status field (no removal/rename). Fix typo `remvoe` on land. |
| **#1885** | P2P: reduce duplicate-trx debug-log spam (libfc `to_string` cleanup) | S | low | `release/1.2` (adapt) | **Land the net_plugin one-line-log change**; libfc signature change optional. Confirm reworked `to_detail_string` timeout-catch path compiles on the hot logging path. |
| **#1874** | Use `system_timer` instead of deprecated `deadline_timer` (producer timing) | S | low | `release/1.2` (adapt, single file) | **Land after sign-off.** Block-production timing precision (deadline_timer "fires seconds late"); drops deprecated boost dep. Verify `expires_at` across NTP/wall-clock (system_clock non-steady); get spoonincode/greg7mdp sign-off. |
| **#1877** | Test: serialize writes to `subprocess_results.log` | S | none | `release/1.2` (adapt) | **Land.** Test-harness only; approved by greg7mdp. CI signal. |
| **#1876** | Test: wait for block 2 + poll `checkPulse` (de-flake `cli_test`) | S | none | `release/1.2` (adapt) | **Land.** Test-only flakiness removal; helpers present on 1.2. |
| **#1873** | Test: block-in-round calc + fix `getBlockProducer` TypeError | S | none | `release/1.2` (adapt) | **Land.** Test-harness only; de-flakes `production_pause_max_rev_blks`; fixes latent `getBlock` TypeError. |

> **Bundle the heifner post-freeze cluster** (#1879 / #1881 / #1883 / #1884 / #1885 / #1888 + tests #1873 / #1876 / #1877, with #1887 reviewed alongside) into **one review pass** to avoid double-rebasing the shared `net_plugin` / `producer_plugin` / `libfc` regions.
>
> **Test PRs #1873/#1876/#1877 are tagged P0-unfreeze** because they are CI-enablers, but note they only produce signal **once runners exist** (Phase 0.1). They land in the same review pass; they do not gate Phase 0.1 itself.

---

## Phase 0 — Take control / unfreeze the fork  `[P0-unfreeze]`

**Goal:** a fork eosrio actually owns and can build, test, sign, and release independently. **Nothing in Phase 1+ is trustable until CI runs and the supply chain is owned. This is the hard gate.**

### 0.1 Stand up CI — **HARD BLOCKER**
- **Problem (verified):** every build/test workflow targets ENF self-hosted runners — labels `self-hosted`, `enf-x86-beefy`, `enf-x86-hightier`, `enf-x86-midtier`, `enf-x86-lowtier` confirmed across `build.yaml`, `build_base.yaml`, `llvm.yaml`, `performance_harness_run.yaml`, `ph_backward_compatibility.yaml`, `pinned_build.yaml`, `release.yaml`, `submod.yaml`. **eosrio has no access; CI will not run as-is.**
- **Action:** **(a)** provision eosrio self-hosted runners with **matching labels** for the heavy C++ build + integration/perf jobs (fastest path; keeps workflows verbatim, no re-validation of the whole pipeline); **(b)** port only light/lint jobs to GitHub-hosted runners. Effort **L**, compat none (infra).
- **CI-re-run triage (strictly downstream of 0.1 — these are P2/test signal, NOT Phase-1 stability deliverables, and must not be treated as landable before CI is green):** once runners are up, re-run and root-cause if reproducible: **#1844** (`get_account_test`, likely flaky), **#1866** (`eosvmoc_interrupt_unit_test` — OC engine, matters if real), **#1867** (`nodeos_late_block_test` — overlaps #1887/#1888 finality work), **#1852** (`transition_to_if_lr` Trace API 404 — possible real `trace_api` indexing bug on shared code), **#1822** (`checktime_unit_test` under OC — clusters with #1880/#1882). Fix before relying on green CI for a release.

### 0.2 Remove Antelope-internal CI tooling  `S · compat none`
- Delete/replace `.github/workflows/jiraIssueCreator.yml` (Antelope JIRA integration — useless to eosrio).
- Replace `.github/workflows/label_new_issues.yaml` (Antelope-org label automation) with eosrio's labels, or remove.

### 0.3 Branch / version strategy  `S (policy) · compat none`
- Set **`release/1.2` (v1.2.2) as the working base**; tag the next release **1.2.3**. `main` (1.3.0-dev) is reference/integration only; `release/2.0*` is archive only. **Do not** rebase the stable spine onto `main` (see §6).
- **#1862** — Make cpack tarballs reproducible (stable file ordering) so they can be signed release assets. Self-contained, no upstream PR. `S · compat none`.

### 0.4 Reproducible builds + supply-chain base  `compat none`
- **#1842** — Reproducible builds fail when built from scratch (milestoned v1.2.3) — **critical for the first independent signed release**. Needs a clean-tree repro to capture the actual error. `M`.
- **#1840** — Decide the reproducible-build base (unpinned vs pinned Debian snapshot vs **self-mirrored snapshot — recommended**) and Debian version. Policy a fork must own early. Pairs with #1841/#1845. `S (policy)`.
- Land supply-chain PRs **#1841** (Debian InRelease validation, already targets `release/1.2`) and **#1845** (LLVM/clang/cmake tarball hash pinning) — see §4.

### 0.5 Fork/vendor the orphaned AntelopeIO submodules  `M · compat none (sources identical at fork point)`
- **Verified `.gitmodules` on `release/1.2`** (14 submodule stanzas; the "6 of 12" figure in recon is stale — count doesn't change the conclusion). **6 point at unmaintained AntelopeIO repos:** `libraries/appbase`→AntelopeIO/appbase; `libraries/eos-vm`→AntelopeIO/eos-vm; `libraries/softfloat`→AntelopeIO/berkeley-softfloat-3; `libraries/cli11/cli11`→AntelopeIO/CLI11; **`libraries/libfc/libraries/bn256`→AntelopeIO/bn256 (Savanna finality pairing crypto — SECURITY-CRITICAL)**; **`libraries/libfc/libraries/bls12-381`→AntelopeIO/bls12-381 (BLS finality signatures — SECURITY-CRITICAL)**.
- **Action:** fork all 6 into eosrio, re-point submodules, pin commit SHAs. **Prioritize `bls12-381` + `bn256`** — the fork's finality security is rooted on dead upstreams. Leave the well-maintained third-party deps (rapidjson, secp256k1, prometheus-cpp, boost, boringssl) alone. (No `llvm` submodule exists yet — confirms #1860 is genuine net-new work, see Phase 3.)

### 0.6 Governance files (none exist — verified)  `S · compat none`
- Add **SECURITY.md** (eosrio disclosure contact + the **process** decided in §6 — embargo, CVE assignment, coordination for vulns found in vendored dead-upstream crypto), **CONTRIBUTING.md**, **CODEOWNERS** (route finality/crypto/p2p reviews to qualified reviewers).
- **#1859** — CI hard-fail tests touching real `~/eosio-wallet` / `~/.local/share/eosio` (HOME-override guard). Cheap dev-safety net; do it here. `S · compat none`.

### 0.7 Signing / release pipeline  `M · compat none`
- Stand up eosrio binary signing + attestation for the 1.2.3 artifact, on top of reproducible builds (#1842) and cpack ordering (#1862). **Decide the trust root first** (see §6): key custody, rotation, Sigstore/cosign vs GPG, where attestations are published.

---

## Phase 1 — Production stability & security (operator-facing)  `[P1-stability]`

Land the harvestable production-correctness PRs (Phase 0 CI must be green to validate) plus fix the P0/P1 bugs affecting 1.2.x. **Every item is compat-safe (no consensus/ABI/wire/RPC-shape change) unless explicitly gated.**

### 1.A Harvested production-correctness PRs — details in §4
- **#1888** (ship) / **#1887** (review only) — Savanna finality/liveness: producer stops manufacturing doomed-orphan blocks when a strong QC locks out its branch. *Why:* deterministic block-loss on live Savanna (Vaulta) chains — observed BP orphaning 11/12 blocks twice daily. *Compat:* none (changes **when** a producer applies already-valid blocks, not validity).
- **#1881** (resolves **#1880**), **#1879** (resolves **#1878**), **#1875**, **#1818** (cherry-pick + close), **#1742** (build).

### 1.B P0/P1 production bugs affecting 1.2.x

| Issue/PR | Pri | Effort | Why | Compat |
|---|:--:|:--:|---|---|
| **#1689** | P1 | M | `irreversible_block` signal fired **before** block-log write + forkdb-root advance → plugins (`net_plugin update_chain_info`, likely SHiP) read stale LIB. Reorder emission; regression-test other signal handlers. | in-scope (node-internal; no wire/shape change) |
| **#180** | P1 | M | SHiP-log write failure during fork-switch corrupts fork DB and **hard-crashes nodeos** (`controller.cpp` + `state_history_plugin.cpp`). Pairs with test **#948**. | in-scope (no response-shape/wire change) |
| **#222** | P1 | M | Data-correctness race: SHiP log pruning vs a block being streamed → corrupt data sent to clients. Internal guarding fix. | in-scope (SHiP wire/format unchanged) |
| **#455** | P1 | M-L | Fork-DB reversible-block memory bounding to prevent **OOM crash** on current Savanna fork-db. Design-stage. | in-scope — **hard gate: verify current fork-db doesn't already mitigate before building** (a wrong fix here is exactly the finality risk the fork must avoid). |
| **#1882** | P1 | M | Concurrent read-only trx on eos-vm can **deadlock** via spurious `timeout_exception` (`platform_timer` not actually expired). Port Wire-Network `platform_timer::set_expired()` across POSIX/kqueue/ASIO + strengthen `read_only_trx_test`. Same subsystem as #1822/#1880. | in-scope (internal) |
| **#1780** | P1 | S | `get_table_rows` `next_key` points at the **already-returned** row on deadline interrupt → duplicate row on next page. One-line `++itr; break`. Confirmed in current `chain_plugin walk_table_row_range`. Affects **every paginating consumer**. *(This is the real issue number; the draft mislabeled it #1690, which is a P3 item — see Phase 3.)* | in-scope (**fixes a bug**; does not change a correct shape) |
| **#615** | P1 | M | `get_table_by_scope` pagination can **loop forever** when a scope has >`limit` tables (DoS-ish on API nodes). | **gated:** naive fix changes the meaning of `more`/`lower_bound` → **must preserve response shape** so clients treating them as names don't break. |
| **#1857** | P1 | L | Migrate `modexp` host function **libgmp→BoringSSL** (removes the last system-GMP consensus dep; supply-chain + determinism). **Sole home for this item** (Phase 3 only cross-references it). | **gated:** `modexp` output is **consensus-critical** — MUST be proven **bit-equivalent** to GMP via differential tests + perf benchmarks before landing, else it forks the chain. |
| **#91** | P1 | L | Savanna finality-violation **test coverage** (light-client builds a verifiable violation proof). | in-scope (test-only). **Pairs with #610, which is parked** (cannot compile on the fork — see §9). Park unless eosrio commits to the slashing-proof epic. |
| **#1842 / #1840 / #1652 / #1670** | P1 | M / S | Reproducible-build + supply-chain hardening (also Phase 0.4). #1652 = evaluate `LIBCXX_HARDENING_MODE=extensive` (currently `fast`, `reproducible.Dockerfile:64`; benchmark perf first); #1670 = bump `_FORTIFY_SOURCE` 2→3 (`reproducible.Dockerfile:78`). | in-scope (build only) |

> **SHiP cluster (#180 / #222 / #272 / #948 / #1011):** treat **#180 + #222** as the production crash/corruption fixes; **#272** (suspected off-by-one in prune math, P2) is cheap to verify alongside #222; **#948** (test: nodeos shuts down on SHiP-log write failure, P1) and **#1011** (corruption tests for pruned/split logs, P2) guard the fail-fast behavior — do #1011 **after** the **#1001** corrupted-log-handling strategy settles.

### 1.C Gated finality-safety items (defer unless an incident surfaces)
- **#543** — fsi update from **all** QCs (not only strong) before first vote during sync. Node-local fsi refinement, **not** a protocol-rule/wire change, but high risk: needs deep finality expertise to prove it can't change vote outcomes. **Hard gate: verify-first**, then defer.
- **#779** — Savanna voting-eligibility (only vote on `fork_db` best-branch blocks). Discussion-only; likely partly resolved in shipped `decide_vote`. **Verify against current code first**, then defer.

---

## Phase 2 — Safe operability, perf & API (compatible only)  `[P2-operability]`

Additive/opt-in only. **Nothing here removes/renames an existing field, changes wire/ABI, or activates a protocol feature.**

| Issue/PR | Effort | Why | Compat / gate |
|---|:--:|---|---|
| **#1610** | M | Persist subjective-billing queue across restarts so producers don't relearn the naughty list for hours. Strongly operator-wanted (Greymass/EOSUSA). | in-scope (node-local, opt-in config, corrupt-ignore; subjective billing is non-consensus) |
| **#1754** | M | Built-in peer blacklist: `p2p-peer-blacklist` (incl. CIDR) + optional net API. Strong operator-security win; ship **config-only first**. | in-scope (purely additive config + *optional* endpoints; no existing shape changed) |
| **#1855** | M | Opt-out of server-side ABI decoding on `send_read_only_transaction` (`return_json:false`-style flag) → halve payload + save decode CPU. Wharf/Greymass-wanted. | in-scope (**additive opt-in flag**; default unchanged) |
| **#798** | S | Snapshot load ignores CTRL-C; nodeos only exits after multi-minute load. Cooperative-shutdown check in controller snapshot-init loop (~`controller.cpp:2435`). | in-scope (operability only) |
| **#1605** | M | Install WASM expiration/interrupt handler **once per thread** vs per-action attach/detach → deadline-path correctness-hardening + mild perf. | in-scope (deadline behavior preserved). **Sequence** with OC/executor work (#578); entangled with 2.0 multi-executor #1394 (parked). |
| **#214** | S | `spring-util` to replace proposer/finalizer keys (Savanna `replace_finalizer_keys`) via snapshot edit. Operator/testnet bootstrap tool. | in-scope. Coordinate with snapshot work (#1203/#438). |
| **#438** | M | Multi-thread snapshot creation to speed generation. | in-scope (output format unchanged). Discussion-only; lower priority than load-ordering #1203. |
| **#1293** | S | Swagger/OpenAPI docs for the already-shipping `get_consensus_parameters` endpoint. | in-scope (doc on existing endpoint). |
| **#1808** | S | Fix broken doc links/rendering in `chain_api_plugin` API-reference docs. | in-scope (docs-only, reproduced). |
| **#1757** | S | keosd/cleos option to sign non-canonical signatures (follow-on to merged #1708). | in-scope, **gated** behind the protocol feature being active on-chain; canonical-by-default retained. |
| **#1846** | S | Relax the hard assert forbidding read-only-threads on producer nodes (`producer_plugin.cpp:~1505`) to ease dev/testing. | **gated:** guard exists for real read-window-vs-production correctness → gate behind `test_mode`/explicit opt-in flag, NOT a blanket warning, so it can't mask misconfig on live producers. |
| **#1203** | M | Order rows in snapshot to cut load time (~7m → ~1m30s). | **gated:** alters snapshot hash (proposed snapshot v8.1) + reduces some validation → land **only with an explicit snapshot-version-bump decision**; integrators comparing snapshot hashes are affected. |
| **#1105** | M | Opt-in API mode for non-stringified big numbers (BigInt). DX nice-to-have. | **gated:** MUST preserve stringified default; opt-in only. |
| **#1379** | L | Make `get_table_by_scope` `more`/`lower_bound` round-trip. | **gated:** the **ABI-format-extension route is OUT** (serialization change → §9). In-scope **only** as a no-ABI-change API-side workaround (encoding hint). |

**Operability/CI signal & test-hygiene (P2):** **#272** (SHiP prune off-by-one), **#1011** (SHiP corruption tests), **#1822 / #1866 / #1867 / #1844 / #1852** (re-run on fork CI; root-cause if real — strictly downstream of Phase 0.1, per §0.1). #1859 already landed in Phase 0.6.

---

## Phase 3 — Tech-debt & toolchain modernization  `[P3-techdebt]`

Keep-buildable-on-current-toolchains (pillar #2) + internal hygiene. No compat surface unless noted. **Governed by the supported-toolchain matrix decided in §6.**

| Issue/PR | Effort | Why | Note |
|---|:--:|---|---|
| **#578** | L | Refactor EOS VM OC off deprecated LLVM **ORCv1 → ORCv2** so the node builds on modern LLVM 12+ (ORCv1 removed in LLVM 12; legacy llvm-11 disappearing from distros). | **Top-2 pillar.** Internal compiler machinery; no consensus/wire impact when output equivalent. Pairs with #1860/#1861/#1605. STATUS (done/validated 2026-06, branch `eosrio-1.2-oc-llvm18`): chose **bypass-ORC** (SimpleCompiler + RuntimeDyld AOT harvest, dual-path so one source compiles on LLVM 7–11 AND 14+); OC compiles + **701/702 OC unit tests pass**; **consensus-validated** by syncing ~48k real Jungle4 blocks through LLVM-18 OC with zero `transaction_mroot` divergence; reloc-safety gate never fired (self-contained blobs). Pinned/reproducible build wired to OC-LLVM-18 (pinllvm 11→18); native OC re-enabled on Ubuntu 24.04/26.04 via system llvm-18. NOTE: the scope doc's hoped-for "drop the 2nd pinned LLVM" payoff is **infeasible** — the toolchain's own LLVM is libstdc++-linked + RTTI-off, so OC (libc++ + RTTI-on) can't reuse it; pinllvm stays. Shipped the safe simplification instead: deduped the now-identical LLVM source download (one 18.1.8 tarball serves both builds) + version guard. Remaining for production sign-off: longer/mainnet-scale replay. |
| **#1860** | M | Vendor LLVM as a git submodule (confirmed **no `llvm` submodule** in `.gitmodules`). | **Prerequisite/blocker for Ubuntu 24 (#1861).** Build-system only. |
| **#1861 (+26.04)** | M | Add Ubuntu **24.04 and 26.04** support incl. CI plumbing (new `.cicd/platforms/ubuntu24.Dockerfile` + `ubuntu26.Dockerfile`; CI today only covers ubuntu20/22). Operators migrating to 24.04/26.04 LTS. | Build/packaging only. **Sequence after #1860 / #578.** STATUS (done/verified 2026-06): `.cicd/platforms/ubuntu24.Dockerfile` + `ubuntu26.Dockerfile` ADDED. Native unpinned build **SUCCEEDS on 24.04 (gcc 13) and 26.04 (gcc 15)** with `-DENABLE_OC=OFF` — Spring 1.2.x compiles clean on both new compilers; smoke + jit unit tests pass. 26.04 additionally needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` (cmake 4.2 rejects vendored boost's pre-3.5 `cmake_minimum_required`). The *pinned/reproducible* `.deb` also installs + runs on both (glibc 2.39/2.43 forward-compat). REMAINING gap = EOS VM **OC** on native 24/26 (needs llvm-11/ORCv1 → blocked on #578/#1860; full-OC available today only via the pinned build) + wiring these platforms into CI (build.yaml matrix + packaging/test jobs). |
| **#1857 (xref)** | — | `modexp` libgmp→BoringSSL. | **Lives in Phase 1.B** (consensus-critical, differential-test gate). This is a cross-reference only — do not double-track. |
| **#1856** | M | Plumb deadline timer into `mod_exp` so the subjective bit-size limit can be removed. | **gated** on #1857; removing a subjective limit changes which inputs are accepted → consensus-relevant if accept/reject changes. Coordinate. |
| **#1692** | M | Normalize `net_plugin` time reps onto a single `tstamp = std::chrono::nanoseconds`. | Maintainability/type-safety; no behavior/wire change. **Order after** heifner p2p PRs to avoid conflicts. |
| **#1830** | S | Swap remaining asio `deadline_timer` for `system_timer` (std::chrono). | Mechanical, low-risk. Land opportunistically; relates to #1874. |
| **#522** | S | Make softfloat `float128_t` usage explicit (namespace/alias) to avoid type confusion. | Maintainability; no behavior/wire change. |
| **#929** | L | C++20 coroutine support in appbase executor so SHiP stops bypassing the priority queue. | Large core-plumbing risk; **defer behind correctness PRs.** No external shape change. |
| **#232 / #1332** | M | WASM spec test overruns 8-min CI limit (#232); restore ability to regenerate WASM spec-conformance tests (#1332, generator lost). | Test-infra only; matters mainly if touching the WASM runtime. Low priority. |
| **#1690** | S | Update `replace_producer_keys` to also swap **finalizer** keys (Savanna; TODO at `controller.cpp:1105`). | **Test/dev utility**, not on production block path; no consensus/wire change. *(This is the sole, correct home for #1690 — the draft wrongly double-used this number for the P1 `get_table_rows` bug, which is actually #1780.)* |
| **#611** | S | After Savanna activation, optionally reject pre-`proto_savanna` peers in `net_plugin`. | Largely moot (pre-Savanna peers gone). Minor hygiene, low value. |
| **#305 / #1001 / #1424 / #1425** | S-M | `spring-util` light-proof-server bootstrap (#305, needs scoping); decide `spring-util` block-logs **deprecate-vs-fix** (#1001); block-log compression research (#1424); replay-without-undo-session research (#1425). | Tooling/research; **scope before acting.** #1001 unblocks #1011. |

---

## 6. Strategic & governance decisions

1. **CI hosting.** ENF self-hosted runners are gone (labels confirmed across 8 workflows). **Recommendation:** provision eosrio self-hosted runners with **matching labels** for the heavy C++ build + integration/perf jobs (fastest path; keeps workflows verbatim and avoids re-validating the pipeline); port only light/lint jobs to GitHub-hosted. **This is the #1 Phase-0 blocker — no fix can be validated until CI runs.**

2. **Branch/version strategy & upstream tracking.** Make **`release/1.2` (v1.2.2)** the production base and ship 1.2.3 / 1.2.4 / … Keep `main` (1.3.0-dev) as reference/integration only and `release/2.0*` as archive only. **Do NOT adopt the 653-commit 2.0 line** and **do NOT rebase the stable spine onto `main`'s 896 commits.** Backport upstream's post-freeze fixes individually onto `release/1.2`. *(`release/1.2` is already 2 ahead / 896 behind `main`; treat `main` strictly as a cherry-pick source.)*

3. **Consuming future upstream commits (load-bearing — the whole value proposition is continuity).** Decide the **mechanism and cadence** now: maintain a **watch list** on upstream `AntelopeIO/spring` and on the 6 vendored submodule upstreams; **cherry-pick** security/correctness fixes onto `release/1.2` on a stated review cadence (e.g., a triage pass per upstream push, or monthly if upstream stays frozen). Default posture is **fork-and-curate**, not fork-and-forget. Record the policy in CONTRIBUTING.md.

4. **Submodule ownership.** Fork all 6 unmaintained AntelopeIO submodules into eosrio with pinned SHAs, **prioritizing `bls12-381` + `bn256`** (Savanna finality crypto). Leave well-maintained third-party deps alone. A fork soliciting production trust cannot root its finality security on dead upstreams.

5. **Security-disclosure process (not just the file).** Publish SECURITY.md with eosrio's contact, **and decide the process**: embargo handling, CVE assignment ownership, and — critically — **how a vuln found in a vendored, now-unmaintained AntelopeIO crypto submodule (`bls12-381`/`bn256`) gets coordinated and patched** when there is no live upstream to coordinate with (eosrio becomes the de-facto upstream for those). Route reviews via CODEOWNERS.

6. **Signing / attestation trust root.** Decide before the first signed release: **who holds keys, key-rotation policy, Sigstore/cosign vs GPG, and where attestations are published.** This is a governance decision, not just an engineering task; reproducible builds (#1842) + cpack ordering (#1862) are prerequisites.

7. **Supported-toolchain matrix (makes pillar #2 actionable).** State explicitly which **LLVM/clang, Ubuntu, and boost** versions the 1.2.x line officially supports going forward, and a deprecation cadence. This scopes #578 (ORCv1→ORCv2), #1860 (vendor LLVM), #1861 (Ubuntu 24) and prevents perpetual toolchain drift. Recommendation: target current LTS (**Ubuntu 24.04 and now 26.04**, released ~2026-04) + modern LLVM (≥12, via vendored submodule) as the forward baseline; keep the existing baseline (20.04/22.04) supported through one transition release. Verified 2026-06: the reproducible `.deb` already installs + runs on 24.04 and 26.04 — so the toolchain-matrix work is about *native build + CI/packaging* on those releases, not runtime.

8. **Never activate 2.0 protocol features on the compat line.** Sync Calls and Events add new host functions, protocol-feature activations, ABI/serialization changes, and consensus-state/snapshot-format changes — every one a compat break. They never shipped, so there is no operator base to serve. Only spin up a separate, deliberately-versioned 2.0 fork under an explicit strategic commitment with a planned protocol-feature-activation + state-migration plan.

9. **Vaulta vs EOS branding (#1509 / #1517 / #1518).** Keep EOS-named CLI params and EOS-prefixed pubkeys on the 1.2.x line; **do NOT adopt the Vaulta rename** — renaming CLI options and the public-key prefix is a hard break for operator scripts, keys, and downstream tooling, the opposite of the zero-break mandate. Park the rename triad in §9 as a deliberate, separately-versioned UX break if ever pursued.

---

## 7. Out of scope / future 2.0-fork appendix

Listed so **nothing is silently dropped**. All excluded by the governing constraint (consensus/ABI/wire/RPC-shape break, protocol-feature activation, or 2.0-only feature line). Counts are exact.

### Sync Calls line — 2.0-only, never in production (16 items)
**#893** (EPIC), **#1622**, **#1838**, **#1254**, **#1394**, **#1461**, **#1839**, **#1853**, **#1623**, **#1624**, **#1544**, **#1545**, **#1735**, **#1847** (SIGSEGV in `execute_sync_call` — the code path does not exist on 1.2.x, so the crash is impossible there), **#1654** (read-only host-fn protocol feature), **#1850** (2.0-dev test flake). — *new host functions / consensus + ABI surface.*

### Events line — 2.0-only, consensus-affecting (33 items)
**#1694** (EPIC), **#1747**, **#1748** (protocol-feature activation), **#1749** (`emit_event` intrinsic), **#1750** (chainbase state + `event_root` + snapshot-format change), **#1751** (epoch-size hard-fork param), **#1758**, **#1759**, **#1760**, **#1761** (SHiP request/result shape addition), **#1762**, **#1763**, **#1764**, **#1765**, **#1766**, **#1767**, **#1768**, **#1769**, **#1770**, **#1771**, **#1772**, **#1774**, **#1775**, **#1795** (DP3 continuation / LIB proofs), **#1796**, **#1829**, **#1835**, **#1430** (remove zlib from SHiP logs — gated on the 2.0 breaking SHiP-log change), **#1438** (trace_api Events fields), **#1696**, **#1697**, **#1698**, **#1706**. — *protocol/consensus break + new SHiP/trace response fields for an unshipped feature.*

### #1872 — moved here on scope grounds (was wrongly P0 in the draft)
- **#1872** — `packed_transaction` zlib normalization leaving `packed_context_free_data` compressed. **Out of scope for 1.2.x:** the defective function `packed_transaction::normalize()` (decompress `packed_trx`, set `compression=none`, but leave CFD compressed) exists **only on `release/2.0` (`transaction.cpp:316`)**, is called only from 2.0's `chain_plugin.cpp:2127/2251` and `net_plugin.cpp:3273`, and depends on 2.0-only machinery (`validate_no_extra_data`/`extra_data`/`validate_no_compression`). On `release/1.2` there is **no `normalize()`**, no in-place mutation of the compression field, and `local_unpack_context_free_data()` decompresses CFD correctly off the unchanged compression value. The data members exist on 1.2, but the defective path does **not** — the bug is **structurally impossible** on the stable line. Per the governing constraint, parked here unless a concrete 1.2 trigger is found.

### Breaking RPC/ABI/consensus changes — rejected on 1.2.x
- **#1864 / #1858** — remove `count` from `get_table_by_scope` response → **breaking RPC-shape change** (confirmed in diff: drops the `count` member + FC_REFLECT). Do NOT backport. Tie to the fork's own deprecation/versioning policy (cluster with #890).
- **#1886** — separate transaction queue: **breaking** `get_unapplied_transactions_result` FC_REFLECT rename (`size`/`incoming_size`/`trxs` → `unapplied_size`/`queued_size`/`unapplied_trxs`/`queued_trxs`) **plus** an unreviewed main-thread scheduler refactor with no behavior wired up. Park.
- **#1070 / #1049** — slab allocator: real op win (state RAM ~3-5% ↓, snapshot load ~2× faster) BUT bumps on-disk state magic **CHAINB01→CHAINB02**, rejecting existing state files (forced replay/snapshot reload on upgrade) — a node-operator state-format break. Also CONFLICTING + entangled with main's fc-pack `db_header` rewrite. Park for a deliberately-gated state-migration release in a future fork.
- **#1729** (bill CPU for WASM instantiation), **#1732** (setcode pricing), **#1730** (raise `max_transaction_net_usage` / `max_inline_action_size`) — **consensus-affecting resource/pricing changes** requiring protocol-feature gating + network governance. Park for governance.
- **#1491** (check_time intrinsic + receipt-format change), **#1379-via-ABI** (table-scope ABI extension — compatible workaround tracked in Phase 2), **#1619** (batch trx notices per peer — p2p wire change + peer-compat negotiation).
- **#890 / #707** — 2.0 Deprecations & Removals EPIC; remove Prometheus (operators may still scrape the exporter → breaking removal). Gate behind the fork's deprecation policy.
- **Vaulta rename triad #1509 / #1517 / #1518** — deprecate/remove EOS-named params + EOS-prefixed pubkeys → breaks operator scripts/keys + downstream tooling. Strategic branding call (§6).
- **#1863** (Protobuf API v2 for `chain_api_plugin`), **#1823 / #1824** (Query API plugin — net-new `/v1/query/<contract>/<action>` surface; #1824 duplicates #1823; reuses `send_read_only_transaction`). Additive but unclear demand and pure DX, not stability — **park as product enhancement**; cheaper alt is documenting the existing `send_read_only_transaction` flow.

### Stale / close
- **#951** — cosmetic action-constructor refactor across ~70 files; the only value (`const action_base`) was removed; CONFLICTING, high rebase cost, zero stability value. **Close**; redo as a fresh focused change if const hardening is ever wanted.
- **#610** — Savanna finality-violation tests; targets ancient `release/1.0`, unresolved arhag CHANGES_REQUESTED, **cannot compile** (missing `finality_violation` contract + `process_finalizer_votes` API). **Close/park** unless eosrio commits to the slashing-proof epic (pairs with #91).
- **#775** — Smart LIB catchup sync via Savanna finality proofs; new sync protocol = wire/peer-compat change. **Research/future**; revisit after p2p hardening.

> **Exact accounting:** Sync Calls = **16** items · Events = **33** items (incl. the previously-dropped **#1795**). Every open PR (21) and every P0/P1 issue from triage appears above — in a phase, gated, referenced via its fixing PR (#1878→#1879, #1880→#1881), or parked.

---

## 8. Risks & unknowns

- **CI capacity & cost.** The heavy C++ build + integration/perf suites are sized for ENF-class self-hosted hardware. Provisioning matching runners has real cost; GitHub-hosted sizing for the heavy jobs is a genuine time/cost risk. **Until CI is green, no fix in Phases 1–3 is validated** — this is the single largest schedule risk.
- **Finality-crypto maintenance burden.** Vendoring `bls12-381` + `bn256` means **eosrio becomes the de-facto upstream** for consensus-critical pairing/BLS crypto with no original maintainer to fall back on. A latent bug or a needed security patch there is on eosrio alone, and a wrong change is a finality/consensus risk. The §6.5 disclosure-and-patching process must explicitly cover this.
- **Divergence from upstream.** Upstream is ~9 months frozen; if it revives, eosrio must reconcile its `release/1.2` backports with any new upstream work (the §6.3 cherry-pick policy mitigates but does not eliminate merge cost). If upstream stays dead, eosrio owns the entire 1.2.x line indefinitely.
- **Consensus-critical gated items.** #1857 (GMP→BoringSSL `modexp`), #455 (fork-db bounding), #543/#779 (finality voting/fsi) all touch Savanna consensus/finality. Each carries a **hard verify-first / prove-bit-equivalence gate**; shipping one wrong forks the chain — the exact outcome the fork exists to prevent.
- **Bus factor.** A small/single-maintainer fork carrying a production consensus binary needs CODEOWNERS-routed review for finality/crypto/p2p and a documented disclosure process, or the bus factor itself becomes the security risk. Governance files (§0.6) are not optional paperwork — they are part of the production-trust surface.
