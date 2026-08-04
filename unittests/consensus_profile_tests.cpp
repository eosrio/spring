#include <boost/test/unit_test.hpp>

#include <eosio/chain/consensus_module_manifest.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

using namespace eosio::chain;

BOOST_AUTO_TEST_SUITE(consensus_profile_tests)

BOOST_AUTO_TEST_CASE(metadata_is_well_formed) {
   BOOST_REQUIRE(!consensus_profile_name.empty());
   BOOST_REQUIRE(!consensus_profile_version.empty());
   const std::string expected_profile =
      std::string{consensus_profile_name} + "@" + std::string{consensus_profile_version};
   BOOST_REQUIRE_EQUAL(consensus_profile, expected_profile);
   BOOST_REQUIRE_EQUAL(
      consensus_profile_manifest.substr(0, consensus_profile_name.size()), consensus_profile_name);
   BOOST_REQUIRE_EQUAL(consensus_module_manifest_hash.size(), 64u);
   BOOST_REQUIRE(std::all_of(
      consensus_module_manifest_hash.begin(), consensus_module_manifest_hash.end(),
      [](unsigned char value) { return std::isxdigit(value); }));
}

BOOST_AUTO_TEST_CASE(profile_owns_module_selection) {
   if(consensus_profile_name == "vanilla") {
      BOOST_REQUIRE(consensus_module_manifest.empty());
   } else {
      BOOST_REQUIRE(!consensus_module_manifest.empty());
   }

   if(consensus_profile_name == "wax") {
      BOOST_REQUIRE(consensus_module_manifest.find("wax@") != std::string_view::npos);
      BOOST_REQUIRE(
         consensus_module_manifest.find("verify_rsa_sha256_sig") != std::string_view::npos);
   }
}

BOOST_AUTO_TEST_SUITE_END()
