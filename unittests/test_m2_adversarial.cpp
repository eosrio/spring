#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/qc.hpp>
#include <eosio/chain/finalizer_policy.hpp>
#include <eosio/chain/transaction_metadata.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/webassembly/return_codes.hpp>
#include <fc/crypto/bls_private_key.hpp>
#include <test_contracts.hpp>

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::chain::webassembly;
using namespace eosio::testing;
using namespace fc::crypto::blslib;

BOOST_AUTO_TEST_SUITE(m2_adversarial_tests)

// =============================================================================
// 1. Issue 1: BLS Weighted-Sum & Pairing 32-Bit Length Wrap Adversarial Tests
// =============================================================================
BOOST_AUTO_TEST_CASE(bls_32bit_wrap_adversarial_stress) { try {
   tester c( setup_policy::preactivate_feature_and_new_bios );

   const auto& tester1_account = account_name("tester1");
   c.create_accounts( {tester1_account} );
   c.produce_block();

   const auto& pfm = c.control->get_protocol_feature_manager();
   const auto& d = pfm.get_builtin_digest( builtin_protocol_feature_t::bls_primitives );
   BOOST_REQUIRE( d );

   c.preactivate_protocol_features( {*d} );
   c.produce_block();

   c.set_code( tester1_account, test_contracts::bls_primitives_test_wasm() );
   c.set_abi( tester1_account, test_contracts::bls_primitives_test_abi().data() );
   c.produce_block();

   std::vector<char> zero_res_g1(96, 0);
   std::vector<char> zero_res_g2(192, 0);
   std::vector<char> zero_res_pairing(576, 0);

   std::vector<char> g1_buf_1elem(96, 0);
   std::vector<char> g2_buf_1elem(192, 0);
   std::vector<char> scalar_buf_1elem(32, 0);

   // Part A: Multiples of 0x08000000 that wrap n*96, n*192, and n*32 all to 0 mod 2^32
   // Tested with empty buffers
   const std::vector<uint32_t> wrap_all_to_zero = {
      0x08000000, // 134,217,728
      0x10000000, // 268,435,456
      0x18000000, // 402,653,184
      0x20000000  // 536,870,912
   };

   for (uint32_t n : wrap_all_to_zero) {
      c.push_action( tester1_account, "testg1wsum"_n, tester1_account, mutable_variant_object()
         ("points", std::vector<char>())
         ("scalars", std::vector<char>())
         ("num", n)
         ("res", zero_res_g1)
         ("expected_error", (int32_t)return_code::failure)
      );

      c.push_action( tester1_account, "testg2wsum"_n, tester1_account, mutable_variant_object()
         ("points", std::vector<char>())
         ("scalars", std::vector<char>())
         ("num", n)
         ("res", zero_res_g2)
         ("expected_error", (int32_t)return_code::failure)
      );

      c.push_action( tester1_account, "testpairing"_n, tester1_account, mutable_variant_object()
         ("g1_points", std::vector<char>())
         ("g2_points", std::vector<char>())
         ("num", n)
         ("res", zero_res_pairing)
         ("expected_error", (int32_t)return_code::failure)
      );
   }

   // Part B: Multiples of 0x08000000 + 1 that wrap n*96 -> 96, n*192 -> 192, n*32 -> 32 mod 2^32
   // In 32-bit math, these wrap to exactly 1-element buffer size (96, 192, 32 bytes).
   // Prior to the fix, the 32-bit guard accepted the 1-element buffer and iterated out-of-bounds!
   const std::vector<uint32_t> wrap_all_to_one = {
      0x08000001,
      0x10000001,
      0x18000001,
      0x20000001
   };

   for (uint32_t n : wrap_all_to_one) {
      c.push_action( tester1_account, "testg1wsum"_n, tester1_account, mutable_variant_object()
         ("points", g1_buf_1elem)
         ("scalars", scalar_buf_1elem)
         ("num", n)
         ("res", zero_res_g1)
         ("expected_error", (int32_t)return_code::failure)
      );

      c.push_action( tester1_account, "testg2wsum"_n, tester1_account, mutable_variant_object()
         ("points", g2_buf_1elem)
         ("scalars", scalar_buf_1elem)
         ("num", n)
         ("res", zero_res_g2)
         ("expected_error", (int32_t)return_code::failure)
      );

      c.push_action( tester1_account, "testpairing"_n, tester1_account, mutable_variant_object()
         ("g1_points", g1_buf_1elem)
         ("g2_points", g2_buf_1elem)
         ("num", n)
         ("res", zero_res_pairing)
         ("expected_error", (int32_t)return_code::failure)
      );
   }

   // Part C: Additional boundary and wrapping values of n
   // 1. n = 0: must reject immediately
   {
      c.push_action( tester1_account, "testg1wsum"_n, tester1_account, mutable_variant_object()
         ("points", std::vector<char>())
         ("scalars", std::vector<char>())
         ("num", 0)
         ("res", zero_res_g1)
         ("expected_error", (int32_t)return_code::failure)
      );

      c.push_action( tester1_account, "testg2wsum"_n, tester1_account, mutable_variant_object()
         ("points", std::vector<char>())
         ("scalars", std::vector<char>())
         ("num", 0)
         ("res", zero_res_g2)
         ("expected_error", (int32_t)return_code::failure)
      );

      c.push_action( tester1_account, "testpairing"_n, tester1_account, mutable_variant_object()
         ("g1_points", std::vector<char>())
         ("g2_points", std::vector<char>())
         ("num", 0)
         ("res", zero_res_pairing)
         ("expected_error", (int32_t)return_code::failure)
      );
   }

   // 2. n = 0x04000000 (2^26): wraps n*192 to 0, but n*32 = 2^31 (2 GB),
   // which exceeds WASM 33 MiB linear memory and is safely trapped by the VM.
   {
      const uint32_t n_wrap_g2 = 0x04000000;
      BOOST_CHECK_THROW(
         c.push_action( tester1_account, "testg2wsum"_n, tester1_account, mutable_variant_object()
            ("points", std::vector<char>())
            ("scalars", std::vector<char>())
            ("num", n_wrap_g2)
            ("res", zero_res_g2)
            ("expected_error", (int32_t)return_code::failure)
         ),
         wasm_execution_error
      );

      BOOST_CHECK_THROW(
         c.push_action( tester1_account, "testpairing"_n, tester1_account, mutable_variant_object()
            ("g1_points", std::vector<char>())
            ("g2_points", std::vector<char>())
            ("num", n_wrap_g2)
            ("res", zero_res_pairing)
            ("expected_error", (int32_t)return_code::failure)
         ),
         wasm_execution_error
      );
   }

   // 3. n = 0x80000000 (2^31): high-bit set, wraps n*96, n*192, and n*32 all to 0 mod 2^32
   // Host function cleanly returns return_code::failure via 64-bit length checks
   {
      const uint32_t n_highbit = 0x80000000;
      c.push_action( tester1_account, "testg1wsum"_n, tester1_account, mutable_variant_object()
         ("points", std::vector<char>())
         ("scalars", std::vector<char>())
         ("num", n_highbit)
         ("res", zero_res_g1)
         ("expected_error", (int32_t)return_code::failure)
      );

      c.push_action( tester1_account, "testg2wsum"_n, tester1_account, mutable_variant_object()
         ("points", std::vector<char>())
         ("scalars", std::vector<char>())
         ("num", n_highbit)
         ("res", zero_res_g2)
         ("expected_error", (int32_t)return_code::failure)
      );

      c.push_action( tester1_account, "testpairing"_n, tester1_account, mutable_variant_object()
         ("g1_points", std::vector<char>())
         ("g2_points", std::vector<char>())
         ("num", n_highbit)
         ("res", zero_res_pairing)
         ("expected_error", (int32_t)return_code::failure)
      );
   }

   // 4. n = 0xFFFFFFFF (UINT32_MAX): wraps to 4GB span in 32-bit math, trapped by VM
   {
      const uint32_t n_max = 0xFFFFFFFF;
      BOOST_CHECK_THROW(
         c.push_action( tester1_account, "testg1wsum"_n, tester1_account, mutable_variant_object()
            ("points", std::vector<char>())
            ("scalars", std::vector<char>())
            ("num", n_max)
            ("res", zero_res_g1)
            ("expected_error", (int32_t)return_code::failure)
         ),
         wasm_execution_error
      );
   }

   // Part D: Corrupted point bytes (all 0xFF - invalid elliptic curve point)
   {
      std::vector<char> bad_g1(96, (char)0xFF);
      std::vector<char> valid_scalar(32, 0);
      valid_scalar[0] = 1;

      c.push_action( tester1_account, "testg1wsum"_n, tester1_account, mutable_variant_object()
         ("points", bad_g1)
         ("scalars", valid_scalar)
         ("num", 1)
         ("res", zero_res_g1)
         ("expected_error", (int32_t)return_code::failure)
      );

      std::vector<char> bad_g2(192, (char)0xFF);
      c.push_action( tester1_account, "testg2wsum"_n, tester1_account, mutable_variant_object()
         ("points", bad_g2)
         ("scalars", valid_scalar)
         ("num", 1)
         ("res", zero_res_g2)
         ("expected_error", (int32_t)return_code::failure)
      );

      c.push_action( tester1_account, "testpairing"_n, tester1_account, mutable_variant_object()
         ("g1_points", bad_g1)
         ("g2_points", bad_g2)
         ("num", 1)
         ("res", zero_res_pairing)
         ("expected_error", (int32_t)return_code::failure)
      );
   }

} FC_LOG_AND_RETHROW() }

// =============================================================================
// 2. Issue 2: Pending QC Vote Bitset Validated Before Indexing Adversarial Tests
// =============================================================================
BOOST_AUTO_TEST_CASE(pending_qc_bitset_validation_adversarial) try {
   // Generate valid BLS keys
   std::vector<bls_private_key> sk;
   for (int i = 0; i < 5; ++i) {
      sk.push_back(bls_private_key::generate());
   }

   // Active policy: 3 finalizers (sk[0], sk[1], sk[2]), weights {1, 1, 1}, threshold = 2
   finalizer_policies_t policies;
   policies.finality_digest = digest_type(fc::sha256("0000000000000000000000000000000000000000000000000000000000000001"));
   policies.active_finalizer_policy = std::make_shared<finalizer_policy>();
   policies.active_finalizer_policy->generation = 1;
   policies.active_finalizer_policy->threshold = 2;
   policies.active_finalizer_policy->finalizers = {
      finalizer_authority{ "act0", 1, sk[0].get_public_key() },
      finalizer_authority{ "act1", 1, sk[1].get_public_key() },
      finalizer_authority{ "act2", 1, sk[2].get_public_key() }
   };

   // Pending policy: 4 finalizers:
   // sk[3] (index 0 - pending only)
   // sk[0] (index 1 - dual finalizer, index 0 in active!)
   // sk[2] (index 2 - dual finalizer, index 2 in active!)
   // sk[4] (index 3 - pending only)
   // Threshold = 3, weights {1, 1, 1, 1}
   policies.pending_finalizer_policy = std::make_shared<finalizer_policy>();
   policies.pending_finalizer_policy->generation = 2;
   policies.pending_finalizer_policy->threshold = 3;
   policies.pending_finalizer_policy->finalizers = {
      finalizer_authority{ "pen0", 1, sk[3].get_public_key() },
      finalizer_authority{ "pen1", 1, sk[0].get_public_key() }, // dual with act0
      finalizer_authority{ "pen2", 1, sk[2].get_public_key() }, // dual with act2
      finalizer_authority{ "pen3", 1, sk[4].get_public_key() }
   };

   // Valid active qc signature (act0 and act2 vote strong, weight = 2 >= threshold 2)
   vote_bitset_t active_strong(3);
   active_strong[0] = 1;
   active_strong[2] = 1;
   bls_aggregate_signature dummy_sig;
   qc_sig_t active_qc_sig{active_strong, {}, dummy_sig};

   // Adversarial Case 1: Pending strong_votes empty (size 0)
   {
      vote_bitset_t empty_bs(0);
      qc_sig_t pending_sig{empty_bs, {}, dummy_sig};
      qc_t qc{100, active_qc_sig, pending_sig};
      BOOST_CHECK_EXCEPTION( qc.verify_basic(policies), invalid_qc,
         eosio::testing::fc_exception_message_starts_with("vote bitset size is not the same as the number of finalizers") );
   }

   // Adversarial Case 2: Pending weak_votes empty (size 0)
   {
      vote_bitset_t empty_bs(0);
      qc_sig_t pending_sig{{}, empty_bs, dummy_sig};
      qc_t qc{100, active_qc_sig, pending_sig};
      BOOST_CHECK_EXCEPTION( qc.verify_basic(policies), invalid_qc,
         eosio::testing::fc_exception_message_starts_with("vote bitset size is not the same as the number of finalizers") );
   }

   // Adversarial Case 3: Pending strong_votes undersized (size 1 < 4, indexing index 1 or 2 would trigger OOB read)
   {
      vote_bitset_t under_bs(1);
      under_bs[0] = 1;
      qc_sig_t pending_sig{under_bs, {}, dummy_sig};
      qc_t qc{100, active_qc_sig, pending_sig};
      BOOST_CHECK_EXCEPTION( qc.verify_basic(policies), invalid_qc,
         eosio::testing::fc_exception_message_starts_with("vote bitset size is not the same as the number of finalizers") );
   }

   // Adversarial Case 4: Pending weak_votes undersized (size 2 < 4, indexing index 2 would trigger OOB read)
   {
      vote_bitset_t under_bs(2);
      under_bs[0] = 1; under_bs[1] = 1;
      qc_sig_t pending_sig{{}, under_bs, dummy_sig};
      qc_t qc{100, active_qc_sig, pending_sig};
      BOOST_CHECK_EXCEPTION( qc.verify_basic(policies), invalid_qc,
         eosio::testing::fc_exception_message_starts_with("vote bitset size is not the same as the number of finalizers") );
   }

   // Adversarial Case 5: Pending bitset oversized (size 5 > 4)
   {
      vote_bitset_t over_bs(5);
      over_bs[0] = 1; over_bs[1] = 1; over_bs[2] = 1;
      qc_sig_t pending_sig{over_bs, {}, dummy_sig};
      qc_t qc{100, active_qc_sig, pending_sig};
      BOOST_CHECK_EXCEPTION( qc.verify_basic(policies), invalid_qc,
         eosio::testing::fc_exception_message_starts_with("vote bitset size is not the same as the number of finalizers") );
   }

   // Adversarial Case 6: Neither strong nor weak votes present
   {
      qc_sig_t pending_sig{std::nullopt, std::nullopt, dummy_sig};
      qc_t qc{100, active_qc_sig, pending_sig};
      BOOST_CHECK_EXCEPTION( qc.verify_basic(policies), invalid_qc,
         eosio::testing::fc_exception_message_starts_with("Neither strong_votes nor weak_votes present") );
   }

   // Adversarial Case 7: Valid format (size 4), but insufficient weight (< threshold 3)
   {
      vote_bitset_t low_weight(4);
      low_weight[1] = 1; low_weight[2] = 1; // weight = 2 < 3
      qc_sig_t pending_sig{low_weight, {}, dummy_sig};
      qc_t qc{100, active_qc_sig, pending_sig};
      BOOST_CHECK_EXCEPTION( qc.verify_basic(policies), invalid_qc,
         eosio::testing::fc_exception_message_contains("quorum is not met") );
   }

   // Adversarial Case 8: Dual finalizer vote mismatch (act0 voted strong, pen1 voted weak)
   {
      vote_bitset_t pen_weak(4);
      pen_weak[0] = 1; pen_weak[1] = 1; pen_weak[2] = 1; // pen1 (act0) votes weak
      qc_sig_t pending_sig{{}, pen_weak, dummy_sig};
      qc_t qc{100, active_qc_sig, pending_sig};
      BOOST_CHECK_EXCEPTION( qc.verify_basic(policies), invalid_qc,
         eosio::testing::fc_exception_message_contains("does not vote the same on active and pending policies") );
   }

   // Valid Case 9: Dual finalizers vote identically (act0/pen1 and act2/pen2 both vote strong, weight 3 >= 3)
   {
      vote_bitset_t pen_strong(4);
      pen_strong[0] = 1; pen_strong[1] = 1; pen_strong[2] = 1;
      qc_sig_t pending_sig{pen_strong, {}, dummy_sig};
      qc_t qc{100, active_qc_sig, pending_sig};
      BOOST_CHECK_NO_THROW( qc.verify_basic(policies) );
   }

} FC_LOG_AND_RETHROW();

// =============================================================================
// 3. Issue 5: QC Vote Aggregator Weak-Final Boundary Off-by-One Adversarial Tests
// =============================================================================
BOOST_AUTO_TEST_CASE(qc_state_weak_final_boundary_adversarial) try {
   using state_t = aggregating_qc_sig_t::state_t;

   digest_type d(fc::sha256("0000000000000000000000000000000000000000000000000000000000000002"));
   std::vector<uint8_t> digest(d.data(), d.data() + d.data_size());

   std::vector<bls_private_key> sk;
   for (int i = 0; i < 7; ++i) {
      sk.push_back(bls_private_key::generate());
   }

   auto weak_vote = [&](aggregating_qc_sig_t& qc, size_t index, uint64_t weight) {
      return qc.add_vote(0, 0, false, index, sk[index].sign(digest), weight);
   };

   auto strong_vote = [&](aggregating_qc_sig_t& qc, size_t index, uint64_t weight) {
      return qc.add_vote(0, 0, true, index, sk[index].sign(digest), weight);
   };

   // Scenario A: Total Weight = 7 (7 finalizers with weight 1 each), Quorum = 5.
   // max_weak_sum_before_weak_final = 7 - 5 = 2.
   {
      aggregating_qc_sig_t qc(7, 5, 2);
      BOOST_CHECK(qc.state() == state_t::unrestricted);

      // Weak vote 1 (weight 1 <= 2)
      weak_vote(qc, 0, 1);
      BOOST_CHECK(qc.state() == state_t::unrestricted);

      // Weak vote 2 (weight 1 -> weak_sum = 2 == max_weak_sum_before_weak_final)
      // At this boundary, remaining weight is 7 - 2 = 5 == Quorum! Strong QC is still achievable!
      weak_vote(qc, 1, 1);
      BOOST_CHECK(qc.state() == state_t::unrestricted);

      // 5 strong votes arrive (indices 2, 3, 4, 5, 6)
      strong_vote(qc, 2, 1); // strong_sum = 1
      strong_vote(qc, 3, 1); // strong_sum = 2
      strong_vote(qc, 4, 1); // strong_sum = 3, total = 5 >= quorum -> weak_achieved
      BOOST_CHECK(qc.state() == state_t::weak_achieved);

      strong_vote(qc, 5, 1); // strong_sum = 4
      strong_vote(qc, 6, 1); // strong_sum = 5 >= quorum -> transitions to strong!
      BOOST_CHECK(qc.state() == state_t::strong);
      auto best = qc.get_best_qc();
      BOOST_REQUIRE(best.has_value());
      BOOST_CHECK(best->is_strong());
   }

   // Scenario B: Exact weak_achieved boundary where weak_sum == max_weak_sum_before_weak_final
   // Total weight = 6 (F0: 2, F1: 1, F2: 1, F3: 1, F4: 1), Quorum = 4.
   // max_weak_sum_before_weak_final = 6 - 4 = 2.
   {
      aggregating_qc_sig_t qc(5, 4, 2);
      BOOST_CHECK(qc.state() == state_t::unrestricted);

      // F0 strong (weight 2), F1 weak (weight 1), F2 strong (weight 1)
      // total = 4 >= quorum (strong_sum = 3, weak_sum = 1) -> weak_achieved
      strong_vote(qc, 0, 2);
      weak_vote(qc, 1, 1);
      strong_vote(qc, 2, 1);
      BOOST_CHECK(qc.state() == state_t::weak_achieved);

      // F3 weak (weight 1): weak_sum becomes 2 == max_weak_sum_before_weak_final!
      // Pre-fix: used '>=' and falsely transitioned to weak_final!
      // Post-fix: uses '>' so 2 > 2 is false, remains weak_achieved!
      weak_vote(qc, 3, 1);
      BOOST_CHECK(qc.state() == state_t::weak_achieved);

      // F4 strong (weight 1): strong_sum becomes 3 + 1 = 4 >= quorum!
      // Transitions to strong!
      strong_vote(qc, 4, 1);
      BOOST_CHECK(qc.state() == state_t::strong);
      auto best = qc.get_best_qc();
      BOOST_REQUIRE(best.has_value());
      BOOST_CHECK(best->is_strong());
   }

   // Scenario C: Exceeding boundary by 1 (weak_sum = 3 > 2)
   // Total weight = 6, Quorum = 4, max_weak_sum_before_weak_final = 2.
   {
      aggregating_qc_sig_t qc(5, 4, 2);

      // F0 weak (weight 2), F1 weak (weight 1) -> weak_sum = 3 > 2.
      // Total weight remaining for strong is 6 - 3 = 3 < 4 (strong quorum impossible!)
      weak_vote(qc, 0, 2);
      weak_vote(qc, 1, 1);
      BOOST_CHECK(qc.state() == state_t::restricted);

      // F2 strong (weight 1): total = 4 >= quorum -> weak_final
      strong_vote(qc, 2, 1);
      BOOST_CHECK(qc.state() == state_t::weak_final);
      auto best = qc.get_best_qc();
      BOOST_REQUIRE(best.has_value());
      BOOST_CHECK(best->is_weak());

      // Remaining votes cannot make it strong
      strong_vote(qc, 3, 1);
      strong_vote(qc, 4, 1);
      BOOST_CHECK(qc.state() == state_t::weak_final);
   }

   // Scenario D: Boundary Q == W (Unanimous consent required: max_weak_sum_before_weak_final = 0)
   {
      aggregating_qc_sig_t qc(3, 3, 0);
      BOOST_CHECK(qc.state() == state_t::unrestricted);

      // Any weak vote (weight 1 > 0) immediately restricts
      weak_vote(qc, 0, 1);
      BOOST_CHECK(qc.state() == state_t::restricted);

      // Strong votes reach 2 < 3, total = 3 -> weak_final
      strong_vote(qc, 1, 1);
      strong_vote(qc, 2, 1);
      BOOST_CHECK(qc.state() == state_t::weak_final);
   }

   // Scenario E: Duplicate vote rejection
   {
      aggregating_qc_sig_t qc(3, 2, 1);
      BOOST_CHECK(strong_vote(qc, 0, 1) == vote_result_t::success);
      // Re-voting same index as strong or weak must return duplicate
      BOOST_CHECK(strong_vote(qc, 0, 1) == vote_result_t::duplicate);
      BOOST_CHECK(weak_vote(qc, 0, 1) == vote_result_t::duplicate);
   }

} FC_LOG_AND_RETHROW();

// =============================================================================
// 4. Issue 9: Finalizer Policy Large Weight 64-bit Accumulation Adversarial Tests
// =============================================================================
BOOST_AUTO_TEST_CASE(finalizer_policy_accumulation_adversarial) {
   // Case 1: Weights exceeding signed 32-bit INT32_MAX (2,147,483,647)
   {
      finalizer_policy fp;
      fp.threshold = 3'000'000'000ULL;
      fp.finalizers = {
         finalizer_authority{ "f1", 2'147'483'648ULL, bls_public_key{} },
         finalizer_authority{ "f2", 2'147'483'648ULL, bls_public_key{} }
      };
      // Total sum = 4,294,967,296ULL (2^32). Under int32, this wrapped to 0!
      // Expected sum - threshold = 4,294,967,296 - 3,000,000,000 = 1,294,967,296ULL.
      BOOST_CHECK_EQUAL(fp.max_weak_sum_before_weak_final(), 1'294'967'296ULL);
   }

   // Case 2: Huge weights near 100 billion
   {
      finalizer_policy fp;
      fp.threshold = 70'000'000'000ULL;
      fp.finalizers = {
         finalizer_authority{ "f1", 50'000'000'000ULL, bls_public_key{} },
         finalizer_authority{ "f2", 30'000'000'000ULL, bls_public_key{} },
         finalizer_authority{ "f3", 20'000'000'000ULL, bls_public_key{} }
      };
      // Total = 100'000'000'000ULL. Expected sum - threshold = 30'000'000'000ULL.
      BOOST_CHECK_EQUAL(fp.max_weak_sum_before_weak_final(), 30'000'000'000ULL);
   }

   // Case 3: Weights exceeding signed 64-bit INT64_MAX (9.22 * 10^18)
   {
      finalizer_policy fp;
      fp.threshold = 15'000'000'000'000'000'000ULL;
      fp.finalizers = {
         finalizer_authority{ "f1", 10'000'000'000'000'000'000ULL, bls_public_key{} },
         finalizer_authority{ "f2",  8'000'000'000'000'000'000ULL, bls_public_key{} }
      };
      // Total = 18'000'000'000'000'000'000ULL. Expected = 3'000'000'000'000'000'000ULL.
      BOOST_CHECK_EQUAL(fp.max_weak_sum_before_weak_final(), 3'000'000'000'000'000'000ULL);
   }

   // Case 4: Boundary: sum equals threshold
   {
      finalizer_policy fp;
      fp.threshold = 1000ULL;
      fp.finalizers = {
         finalizer_authority{ "f1", 600ULL, bls_public_key{} },
         finalizer_authority{ "f2", 400ULL, bls_public_key{} }
      };
      BOOST_CHECK_EQUAL(fp.max_weak_sum_before_weak_final(), 0ULL);
   }

   // Case 5: Empty finalizer list
   {
      finalizer_policy fp;
      fp.threshold = 0ULL;
      BOOST_CHECK_EQUAL(fp.max_weak_sum_before_weak_final(), 0ULL);
   }
}

// =============================================================================
// 5. Issue 7: Chain-Side Authorization Satisfaction Tracking Adversarial Tests
// =============================================================================
BOOST_AUTO_TEST_CASE_TEMPLATE(auth_satisfied_tracking_adversarial, TESTER, validating_testers) { try {
   TESTER chain;

   chain.create_accounts( {"victim"_n, "attacker"_n, "partner"_n} );
   chain.produce_block();

   // --- Adversarial Attack 1: Attacker constructs transaction naming victim without signature ---
   {
      auto act = chain.get_action(eosio::chain::config::system_account_name, "reqauth"_n,
                                  {permission_level{"victim"_n, eosio::chain::config::active_name}},
                                  fc::mutable_variant_object()("from", "victim"));
      signed_transaction trx;
      trx.actions.emplace_back(std::move(act));
      chain.set_transaction_headers(trx);

      auto ptrx = std::make_shared<packed_transaction>(trx);
      auto meta = transaction_metadata::start_recover_keys(
         ptrx, chain.control->get_thread_pool(), chain.control->get_chain_id(),
         fc::microseconds::maximum(), transaction_metadata::trx_type::input
      ).get();

      BOOST_CHECK_EQUAL(meta->satisfied_authorizations(), false);
      auto trace = chain.control->push_transaction(meta, fc::time_point::maximum(), fc::microseconds::maximum(), 0, false, 0);
      BOOST_REQUIRE(trace->except);
      BOOST_CHECK_EQUAL(trace->except->code(), unsatisfied_authorization::code_value);
      // CRITICAL: declared_auths_satisfied MUST remain false so victim is NOT blamed
      BOOST_CHECK_EQUAL(meta->satisfied_authorizations(), false);
      BOOST_CHECK_EQUAL(meta->declared_auths_satisfied, false);
   }

   // --- Adversarial Attack 2: Attacker signs with attacker's key while naming victim ---
   {
      auto act = chain.get_action(eosio::chain::config::system_account_name, "reqauth"_n,
                                  {permission_level{"victim"_n, eosio::chain::config::active_name}},
                                  fc::mutable_variant_object()("from", "victim"));
      signed_transaction trx;
      trx.actions.emplace_back(std::move(act));
      chain.set_transaction_headers(trx);
      trx.sign( chain.get_private_key("attacker"_n, "active"), chain.control->get_chain_id() );

      auto ptrx = std::make_shared<packed_transaction>(trx);
      auto meta = transaction_metadata::start_recover_keys(
         ptrx, chain.control->get_thread_pool(), chain.control->get_chain_id(),
         fc::microseconds::maximum(), transaction_metadata::trx_type::input
      ).get();

      BOOST_CHECK_EQUAL(meta->satisfied_authorizations(), false);
      auto trace = chain.control->push_transaction(meta, fc::time_point::maximum(), fc::microseconds::maximum(), 0, false, 0);
      BOOST_REQUIRE(trace->except);
      BOOST_CHECK_EQUAL(trace->except->code(), unsatisfied_authorization::code_value);
      // CRITICAL: attacker cannot blame victim
      BOOST_CHECK_EQUAL(meta->satisfied_authorizations(), false);
   }

   // --- Adversarial Attack 3: Multi-auth action where only 1 of 2 required parties signs ---
   {
      auto act = chain.get_action(eosio::chain::config::system_account_name, "reqauth"_n,
                                  {permission_level{"victim"_n, eosio::chain::config::active_name},
                                   permission_level{"partner"_n, eosio::chain::config::active_name}},
                                  fc::mutable_variant_object()("from", "victim"));
      signed_transaction trx;
      trx.actions.emplace_back(std::move(act));
      chain.set_transaction_headers(trx);
      // Only partner signs, victim did not sign
      trx.sign( chain.get_private_key("partner"_n, "active"), chain.control->get_chain_id() );

      auto ptrx = std::make_shared<packed_transaction>(trx);
      auto meta = transaction_metadata::start_recover_keys(
         ptrx, chain.control->get_thread_pool(), chain.control->get_chain_id(),
         fc::microseconds::maximum(), transaction_metadata::trx_type::input
      ).get();

      auto trace = chain.control->push_transaction(meta, fc::time_point::maximum(), fc::microseconds::maximum(), 0, false, 0);
      BOOST_REQUIRE(trace->except);
      BOOST_CHECK_EQUAL(trace->except->code(), unsatisfied_authorization::code_value);
      // Incomplete authorizations must NOT be marked satisfied
      BOOST_CHECK_EQUAL(meta->satisfied_authorizations(), false);

      // Now both sign
      trx.signatures.clear();
      trx.sign( chain.get_private_key("victim"_n, "active"), chain.control->get_chain_id() );
      trx.sign( chain.get_private_key("partner"_n, "active"), chain.control->get_chain_id() );

      auto ptrx_both = std::make_shared<packed_transaction>(trx);
      auto meta_both = transaction_metadata::start_recover_keys(
         ptrx_both, chain.control->get_thread_pool(), chain.control->get_chain_id(),
         fc::microseconds::maximum(), transaction_metadata::trx_type::input
      ).get();

      auto trace_both = chain.control->push_transaction(meta_both, fc::time_point::maximum(), fc::microseconds::maximum(), 0, false, 0);
      BOOST_CHECK(!trace_both->except);
      // Both signed -> satisfied
      BOOST_CHECK_EQUAL(meta_both->satisfied_authorizations(), true);
   }

   // --- Adversarial Attack 4: Transaction with valid auths that FAILS in execution ---
   // (e.g. deleting 'owner' permission which is rejected by system contract assert)
   // In this case, auth WAS satisfied, so failure billing SHOULD apply to authorizer.
   {
      chain.produce_block();
      auto act = chain.get_action(eosio::chain::config::system_account_name, "deleteauth"_n,
                                  {permission_level{"victim"_n, eosio::chain::config::owner_name}},
                                  fc::mutable_variant_object()
                                     ("account", "victim")
                                     ("permission", "owner"));

      signed_transaction trx;
      trx.actions.emplace_back(std::move(act));
      chain.set_transaction_headers(trx);
      trx.sign( chain.get_private_key("victim"_n, "owner"), chain.control->get_chain_id() );

      auto ptrx = std::make_shared<packed_transaction>(trx);
      auto meta = transaction_metadata::start_recover_keys(
         ptrx, chain.control->get_thread_pool(), chain.control->get_chain_id(),
         fc::microseconds::maximum(), transaction_metadata::trx_type::input
      ).get();

      auto trace = chain.control->push_transaction(meta, fc::time_point::maximum(), fc::microseconds::maximum(), 0, false, 0);
      // Execution failed (system contract assert: cannot delete owner)
      BOOST_REQUIRE(trace->except);
      BOOST_CHECK_NE(trace->except->code(), unsatisfied_authorization::code_value);
      // Authorization WAS satisfied before exec failed!
      BOOST_CHECK_EQUAL(meta->satisfied_authorizations(), true);
   }

   // --- Adversarial Attack 5: Poisoned cache / retry simulation ---
   // Simulate unapplied transaction queue retry where metadata object was previously marked satisfied = true
   {
      chain.produce_block();
      auto act = chain.get_action(eosio::chain::config::system_account_name, "reqauth"_n,
                                  {permission_level{"victim"_n, eosio::chain::config::active_name}},
                                  fc::mutable_variant_object()("from", "victim"));
      signed_transaction trx;
      trx.actions.emplace_back(std::move(act));
      chain.set_transaction_headers(trx);
      // Sign with attacker key
      trx.sign( chain.get_private_key("attacker"_n, "active"), chain.control->get_chain_id() );

      auto ptrx = std::make_shared<packed_transaction>(trx);
      auto meta = transaction_metadata::start_recover_keys(
         ptrx, chain.control->get_thread_pool(), chain.control->get_chain_id(),
         fc::microseconds::maximum(), transaction_metadata::trx_type::input
      ).get();

      // Poison the metadata object to simulate leftover true
      meta->declared_auths_satisfied = true;
      BOOST_CHECK_EQUAL(meta->satisfied_authorizations(), true);

      // push_transaction must reset it to false upon entry!
      auto trace = chain.control->push_transaction(meta, fc::time_point::maximum(), fc::microseconds::maximum(), 0, false, 0);
      BOOST_REQUIRE(trace->except);
      BOOST_CHECK_EQUAL(trace->except->code(), unsatisfied_authorization::code_value);
      BOOST_CHECK_EQUAL(meta->satisfied_authorizations(), false);
   }

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
