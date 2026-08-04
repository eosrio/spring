#include <boost/test/unit_test.hpp>

#include <eosio/chain/consensus_module_manifest.hpp>
#include <eosio/chain/types.hpp>
#include <eosio/testing/tester.hpp>

#include <boost/multiprecision/cpp_int.hpp>

#include <fc/crypto/hex.hpp>
#include <fc/crypto/sha256.hpp>

#include <cstdint>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace eosio;
using namespace eosio::chain;

namespace {

constexpr bool wax_module_enabled = consensus_module_manifest.find("wax@") != std::string_view::npos;

// These keys and test cases come from the WAX fork's wax/signature_tests.cpp.
constexpr std::string_view public_exponent_1024 = "3";

constexpr std::string_view private_exponent_1024 =
   "00954e45c37dca926fcbde438a9f8df39643133e4877151a2d423da2a87c54a681cc3c27b6970e7289c67191f596ea5caed709d762b4c937d95da9279bb32292a344c57631a3adfa8d1a837699908861dfe108d9bd37184b3f8b3810e13f183382c497b424b4530b56bdfb6c97e0c74b9dec7558425221e2e4ba3a2737aa3e2f53";

constexpr std::string_view modulus_1024 =
   "dff568a53cafdba7b1cd654fef54ed61649cdd6cb29fa743e35c73fcba7ef9c2b25a3b91e295abcea9aa5af0625f8b06428ec3140f2dd3c60c7dbb698cb3dbf6c64b1160daec4eb7d6deca1dfc45b83d5f30e5398f6f737ee394d57c8d2bf412f056c2e8a54d9bf554149c0da31346e31f23ffb516b1f9797d650169199b7add";

// Production vector from WAX Testnet block 419439156, transaction
// f541307e62ee07e54879ba9fd31b5032e594edb5e0af12ad2d1553f0a163c174.
// The message is orng.wax's make_msg(seed, dapp, nonce) result for request 3641.
constexpr std::string_view orng_message_hex =
   "89130c5c32819b35f1710fbc0b3362fe153b9c6ad90aaf7bae1c02cb32d71c03";
constexpr std::string_view orng_exponent = "10001";
constexpr std::string_view orng_signature =
   "a3f08ea5d86e65b6db72eed2a69a3c2dab241a7a88fb392ee85327b3644c2e2ff0c4e75a03613790336f420f30e60609374270567f85bb17d119061b885ac153"
   "fc339c02fe148bf61d93503bb7bbdd44749658cbd39e5a4a6188678b5095c438be892676b3617d431787ced290d4985b118a4014a644d9fe89fe41bb6d8e4fed"
   "ef940105a8082fa7e2de211382cd0f685d34b85d19a92d5e361e4b1a8ad790b7be7c11a06377dd81746be7238b2afde139afc3c36aac26550af97b2f92bfb435"
   "0a9ab4bd69e5e9f52aef9d7b330d1c700d67d52e60dff617264451cc0b2ba2aee443db61e4dddcf0d19a2bd4a83c2cc2cbe64df46d826491315297bd246b9c5e"
   "1a3b9b07b50de54f37608722631f3e5693c218767201d02ef4e40e982d8cec898b4d6951f68cacb50b791c9a49aedd20e7f10b3701e14d4f885a0f9b6332e50c"
   "9b6451c314352664088e83d191210c2ac53989c1ed09f6b9f10b31d1a473042744e665ddef4f58b60703d991d3590a2ce93425a53c38d4d20bfd0dcbb60cebf8"
   "5a7a079db91ef31b52ee97bfef6f9364bd0ad0afe1152e60224160ab59e8f47146f73aca5675c733e79cb5fa37e0187f4393b1ab416245993a6dfed0f8d2ecc6"
   "0d15b4c858f375762b6bd099824ca2b2513f3a168784f3365e7155a5086e252fef04a7e6fce5a489e90e1746150fd0bb76926343e0defd094d87ecf6176c706c";
constexpr std::string_view orng_modulus =
   "ae8f5e266dccc5f8809e9c68a0271e7e2199db8159389aa8016dcb47495e5919d348d31dca6acc0238480d8a600286ba09028161bf519061a95e0f26ccadb4fa"
   "b8ecf4fbf8ac7c64b34729dcc217a280184981dd59c0040c47db82b232a4516afbdcb3cc360893e4a8ce03c21282eff12fbc9b34906975041bd984815b17d8df"
   "b0aef5170bf62f16ac2412423af95e557ed598164b65636ec15389be32b348851bdd8810e12c7ec9cc517ef515ddc22d5dceaf026402a6b632f24b392a6e0b04"
   "7675ca3d2a844deaf7bd8a2a1c7233e5d7d20582e1c3afe8744cbbd32b7db8121a44c4345d1e457446c1662984d018ca6c2b0112ab7cfe0356edcc8bb88014ec"
   "7cc3e747b90d7ffd44b3e9f8ae9214f62335dd09542f9eefbb6a0e2543f723844733ac1d38690947e4ca808b4e9612b569a1119d0e0aec409baacf2a6636cc1e"
   "6ae976c15d5f826eb8ce1d1dfa18e55c5bc9942cbc50c45412cd12bf1050707d3580413bdce902063a2fb3ca04500820b22a37c0237853d566efda5c24ffc9eb"
   "b8e5b1e9e54f42a92fd51785f35c8fee7dfadbe770d36c74b32806f32868b4abe85feeed0e8fcfc662b432e760e1bfc8991fd1d54afa8018bbeb5ceed2c22cc7"
   "65a9b1ba209178168d74aa7c214e6eeb79186a94828409731dff8af4a61a69ff0eee2a9044e375afa278858dff38bd55ff32badc3e6b944c6d91b3a8949e943d";

std::string hex_to_bytes(std::string_view hex) {
   if(hex.size() % 2)
      throw std::invalid_argument{"hex string must contain whole bytes"};
   std::string result(hex.size() / 2, '\0');
   const auto written = fc::from_hex(std::string{hex}, result.data(), result.size());
   if(written != result.size())
      throw std::invalid_argument{"invalid hex string"};
   return result;
}

class rsa_signer {
 public:
   rsa_signer(std::string_view private_exponent, std::string_view modulus)
      : private_exponent_{"0x" + require_nonempty(private_exponent)},
        modulus_{"0x" + require_modulus(modulus)},
        modulus_size_{modulus.size()} {}

   std::string sign(std::string_view message) const {
      using namespace boost::multiprecision;

      require_nonempty(message);
      const fc::sha256 message_hash = fc::sha256::hash(message.data(), message.size());
      const std::string encoded = pkcs1_encode(
         modulus_size_, bytes_to_hex(message_hash.data(), message_hash.data_size()));
      const cpp_int signature = powm(cpp_int{"0x" + encoded}, private_exponent_, modulus_);

      std::vector<char> bytes;
      export_bits(signature, std::back_inserter(bytes), 8);
      return bytes_to_hex(bytes.data(), bytes.size());
   }

 private:
   boost::multiprecision::cpp_int private_exponent_;
   boost::multiprecision::cpp_int modulus_;
   std::size_t modulus_size_;

   static std::string require_nonempty(std::string_view value) {
      if(value.empty())
         throw std::invalid_argument{"String cannot be empty"};
      return std::string{value};
   }

   static std::string require_modulus(std::string_view modulus) {
      const std::string result = require_nonempty(modulus);
      if(result.front() == '0')
         throw std::invalid_argument{"No leading zeroes allowed in modulus"};
      return result;
   }

   static std::string bytes_to_hex(const char* bytes, std::size_t size) {
      static constexpr char hex[] = "0123456789abcdef";
      std::string result;
      result.reserve(size * 2);
      for(std::size_t i = 0; i < size; ++i) {
         const auto byte = static_cast<unsigned char>(bytes[i]);
         result += hex[byte >> 4];
         result += hex[byte & 0x0f];
      }
      return result;
   }

   static std::string pkcs1_encode(std::size_t modulus_length, const std::string& message_hash) {
      constexpr std::string_view pkcs1_sha256 = "003031300d060960864801650304020105000420";
      std::string encoded = std::string{pkcs1_sha256} + message_hash;
      encoded = std::string(modulus_length - encoded.size() - 3, 'f') + encoded;
      encoded.front() = '1';
      return encoded;
   }
};

struct rsa_case {
   std::string message;
   std::string signature;
   std::string exponent;
   std::string modulus;
   bool expected;
};

std::string wat_bytes(std::string_view value) {
   std::ostringstream result;
   result << std::hex << std::setfill('0');
   for(const unsigned char byte : value)
      result << '\\' << std::setw(2) << static_cast<unsigned>(byte);
   return result.str();
}

std::string make_rsa_contract(const std::vector<rsa_case>& cases) {
   struct wasm_args {
      std::uint32_t message;
      std::uint32_t signature;
      std::uint32_t exponent;
      std::uint32_t modulus;
   };

   constexpr std::string_view failure_message = "WAX RSA compatibility case failed";
   std::ostringstream data;
   std::ostringstream checks;
   std::uint32_t next_offset = 64;

   const auto add_data = [&](const std::string& value) {
      const auto offset = next_offset;
      if(!value.empty()) {
         data << "(data (i32.const " << offset << ") \"" << wat_bytes(value) << "\")\n";
         next_offset += value.size();
      }
      return offset;
   };

   for(const auto& test : cases) {
      const wasm_args args{
         add_data(test.message), add_data(test.signature), add_data(test.exponent), add_data(test.modulus)};
      checks
         << "(call $eosio_assert\n"
         << "  (i32.eq\n"
         << "    (call $verify_rsa_sha256_sig "
         << "(i32.const " << args.message << ") (i32.const " << test.message.size() << ") "
         << "(i32.const " << args.signature << ") (i32.const " << test.signature.size() << ") "
         << "(i32.const " << args.exponent << ") (i32.const " << test.exponent.size() << ") "
         << "(i32.const " << args.modulus << ") (i32.const " << test.modulus.size() << "))\n"
         << "    (i32.const " << test.expected << "))\n"
         << "  (i32.const 0))\n";
   }

   std::ostringstream wat;
   wat
      << "(module\n"
      << "(import \"env\" \"verify_rsa_sha256_sig\"\n"
      << "  (func $verify_rsa_sha256_sig (param i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))\n"
      << "(import \"env\" \"eosio_assert\" (func $eosio_assert (param i32 i32)))\n"
      << "(table 0 anyfunc)\n"
      << "(memory 1)\n"
      << "(export \"memory\" (memory 0))\n"
      << "(export \"apply\" (func $apply))\n"
      << "(data (i32.const 0) \"" << wat_bytes(failure_message) << "\\00\")\n"
      << data.str()
      << "(func $apply (param i64 i64 i64)\n"
      << checks.str()
      << ")\n"
      << ")\n";
   return wat.str();
}

constexpr std::string_view rsa_test_abi = R"({
   "version": "eosio::abi/1.2",
   "types": [],
   "structs": [{"name": "run", "base": "", "fields": []}],
   "actions": [{"name": "run", "type": "run", "ricardian_contract": ""}],
   "tables": [],
   "ricardian_clauses": []
})";

struct wax_rsa_fixture : eosio::testing::validating_tester {
   static constexpr name account = "wax.rsa"_n;
   rsa_signer signer{private_exponent_1024, modulus_1024};

   wax_rsa_fixture() {
      create_accounts({account});
      set_abi(account, std::string{rsa_test_abi});
      produce_block();
   }

   transaction_trace_ptr run(const std::vector<rsa_case>& cases) {
      if constexpr(!wax_module_enabled) {
         BOOST_TEST_MESSAGE("WAX consensus module is not selected; compatibility vector is not applicable");
         return {};
      }
      const std::string contract = make_rsa_contract(cases);
      set_code(account, contract.c_str());
      produce_block();
      return push_action(account, "run"_n, account, fc::mutable_variant_object());
   }
};

} // namespace

BOOST_AUTO_TEST_SUITE(wax_rsa_tests)

BOOST_FIXTURE_TEST_CASE(check_invalid_lengths, wax_rsa_fixture) {
   run({
      {"", "abcd", "abcd", "abcd", false},
      {"abcd", "", "abcd", "abcd", false},
      {"abcd", "abcd", "", "abcd", false},
      {"abcd", "abcd", "abcd", "", false},
   });
}

BOOST_FIXTURE_TEST_CASE(check_invalid_hex_strings, wax_rsa_fixture) {
   run({
      {"foo", "XXXX", "abcd", "abcd", false},
      {"foo", "abcd", "XXXX", "abcd", false},
      {"foo", "abcd", "abcd", "XXXX", false},
   });
}

BOOST_FIXTURE_TEST_CASE(signing_happy_path, wax_rsa_fixture) {
   const std::string message = "message to sign";
   const std::string signature = signer.sign(message);
   BOOST_REQUIRE_EQUAL(signature.size(), modulus_1024.size());
   run({{message, signature, std::string{public_exponent_1024}, std::string{modulus_1024}, true}});
}

BOOST_FIXTURE_TEST_CASE(minimal_message_length, wax_rsa_fixture) {
   const std::string message = "1";
   run({{message, signer.sign(message), std::string{public_exponent_1024}, std::string{modulus_1024}, true}});
}

BOOST_FIXTURE_TEST_CASE(wax_testnet_orng_production_vector, wax_rsa_fixture) {
   run({{hex_to_bytes(orng_message_hex), std::string{orng_signature}, std::string{orng_exponent},
         std::string{orng_modulus}, true}});
}

BOOST_FIXTURE_TEST_CASE(malformed_parameter_length, wax_rsa_fixture) {
   run({{"message to sign", "A", "A", "B", false}});
}

BOOST_FIXTURE_TEST_CASE(corrupted_signature, wax_rsa_fixture) {
   const std::string message = "message to sign";
   std::string signature = signer.sign(message);
   BOOST_REQUIRE(!signature.empty());
   signature.back() = signature.back() == '0' ? '1' : '0';
   run({{message, signature, std::string{public_exponent_1024}, std::string{modulus_1024}, false}});
}

BOOST_AUTO_TEST_CASE(intrinsic_availability_matches_build) {
   eosio::testing::validating_tester chain;
   chain.create_accounts({wax_rsa_fixture::account});
   const std::string contract = make_rsa_contract({{"message", "aa", "3", "bb", false}});
   if constexpr(wax_module_enabled) {
      BOOST_REQUIRE_NO_THROW(chain.set_code(wax_rsa_fixture::account, contract.c_str()));
   } else {
      BOOST_CHECK_THROW(chain.set_code(wax_rsa_fixture::account, contract.c_str()), wasm_exception);
   }
}

BOOST_AUTO_TEST_SUITE_END()
