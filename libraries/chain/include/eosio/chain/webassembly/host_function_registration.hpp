#pragma once

#include <eosio/chain/webassembly/common.hpp>
#include <eosio/chain/webassembly/interface.hpp>

#ifdef EOSIO_EOS_VM_OC_RUNTIME_ENABLED
#include <eosio/chain/webassembly/eos-vm-oc.hpp>
#endif

#include <boost/hana/equal.hpp>
#include <boost/hana/string.hpp>

#include <tuple>

namespace eosio::chain::webassembly {

/**
 * Registers a host function with every enabled WASM runtime.
 *
 * This type is public so compile-time consensus modules can register host
 * functions without adding declarations or registration statements to the
 * chain implementation. The function must be a member of webassembly::interface;
 * invoke_consensus_intrinsic adapts module-owned free functions to that ABI.
 */
template<auto HostFunction, typename... Preconditions>
struct host_function_registrator {
   template<typename Mod, typename Name>
   constexpr host_function_registrator(Mod mod_name, Name fn_name) {
      eos_vm_host_functions_t::add<HostFunction, Preconditions...>(mod_name.c_str(), fn_name.c_str());
#ifdef EOSIO_EOS_VM_OC_RUNTIME_ENABLED
      constexpr bool is_injected = (Mod() == BOOST_HANA_STRING(EOSIO_INJECTED_MODULE_NAME));
      eosvmoc::register_eosvm_oc<HostFunction, is_injected, std::tuple<Preconditions...>>(
         mod_name + BOOST_HANA_STRING(".") + fn_name);
#endif
   }
};

} // namespace eosio::chain::webassembly
