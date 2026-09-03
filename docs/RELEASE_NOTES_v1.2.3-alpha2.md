# Spring v1.2.3-alpha2 — eosrio/spring maintenance line

⚠️ **Alpha — for testing only, not for production.** Second testing build of the community-maintained 1.2.x stable line (`eosrio/spring`), continuing `AntelopeIO/spring` after upstream stalled. Built on v1.2.2 + OC-on-LLVM-18 (`v1.2.3-alpha1`) — with a hard rule of zero compatibility breaks (no consensus/protocol-rule, ABI/serialization/wire, or RPC/SHiP response-shape changes). The v2.0 line is out of scope.

---

### Highlights: Comprehensive Community Security & Stability Remediation

This release incorporates an exhaustive audit, patch implementation, and multi-layered verification of **13 community vulnerability and bug reports** inherited across Spring-derived nodes (reported by Wire Network in comparative review against `origin/main`).

Key remediations include:
- **Consensus Divergence Prevention (BLS Intrinsics)**: Fixed 32-bit modulo length wrapping in BLS weighted-sum and pairing intrinsics (`crypto.cpp`) that previously allowed attacker-crafted transactions to trigger out-of-bounds reads, diverging across WASM runtimes (`eos-vm-oc` vs `eos-vm` / `eos-vm-jit`).
- **P2P Stream Desync Defense**: Introduced `fc::bounded_datastream` to strictly bound message parsers to declared message lengths, ensuring under-length or malicious P2P frames cannot consume bytes from subsequent pipelined messages or desynchronize network streams.
- **Savanna Finality Hardening**: Reordered pending QC vote bitset format and weight verification strictly before dual-finalizer vote indexing, added missing bitset bounds assertions, and corrected an off-by-one check in `weak_final` QC aggregation boundary calculations.
- **Anti-DoS Protections**:
  - Prevented targeted victim account throttling under subjective billing by ensuring failed-transaction penalties are only billed when declared authorizations were actually satisfied by valid signatures.
  - Bounded the SHiP WebSocket status-request queue to protect history nodes from memory exhaustion.
  - Corrected connection keepalive watchdog progress logic to only consider held blocks as progress, ensuring heartbeat recovery cannot be suppressed by peers announcing unheld blocks.
- **Memory Safety & Undefined Behavior**: Fixed SHA3 big-endian out-of-bounds writes and uninitialized indices, added bounds checks on `message_buffer::advance_read_ptr`, guarded null pointer arithmetic in `to_base58`, and eliminated `memcpy(dst, nullptr, 0)` in state history serialization.

---

### Categorized Changes

#### Consensus / chain runtime (Savanna)
- **BLS length checks wrap in 32-bit** (Issue 1): Hoist `n` into `size_t` prior to multiplication across `bls_g1_weighted_sum`, `bls_g2_weighted_sum`, and `bls_pairing`, eliminating integer truncation and cross-runtime execution divergence.
- **Pending QC vote bitset validated after indexing** (Issue 2): Reorder `qc_t::verify_basic` so `verify_vote_format` and `verify_weights` run before `verify_dual_finalizers_votes`; add missing `other_vote_index` bounds assertions in `vote_same_at`.
- **`weak_final` boundary off by one in QC aggregation** (Issue 5): Use strict `>` comparison under `state_t::weak_achieved` in `qc.cpp`, permitting strong QC formation at exact threshold weight.
- **Finalizer authority weight accumulation overflow** (Issue 9): Initialize `std::accumulate` with `uint64_t{0}` in `finalizer_policy.hpp` to prevent signed 32-bit integer overflow when finalizer weights exceed $2^{31}-1$.
- **Transaction authorization verification tracking** (Issue 7): Record `declared_auths_satisfied` on `transaction_metadata` upon successful `check_authorization()` in `controller.cpp`.

#### P2P / networking
- **P2P frame parsers not bounded to declared message length** (Issue 4): Introduce `fc::bounded_datastream`, route all net message unpacking through bounded streams, and add `connection::advance_to_frame_end()` to skip unconsumed declared bytes without desynchronizing the stream.
- **Missing block notices count as block progress** (Issue 3): Refresh `latest_blk_time` only when receiving notices for blocks already held in local state; default `p2p-disable-block-nack` to `true` when a block producer is configured.
- **Invalid peer retention in `supplied_peers` and non-numeric port rejection** (Issue 13): Validate endpoint syntax and enforce numeric port range (1..65535) via `net_utils::is_valid_port` and `split_host_port_type` prior to insertion into `supplied_peers` in `connections_manager::connect()`.

#### Plugins & State History (SHiP & Producer)
- **SHiP status-request queue unbounded** (Issue 6): Cap `queued_status_requests` in `session.hpp` to 100 entries with deterministic swap extraction to prevent memory exhaustion, and cleanly terminate sessions that exceed the limit with `status_request_queue_limit_exceeded`.
- **Failed-transaction blame uses unverified authorizer** (Issue 7): Gate subjective billing account failures (`_account_fails.add`) and failure CPU billing on `trx->satisfied_authorizations()`, preventing unauthenticated attackers from throttling arbitrary victim accounts.

#### Core utilities & correctness (`libfc`, `state_history`)
- **`sha3` big-endian OOB write and uninitialized index** (Issue 8): Initialize loop index `i = 0`, bound word conversion by `number_of_words = 25`, and fix `#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__` endianness macro checks in `sha3.cpp`.
- **`message_buffer::advance_read_ptr` bounds check** (Issue 10): Add bounds verification throwing `fc::out_of_range_exception` prior to adjusting read pointers, preventing 32-bit unsigned underflow and heap memory corruption.
- **`to_base58` null pointer safety** (Issue 11): Return an empty string for zero-length buffers, assert non-null data pointer on positive length, and guard `EncodeBase58` overloads against empty containers (`data() == nullptr`).
- **`history_pack_big_bytes(shared_blob)` null `memcpy`** (Issue 12): Add size guard `if (b.size()) ds.write(b.data(), b.size());` matching the `bytes` overload, eliminating undefined behavior under UBSan.

#### Licensing
- **Synchronize upstream MIT License**: Upstream `AntelopeIO/spring` transitioned the project from Business Source License 1.1 (BSL 1.1) to the MIT License in commit [`e6a99f68`](https://github.com/AntelopeIO/spring/commit/e6a99f68b67abc4d89fe716755b2e1394a4991f7) on November 12, 2025 (prior to the eosrio maintenance releases). Because upstream applied the change to `main` while `release/1.2` was frozen, the 1.2.x maintenance branch had inadvertently retained the older BSL 1.1 text. This release synchronizes `LICENSE` with upstream's official MIT License text.

#### Controller / stability
- Keep `onblock` REJECTING trace out of warn during sync/replay ([`0b9aca87c`](https://github.com/eosrio/spring/commit/0b9aca87c)).

---

### Credits
Special thanks to Wire Network ([`Wire-Network/wire-sysio`](https://github.com/Wire-Network/wire-sysio), credit Kevin Heifner) for auditing the shared upstream code, documenting reproducible runtime measurements, and publishing reference fixes, as well as the original community reporters and Spring maintainers.

---

### Verification & Validation Evidence
- **Build**: 153/153 targets built cleanly (`ninja -C build`) with zero errors or warnings.
- **Libfc Test Suite**: `test_fc` passed 159/159 test cases and 5,012,360 assertions with 0 failures (including `test_bounded_datastream`, `test_base58`, `test_message_buffer`, and `test_m1_adversarial`).
- **Net Plugin Test Suite**: `test_net_plugin` passed 20/20 test cases with 0 failures.
- **Consensus & Unit Tests**: `unit_test` passed 99/99 test cases across `bls_primitives_tests`, `block_state_tests`, `finality_misc_tests`, `subjective_billing_tests`, `state_history_tests`, `auth_tests`, and `m2_adversarial_tests`.
- **Multi-WASM Runtimes**: 20/20 test suites passed cleanly across `eos-vm-oc`, `eos-vm` (interpreter), and `eos-vm-jit`.
- **Cluster Integration**: Multi-node cluster test `subjective_billing_test` passed in 132.55s.

---

### Artifacts & Verification
- `antelope-spring_1.2.3-alpha2_amd64.deb` — Hermetic reproducible build for Ubuntu 20.04 / 22.04 / 24.04 / 26.04 with modern LLVM 18 OC.
- `antelope-spring-1.2.3-alpha2-x86_64.tar.zst` — Portable tarball.
- `SHA256SUMS.txt` — Cryptographic checksums.

**Verify binary version string:**
```bash
nodeos --full-version
# Expected: v1.2.3-alpha2-<hash>
```

---

### What to test / known limitations
- Primary ask for testnet node operators: Run `nodeos` with `--eos-vm-oc-enable=all` and observe peer block exchange stability and synchronization under notice mode.
- Validate that P2P peers with notice mode enabled (`p2p-disable-block-nack=false`) maintain reliable synchronization and that heartbeat watchdog recovery fires promptly if missing blocks are announced.
- For nodes operating with subjective billing enabled (`--disable-subjective-p2p-billing=false`), verify that spoofed transactions naming valid third-party accounts do not throttle legitimate victim transactions.

**Full Diff:** [v1.2.3-alpha1...release/1.2.3-alpha2](https://github.com/eosrio/spring/compare/v1.2.3-alpha1...release/1.2.3-alpha2)
