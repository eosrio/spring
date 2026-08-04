# WAX consensus module

This experimental compile-time module provides the historical WAX
`env.verify_rsa_sha256_sig` WASM intrinsic without patching Spring's chain
interface, runtime registration source, OC intrinsic table, or genesis list.

Enable it at configure time:

```sh
cmake -S . -B build \
  -DSPRING_CONSENSUS_MODULES=consensus_modules/wax
```

The implementation intentionally preserves the current WAX fork's observable
behavior for compatibility testing. Before production use, its unbounded
multi-precision exponentiation needs a resource-exhaustion review. Changing
limits or failure behavior for an already-active intrinsic can itself be a
consensus change.
