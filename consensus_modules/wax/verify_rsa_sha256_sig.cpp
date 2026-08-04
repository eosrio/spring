#include <eosio/chain/apply_context.hpp>
#include <eosio/chain/transaction_context.hpp>
#include <eosio/chain/webassembly/host_function_registration.hpp>
#include <eosio/chain/webassembly/preconditions.hpp>

#include <fc/crypto/sha256.hpp>
#include <fc/crypto/hex.hpp>

#include <boost/hana/string.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <cstdint>
#include <exception>
#include <string>

namespace eosio::chain::consensus_modules::wax {

namespace {

int32_t verify_rsa_sha256_sig(apply_context& context,
                              legacy_span<const char> message,
                              legacy_span<const char> signature,
                              legacy_span<const char> exponent,
                              legacy_span<const char> modulus) {
   using boost::multiprecision::cpp_int;
   using boost::multiprecision::powm;
   using namespace std::string_literals;

   const auto error_prefix = "[ERROR] verify_rsa_sha256_sig: "s;

   try {
      const size_t message_len = message.size_bytes();
      const size_t signature_len = signature.size_bytes();
      const size_t exponent_len = exponent.size_bytes();
      const size_t modulus_len = modulus.size_bytes();

      if(message_len && signature_len && exponent_len && modulus_len == signature_len && modulus_len % 2 == 0) {
         const fc::sha256 message_sha256 =
            context.trx_context.hash_with_checktime<fc::sha256>(message.data(), message.size());

         auto pkcs1_encoding =
            "3031300d060960864801650304020105000420"s +
            fc::to_hex(message_sha256.data(), message_sha256.data_size());

         const auto encoded_message_len = modulus_len / 2;
         const auto digest_info_len = pkcs1_encoding.size() / 2;

         if(encoded_message_len >= digest_info_len + 11) {
            pkcs1_encoding = "0001"s +
                             std::string(2 * (encoded_message_len - digest_info_len - 3), 'f') +
                             "00"s + pkcs1_encoding;

            const cpp_int signature_int{"0x"s + std::string{signature.data(), signature_len}};
            const cpp_int exponent_int{"0x"s + std::string{exponent.data(), exponent_len}};
            const cpp_int modulus_int{"0x"s + std::string{modulus.data(), modulus_len}};
            return cpp_int{"0x"s + pkcs1_encoding} == powm(signature_int, exponent_int, modulus_int);
         }

         context.console_append(error_prefix + "Intended encoding message lenght too short\n");
      } else {
         context.console_append(error_prefix + "At least 1 param has an invalid length\n");
      }
   } catch(const std::exception& e) {
      context.console_append(error_prefix + e.what() + "\n");
   } catch(...) {
      context.console_append(error_prefix + "Unknown exception\n");
   }

   return false;
}

using legacy_char_span = legacy_span<const char>;
inline constexpr auto host_function =
   &webassembly::interface::template invoke_consensus_intrinsic<
      &verify_rsa_sha256_sig,
      legacy_char_span,
      legacy_char_span,
      legacy_char_span,
      legacy_char_span>;

} // namespace

} // namespace eosio::chain::consensus_modules::wax

extern "C" void spring_register_consensus_module_wax() {
   using namespace eosio::chain::consensus_modules::wax;
   static eosio::chain::webassembly::host_function_registrator<
      host_function,
      eosio::chain::webassembly::legacy_static_check_wl_args>
      register_host_function{BOOST_HANA_STRING("env"), BOOST_HANA_STRING("verify_rsa_sha256_sig")};
   static_cast<void>(register_host_function);
}
