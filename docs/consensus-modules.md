# Experimental consensus modules

Spring's appbase plugins extend node operation: APIs, networking, metrics, and
event consumers. They are intentionally outside transaction execution. Chain
compatibility code is different: every validating node must execute the same
host function with the same inputs, outputs, failures, and resource behavior.

This experiment adds a compile-time consensus-module layer below appbase. It is
not a dynamically loadable nodeos plugin.

## Prototype contract

A module owns its implementation and declares its build metadata in its own
`CMakeLists.txt`:

```cmake
spring_add_consensus_module(
   NAME example
   VERSION 1
   SOURCES example.cpp
   INTRINSICS example_intrinsic
)
```

The module exports one idempotent registration entry point derived from its
name:

```cpp
extern "C" void spring_register_consensus_module_example();
```

Spring generates and compiles:

- an explicit registry that references every selected module, preventing
  module-only objects from being discarded by static-library linking;
- additions to the genesis intrinsic whitelist;
- additions to the EOS-VM-OC intrinsic name table;
- a manifest containing module versions, intrinsic names, and source hashes;
- a manifest hash mixed into the OC cache identifier, so changing modules
  invalidates locally compiled code instead of reusing incompatible ordinals.

`webassembly::interface::invoke_consensus_intrinsic` adapts a module-owned free
function that receives `apply_context&` to the member-function ABI expected by
EOS-VM and EOS-VM-OC. The public `host_function_registrator` registers that
adapter with both runtimes.

Configure a build with one or more module source directories:

```sh
cmake -S . -B build \
  -DSPRING_CONSENSUS_MODULES="consensus_modules/wax;../another-module"
```

No modules are enabled by default.

## Activation and compatibility

Compiling an implementation into the binary and enabling it in consensus state
are separate concerns. WASM validation already checks the on-chain intrinsic
whitelist. Existing snapshots carry that whitelist. A replay from genesis uses
the generated genesis list.

The prototype therefore supports WAX's historical genesis intrinsic, but a
production API also needs an explicit activation policy:

- legacy chains can declare an intrinsic as active from genesis;
- new intrinsics should normally be activated by a protocol feature;
- builds should declare which chain profiles they support and refuse an
  unsupported profile before replay or block validation;
- node version output and build metadata should expose the module manifest hash.

## Production requirements

Before treating this as a stable community interface:

1. Define a versioned module ABI and compatibility policy across Spring releases.
2. Give module protocol features a stable digest and deterministic activation
   callback instead of requiring edits to `controller.cpp`.
3. Validate duplicate names and collisions with core intrinsics at configure
   time and again at process startup.
4. Make supported chain IDs/profiles and required module hashes machine-readable.
5. Add cross-runtime conformance tests for EOS-VM, EOS-VM-JIT, and EOS-VM-OC.
6. Add replay, snapshot restore, fork-switch, and mixed-binary rejection tests.
7. Define deterministic coding constraints: no locale, wall clock, network,
   nondeterministic threading, unchecked platform crypto, or unbounded work.
8. Establish review ownership and release signing for modules, because module
   source has the same trust level as `libraries/chain`.

The WAX RSA module is a useful first case because it is narrow but exercises all
of the important surfaces: WASM ABI registration, genesis state, optimized
runtime mapping, compatibility tests, and resource accounting.
