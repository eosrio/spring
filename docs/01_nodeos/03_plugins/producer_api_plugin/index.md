## Description

The `producer_api_plugin` exposes a number of endpoints for the [`producer_plugin`](../producer_plugin/index.md) to the RPC API interface managed by the [`http_plugin`](../http_plugin/index.md).

## Usage

```console
# config.ini
plugin = eosio::producer_api_plugin
```
```sh
# nodeos startup params
nodeos ... --plugin eosio::producer_api_plugin
```

## Options

These can be specified from both the command-line or the `config.ini` file:

```console
Config Options for eosio::producer_api_plugin:
  --http-expose-nonloopback-producer-api
                                        Allow producer_rw and snapshot HTTP
                                        APIs to bind to non-loopback addresses.
                                        These endpoints have no authentication
                                        and can pause/resume production, change
                                        runtime options, manage snapshots, and
                                        schedule protocol features. Default is
                                        false: non-loopback exposure is refused
                                        at startup. Loopback and UNIX socket
                                        bindings do not require this option.
```

Related `http_plugin` option:

```console
  --http-allow-control-plane-cors
                                        Allow Access-Control-Allow-Origin while
                                        producer_rw/snapshot are bound to a
                                        non-loopback address. Without this flag
                                        that combination is refused.
```

## Security

`producer_api_plugin` RPCs are **unauthenticated**. Destructive calls (`pause`,
`resume`, `update_runtime_options`, snapshot schedule, whitelist/greylist,
protocol feature schedule) share the `http_plugin` listener.

Safe defaults:

* Bind HTTP to `127.0.0.1` or a UNIX socket (the `http-server-address` default
  is loopback). Local `cleos` / operator tooling keeps working with no extra flags.
* Prefer `--http-category-address` so `producer_rw` and `snapshot` listen on
  loopback or a UNIX socket while public chain APIs use a different address.
* Non-loopback exposure requires `--http-expose-nonloopback-producer-api`.
* Combining a configured `access-control-allow-origin` with non-loopback
  producer/snapshot APIs requires `--http-allow-control-plane-cors`.
* `access-control-allow-origin=*` cannot be combined with
  `access-control-allow-credentials=true` (refused at startup).

## Dependencies

* [`producer_plugin`](../producer_plugin/index.md)
* [`chain_plugin`](../chain_plugin/index.md)
* [`http_plugin`](../http_plugin/index.md)

### Load Dependency Examples

```console
# config.ini
plugin = eosio::producer_plugin
[options]
plugin = eosio::chain_plugin
[options]
plugin = eosio::http_plugin
[options]
```
```sh
# command-line
nodeos ... --plugin eosio::producer_plugin [options]  \
           --plugin eosio::chain_plugin [operations] [options]  \
           --plugin eosio::http_plugin [options]
```
