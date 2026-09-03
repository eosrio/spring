#include <eosio/chain/qc.hpp>
#include <eosio/chain/types.hpp>
#include <eosio/chain/block_header.hpp>

#include <fc/exception/exception.hpp>
#include <fc/crypto/bls_private_key.hpp>
#include <fc/crypto/bls_utils.hpp>

#include <boost/test/unit_test.hpp>

// -----------------------------------------------------------------------------
//            Allow boost to print `aggregating_qc_sig_t::state_t`
// -----------------------------------------------------------------------------
namespace std {
   using state_t = eosio::chain::aggregating_qc_sig_t::state_t;
   std::ostream& operator<<(std::ostream& os, state_t s)
   {
      switch(s) {
      case state_t::unrestricted:   os << "unrestricted"; break;
      case state_t::restricted:     os << "restricted"; break;
      case state_t::weak_achieved:  os << "weak_achieved"; break;
      case state_t::weak_final:     os << "weak_final"; break;
      case state_t::strong:         os << "strong"; break;
      }
      return os;
   }
}

BOOST_AUTO_TEST_SUITE(finality_misc_tests)

BOOST_AUTO_TEST_CASE(qc_state_transitions) try {
   using namespace eosio::chain;
   using namespace fc::crypto::blslib;
   using state_t = aggregating_qc_sig_t::state_t;

   digest_type d(fc::sha256("0000000000000000000000000000001"));
   std::vector<uint8_t> digest(d.data(), d.data() + d.data_size());

   std::vector<bls_private_key> sk {
      bls_private_key("PVT_BLS_0d8dsux83r42Qg8CHgAqIuSsn9AV-QdCzx3tPj0K8yOJA_qb"),
      bls_private_key("PVT_BLS_Wfs3KzfTI2P5F85PnoHXLnmYgSbp-XpebIdS6BUCHXOKmKXK"),
      bls_private_key("PVT_BLS_74crPc__6BlpoQGvWjkHmUdzcDKh8QaiN_GtU4SD0QAi4BHY"),
      bls_private_key("PVT_BLS_foNjZTu0k6qM5ftIrqC5G_sim1Rg7wq3cRUaJGvNtm2rM89K"),
      bls_private_key("PVT_BLS_FWK1sk_DJnoxNvUNhwvJAYJFcQAFtt_mCtdQCUPQ4jN1K7eT"),
      bls_private_key("PVT_BLS_tNAkC5MnI-fjHWSX7la1CPC2GIYgzW5TBfuKFPagmwVVsOeW")
   };

   std::vector<bls_public_key> pubkey;
   pubkey.reserve(sk.size());
   for (const auto& k : sk)
      pubkey.push_back(k.get_public_key());

   auto weak_vote = [&](aggregating_qc_sig_t& qc, const std::vector<uint8_t>& digest_to_sign, size_t index, uint64_t weight) {
      return qc.add_vote(0, 0, false, index, sk[index].sign(digest_to_sign), weight);
   };

   auto strong_vote = [&](aggregating_qc_sig_t& qc, const std::vector<uint8_t>& digest_to_sign, size_t index, uint64_t weight) {
      return qc.add_vote(0, 0, true, index, sk[index].sign(digest_to_sign), weight);
   };

   constexpr uint64_t weight = 1;

   {
      constexpr uint64_t quorum = 1;
      constexpr uint64_t max_weak_sum_before_weak_final = 1;
      aggregating_qc_sig_t qc(2, quorum, max_weak_sum_before_weak_final); // 2 finalizers
      BOOST_CHECK_EQUAL(qc.state(), state_t::unrestricted);

      // add one weak vote
      // -----------------
      weak_vote(qc, digest, 0, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::weak_achieved);
      BOOST_CHECK(qc.is_quorum_met());

      // add duplicate weak vote
      // -----------------------
      auto ok = weak_vote(qc, digest, 0, weight);
      BOOST_CHECK(ok != vote_result_t::success); // vote was a duplicate
      BOOST_CHECK_EQUAL(qc.state(), state_t::weak_achieved);
      BOOST_CHECK(qc.is_quorum_met());

      // add another weak vote
      // ---------------------
      weak_vote(qc, digest, 1, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::weak_final);
   }

   {
      constexpr uint64_t quorum = 1;
      constexpr uint64_t max_weak_sum_before_weak_final = 1;
      aggregating_qc_sig_t qc(2, quorum, max_weak_sum_before_weak_final); // 2 finalizers
      BOOST_CHECK_EQUAL(qc.state(), state_t::unrestricted);

      // add a weak vote
      // ---------------
      weak_vote(qc, digest, 0, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::weak_achieved);
      BOOST_CHECK(qc.is_quorum_met());

      // add a strong vote
      // -----------------
      strong_vote(qc, digest, 1, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::strong);
      BOOST_CHECK(qc.is_quorum_met());
   }

   {
      constexpr uint64_t quorum = 1;
      constexpr uint64_t max_weak_sum_before_weak_final = 1;
      aggregating_qc_sig_t qc(2, quorum, max_weak_sum_before_weak_final); // 2 finalizers, weight_sum_minus_quorum = 1
      BOOST_CHECK_EQUAL(qc.state(), state_t::unrestricted);

      // add a strong vote
      // -----------------
      strong_vote(qc, digest, 1, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::strong);
      BOOST_CHECK(qc.is_quorum_met());

      // add a strong vote
      // -----------------
      strong_vote(qc, digest, 1, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::strong);
      BOOST_CHECK(qc.is_quorum_met());
   }

   {
      constexpr uint64_t quorum = 2;
      constexpr uint64_t max_weak_sum_before_weak_final = 1;
      aggregating_qc_sig_t qc(3, quorum, max_weak_sum_before_weak_final); // 3 finalizers

      // add a weak vote
      // ---------------
      weak_vote(qc, digest, 0, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::unrestricted);
      BOOST_CHECK(!qc.is_quorum_met());

      // add a strong vote
      // -----------------
      strong_vote(qc, digest, 1, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::weak_achieved);
      BOOST_CHECK(qc.is_quorum_met());

      {
         aggregating_qc_sig_t qc2(std::move(qc));

         // add a weak vote
         // ---------------
         weak_vote(qc2, digest, 2, weight);
         BOOST_CHECK_EQUAL(qc2.state(), state_t::weak_final);
         BOOST_CHECK(qc2.is_quorum_met());
      }
   }

   {
      constexpr uint64_t quorum = 2;
      constexpr uint64_t max_weak_sum_before_weak_final = 1;
      aggregating_qc_sig_t qc(3, quorum, max_weak_sum_before_weak_final); // 3 finalizers, quorum = 2

      // add a weak vote
      // ---------------
      weak_vote(qc, digest, 0, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::unrestricted);
      BOOST_CHECK(!qc.is_quorum_met());

      // add a strong vote
      // -----------------
      strong_vote(qc, digest, 1, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::weak_achieved);
      BOOST_CHECK(qc.is_quorum_met());

      {
         aggregating_qc_sig_t qc2(std::move(qc));

         // add a strong vote
         // -----------------
         strong_vote(qc2, digest, 2, weight);
         BOOST_CHECK_EQUAL(qc2.state(), state_t::strong);
         BOOST_CHECK(qc2.is_quorum_met());
      }
   }

   {
      constexpr uint64_t quorum = 2;
      constexpr uint64_t max_weak_sum_before_weak_final = 1;
      aggregating_qc_sig_t qc(3, quorum, max_weak_sum_before_weak_final); // 3 finalizers, quorum = 2

      // add a weak vote
      // ---------------
      weak_vote(qc, digest, 0, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::unrestricted);
      BOOST_CHECK(!qc.is_quorum_met());

      // add a weak vote
      // ---------------
      weak_vote(qc, digest, 1, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::weak_final);
      BOOST_CHECK(qc.is_quorum_met());

      {
         aggregating_qc_sig_t qc2(std::move(qc));

         // add a weak vote
         // ---------------
         weak_vote(qc2, digest, 2, weight);
         BOOST_CHECK_EQUAL(qc2.state(), state_t::weak_final);
         BOOST_CHECK(qc2.is_quorum_met());
      }
   }

   {
      constexpr uint64_t quorum = 2;
      constexpr uint64_t max_weak_sum_before_weak_final = 1;
      aggregating_qc_sig_t qc(3, quorum, max_weak_sum_before_weak_final); // 3 finalizers, quorum = 2

      // add a weak vote
      // ---------------
      weak_vote(qc, digest, 0, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::unrestricted);
      BOOST_CHECK(!qc.is_quorum_met());

      // add a weak vote
      // ---------------
      weak_vote(qc, digest, 1, weight);
      BOOST_CHECK_EQUAL(qc.state(), state_t::weak_final);
      BOOST_CHECK(qc.is_quorum_met());

      {
         aggregating_qc_sig_t qc2(std::move(qc));

         // add a strong vote
         // -----------------
         strong_vote(qc2, digest, 2, weight);
         BOOST_CHECK_EQUAL(qc2.state(), state_t::weak_final);
         BOOST_CHECK(qc2.is_quorum_met());
      }
   }

} FC_LOG_AND_RETHROW();

// Issue 5: weak_final boundary off by one in QC vote aggregation
BOOST_AUTO_TEST_CASE(qc_state_weak_final_boundary_off_by_one) try {
   using namespace eosio::chain;
   using namespace fc::crypto::blslib;
   using state_t = aggregating_qc_sig_t::state_t;

   digest_type d(fc::sha256("0000000000000000000000000000001"));
   std::vector<uint8_t> digest(d.data(), d.data() + d.data_size());

   std::vector<bls_private_key> sk {
      bls_private_key("PVT_BLS_0d8dsux83r42Qg8CHgAqIuSsn9AV-QdCzx3tPj0K8yOJA_qb"),
      bls_private_key("PVT_BLS_Wfs3KzfTI2P5F85PnoHXLnmYgSbp-XpebIdS6BUCHXOKmKXK"),
      bls_private_key("PVT_BLS_74crPc__6BlpoQGvWjkHmUdzcDKh8QaiN_GtU4SD0QAi4BHY"),
      bls_private_key("PVT_BLS_foNjZTu0k6qM5ftIrqC5G_sim1Rg7wq3cRUaJGvNtm2rM89K")
   };

   auto weak_vote = [&](aggregating_qc_sig_t& qc, const std::vector<uint8_t>& digest_to_sign, size_t index, uint64_t weight) {
      return qc.add_vote(0, 0, false, index, sk[index].sign(digest_to_sign), weight);
   };

   auto strong_vote = [&](aggregating_qc_sig_t& qc, const std::vector<uint8_t>& digest_to_sign, size_t index, uint64_t weight) {
      return qc.add_vote(0, 0, true, index, sk[index].sign(digest_to_sign), weight);
   };

   // Total weight = 5 (F0: 2, F1: 1, F2: 1, F3: 1), Quorum = 3
   // max_weak_sum_before_weak_final = 5 - 3 = 2.
   constexpr uint64_t quorum = 3;
   constexpr uint64_t max_weak_sum_before_weak_final = 2;

   aggregating_qc_sig_t qc(4, quorum, max_weak_sum_before_weak_final);
   BOOST_CHECK_EQUAL(qc.state(), state_t::unrestricted);

   // 1. F0 votes strong with weight 2: strong_sum = 2, weak_sum = 0 (< quorum 3) -> unrestricted
   strong_vote(qc, digest, 0, 2);
   BOOST_CHECK_EQUAL(qc.state(), state_t::unrestricted);
   BOOST_CHECK(!qc.is_quorum_met());

   // 2. F1 votes weak with weight 1: strong_sum = 2, weak_sum = 1 (sum = 3 >= quorum) -> weak_achieved
   weak_vote(qc, digest, 1, 1);
   BOOST_CHECK_EQUAL(qc.state(), state_t::weak_achieved);
   BOOST_CHECK(qc.is_quorum_met());

   // 3. F2 votes weak with weight 1: weak_sum becomes 2 == max_weak_sum_before_weak_final.
   // Pre-fix: used '>=' and prematurely transitioned to weak_final.
   // Post-fix: uses '>' so 2 > 2 is false, state remains weak_achieved!
   weak_vote(qc, digest, 2, 1);
   BOOST_CHECK_EQUAL(qc.state(), state_t::weak_achieved);
   BOOST_CHECK(qc.is_quorum_met());

   // 4. F3 votes strong with weight 1: strong_sum becomes 2 + 1 = 3 >= quorum.
   // Because state remained weak_achieved, this transitions to state_t::strong!
   strong_vote(qc, digest, 3, 1);
   BOOST_CHECK_EQUAL(qc.state(), state_t::strong);
   BOOST_CHECK(qc.is_quorum_met());

} FC_LOG_AND_RETHROW();

// Issue 9: max_weak_sum_before_weak_final() accumulates in int
BOOST_AUTO_TEST_CASE(finalizer_policy_large_weight_accumulate) {
   using namespace eosio::chain;

   finalizer_policy fp;
   fp.threshold = 4'000'000'000ULL;
   fp.finalizers = {
      finalizer_authority{ "fin1", 3'000'000'000ULL, bls_public_key{} },
      finalizer_authority{ "fin2", 3'000'000'000ULL, bls_public_key{} }
   };
   // Total weight = 6'000'000'000ULL (> 2^32, well over INT32_MAX).
   // Expected sum - threshold = 6'000'000'000 - 4'000'000'000 = 2'000'000'000ULL.
   BOOST_CHECK_EQUAL(fp.max_weak_sum_before_weak_final(), 2'000'000'000ULL);

   finalizer_policy fp_huge;
   fp_huge.threshold = 10'000'000'000ULL;
   fp_huge.finalizers = {
      finalizer_authority{ "fin1", 10'000'000'000ULL, bls_public_key{} },
      finalizer_authority{ "fin2", 20'000'000'000ULL, bls_public_key{} }
   };
   // Total weight = 30'000'000'000ULL.
   // Expected sum - threshold = 30'000'000'000 - 10'000'000'000 = 20'000'000'000ULL.
   BOOST_CHECK_EQUAL(fp_huge.max_weak_sum_before_weak_final(), 20'000'000'000ULL);
}

BOOST_AUTO_TEST_SUITE_END()
