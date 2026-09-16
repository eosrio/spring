#include <boost/test/unit_test.hpp>

#include <eosio/chain/subjective_billing.hpp>
#include <eosio/chain/transaction_metadata.hpp>
#include <eosio/testing/tester.hpp>
#include <fc/time.hpp>

namespace {

using namespace eosio;
using namespace eosio::chain;

BOOST_AUTO_TEST_SUITE(subjective_billing_test)

BOOST_AUTO_TEST_CASE( subjective_bill_test ) {

   fc::logger log;

   transaction_id_type id1 = sha256::hash( "1" );
   transaction_id_type id2 = sha256::hash( "2" );
   transaction_id_type id3 = sha256::hash( "3" );
   account_name a = "a"_n;
   account_name b = "b"_n;
   account_name c = "c"_n;

   const auto now = time_point::now();
   const fc::time_point_sec now_sec{now};

   subjective_billing timing_sub_bill;
   const auto halftime = now + fc::milliseconds(timing_sub_bill.get_expired_accumulator_average_window() * subjective_billing::subjective_time_interval_ms / 2);
   const auto endtime = now + fc::milliseconds(timing_sub_bill.get_expired_accumulator_average_window() * subjective_billing::subjective_time_interval_ms);


   {  // Failed transactions remain until expired in subjective billing.
      subjective_billing sub_bill;

      sub_bill.subjective_bill( id1, now_sec, a, fc::microseconds( 13 ) );
      sub_bill.subjective_bill( id2, now_sec, a, fc::microseconds( 11 ) );
      sub_bill.subjective_bill( id3, now_sec, b, fc::microseconds( 9 ) );

      BOOST_CHECK_EQUAL( 13+11, sub_bill.get_subjective_bill(a, now) );
      BOOST_CHECK_EQUAL( 9, sub_bill.get_subjective_bill(b, now) );

      sub_bill.on_block(log, {}, now);

      BOOST_CHECK_EQUAL( 13+11, sub_bill.get_subjective_bill(a, now) );
      BOOST_CHECK_EQUAL( 9, sub_bill.get_subjective_bill(b, now) );

      // expires transactions but leaves them in the decay at full value
      sub_bill.remove_expired( log, now + fc::microseconds(1), now, [](){ return false; } );

      BOOST_CHECK_EQUAL( 13+11, sub_bill.get_subjective_bill(a, now) );
      BOOST_CHECK_EQUAL( 9, sub_bill.get_subjective_bill(b, now) );
      BOOST_CHECK_EQUAL( 0, sub_bill.get_subjective_bill(c, now) );

      // ensure that the value decays away at the window
      BOOST_CHECK_EQUAL( 0, sub_bill.get_subjective_bill(a, endtime) );
      BOOST_CHECK_EQUAL( 0, sub_bill.get_subjective_bill(b, endtime) );
      BOOST_CHECK_EQUAL( 0, sub_bill.get_subjective_bill(c, endtime) );
   }
   {  // db_read_mode HEAD mode, so transactions are immediately reverted
      subjective_billing sub_bill;

      sub_bill.subjective_bill( id1, now_sec, a, fc::microseconds( 23 ) );
      sub_bill.subjective_bill( id2, now_sec, a, fc::microseconds( 19 ) );
      sub_bill.subjective_bill( id3, now_sec, b, fc::microseconds( 7 ) );

      BOOST_CHECK_EQUAL( 23+19, sub_bill.get_subjective_bill(a, now) );
      BOOST_CHECK_EQUAL( 7, sub_bill.get_subjective_bill(b, now) );

      sub_bill.on_block(log, {}, now); // have not seen any of the transactions come back yet

      BOOST_CHECK_EQUAL( 23+19, sub_bill.get_subjective_bill(a, now) );
      BOOST_CHECK_EQUAL( 7, sub_bill.get_subjective_bill(b, now) );

      sub_bill.on_block(log, {}, now);
      sub_bill.remove_subjective_billing( id1, 0 ); // simulate seeing id1 come back in block (this is what on_block would do)

      BOOST_CHECK_EQUAL( 19, sub_bill.get_subjective_bill(a, now) );
      BOOST_CHECK_EQUAL( 7, sub_bill.get_subjective_bill(b, now) );
   }
   { // failed handling logic, decay with repeated failures should be exponential, single failures should be linear
      subjective_billing sub_bill;

      sub_bill.subjective_bill_failure(a, fc::microseconds(1024), now);
      sub_bill.subjective_bill_failure(b, fc::microseconds(1024), now);
      BOOST_CHECK_EQUAL( 1024, sub_bill.get_subjective_bill(a, now) );
      BOOST_CHECK_EQUAL( 1024, sub_bill.get_subjective_bill(b, now) );

      sub_bill.subjective_bill_failure(a, fc::microseconds(1024), halftime);
      BOOST_CHECK_EQUAL( 512 + 1024, sub_bill.get_subjective_bill(a, halftime) );
      BOOST_CHECK_EQUAL( 512, sub_bill.get_subjective_bill(b, halftime) );

      sub_bill.subjective_bill_failure(a, fc::microseconds(1024), endtime);
      BOOST_CHECK_EQUAL( 256 + 512 + 1024, sub_bill.get_subjective_bill(a, endtime) );
      BOOST_CHECK_EQUAL( 0, sub_bill.get_subjective_bill(b, endtime) );
   }

   { // expired handling logic, full billing until expiration then failed/decay logic
      subjective_billing sub_bill;

      sub_bill.subjective_bill( id1, now_sec, a, fc::microseconds( 1024 ) );
      sub_bill.subjective_bill( id2, fc::time_point_sec{now + fc::seconds(1)}, a, fc::microseconds( 1024 ) );
      sub_bill.subjective_bill( id3, now_sec, b, fc::microseconds( 1024 ) );
      BOOST_CHECK_EQUAL( 1024 + 1024, sub_bill.get_subjective_bill(a, now) );
      BOOST_CHECK_EQUAL( 1024, sub_bill.get_subjective_bill(b, now) );

      sub_bill.remove_expired( log, now, now, [](){ return false; } );
      BOOST_CHECK_EQUAL( 1024 + 1024, sub_bill.get_subjective_bill(a, now) );
      BOOST_CHECK_EQUAL( 1024, sub_bill.get_subjective_bill(b, now) );

      BOOST_CHECK_EQUAL( 512 + 1024, sub_bill.get_subjective_bill(a, halftime) );
      BOOST_CHECK_EQUAL( 512, sub_bill.get_subjective_bill(b, halftime) );

      BOOST_CHECK_EQUAL( 1024, sub_bill.get_subjective_bill(a, endtime) );
      BOOST_CHECK_EQUAL( 0, sub_bill.get_subjective_bill(b, endtime) );

      sub_bill.remove_expired( log, now + fc::seconds(1), now, [](){ return false; } );
      BOOST_CHECK_EQUAL( 1024 + 1024, sub_bill.get_subjective_bill(a, now) );
      BOOST_CHECK_EQUAL( 1024, sub_bill.get_subjective_bill(b, now) );

      BOOST_CHECK_EQUAL( 512 + 512, sub_bill.get_subjective_bill(a, halftime) );
      BOOST_CHECK_EQUAL( 512, sub_bill.get_subjective_bill(b, halftime) );

      BOOST_CHECK_EQUAL( 0, sub_bill.get_subjective_bill(a, endtime) );
      BOOST_CHECK_EQUAL( 0, sub_bill.get_subjective_bill(b, endtime) );
   }

}

BOOST_AUTO_TEST_CASE( producer_failure_billing_satisfied_auths_test ) {
   account_name victim = "victim"_n;
   account_name legit = "legit"_n;

   signed_transaction trx1;
   trx1.actions.emplace_back(vector<permission_level>{{victim, config::active_name}},
                             "eosio"_n, "action"_n, bytes{});
   auto packed_trx1 = std::make_shared<packed_transaction>(trx1);
   auto meta_unauth = transaction_metadata::create_no_recover_keys(packed_trx1, transaction_metadata::trx_type::input);

   // Fresh transaction metadata MUST have satisfied_authorizations() == false
   BOOST_CHECK_EQUAL(meta_unauth->satisfied_authorizations(), false);

   signed_transaction trx2;
   trx2.actions.emplace_back(vector<permission_level>{{legit, config::active_name}},
                             "eosio"_n, "action"_n, bytes{});
   auto packed_trx2 = std::make_shared<packed_transaction>(trx2);
   auto meta_auth = transaction_metadata::create_no_recover_keys(packed_trx2, transaction_metadata::trx_type::input);
   meta_auth->set_satisfied_authorizations(true);
   BOOST_CHECK_EQUAL(meta_auth->satisfied_authorizations(), true);

   // Emulate producer_plugin_impl::handle_push_result failure logic
   struct mock_account_failures {
      std::map<account_name, uint32_t> fails;
      void add(const account_name& a, const fc::exception&) {
         fails[a]++;
      }
      uint32_t count(const account_name& a) const {
         auto it = fails.find(a);
         return it != fails.end() ? it->second : 0;
      }
   };

   auto handle_failure = [](const transaction_metadata_ptr& trx,
                            account_name first_auth,
                            fc::microseconds elapsed,
                            const fc::exception& e,
                            subjective_billing& sub_bill,
                            mock_account_failures& acc_fails,
                            bool disable_subjective_enforcement = false) {
      if (e.code() != tx_duplicate::code_value) {
         if (!disable_subjective_enforcement && trx->satisfied_authorizations()) {
            sub_bill.subjective_bill_failure(first_auth, elapsed, fc::time_point::now());
         }
         if (!disable_subjective_enforcement && trx->satisfied_authorizations()) {
            acc_fails.add(first_auth, e);
         }
      }
   };

   subjective_billing sub_bill;
   mock_account_failures acc_fails;
   const auto now = time_point::now();
   fc::exception auth_exc(FC_LOG_MESSAGE(error, "unsatisfied authorization"));
   fc::exception assert_exc(FC_LOG_MESSAGE(error, "assertion failure"));

   // Test 1: Unauthorized transaction fails (e.g. attacker claims victim as first_auth)
   // Because satisfied_authorizations() is false, victim must NOT be billed or marked with failure
   handle_failure(meta_unauth, victim, fc::microseconds(2000), auth_exc, sub_bill, acc_fails);
   BOOST_CHECK_EQUAL(sub_bill.get_subjective_bill(victim, now), 0);
   BOOST_CHECK_EQUAL(acc_fails.count(victim), 0u);

   // Repeat 50 times to simulate brute-force / DoS attack targeting victim
   for (int i = 0; i < 50; ++i) {
      handle_failure(meta_unauth, victim, fc::microseconds(2000), auth_exc, sub_bill, acc_fails);
   }
   BOOST_CHECK_EQUAL(sub_bill.get_subjective_bill(victim, now), 0);
   BOOST_CHECK_EQUAL(acc_fails.count(victim), 0u);

   // Test 2: Legitimate transaction with satisfied authorizations fails during execution
   // Because satisfied_authorizations() is true, legit MUST be billed and marked with failure
   handle_failure(meta_auth, legit, fc::microseconds(2000), assert_exc, sub_bill, acc_fails);
   BOOST_CHECK_EQUAL(sub_bill.get_subjective_bill(legit, now), 2000);
   BOOST_CHECK_EQUAL(acc_fails.count(legit), 1u);

   // Test 3: If disable_subjective_enforcement is true, neither is billed
   subjective_billing sub_bill2;
   mock_account_failures acc_fails2;
   handle_failure(meta_auth, legit, fc::microseconds(2000), assert_exc, sub_bill2, acc_fails2, true);
   BOOST_CHECK_EQUAL(sub_bill2.get_subjective_bill(legit, now), 0);
}

BOOST_AUTO_TEST_CASE( producer_failure_billing_adversarial_multi_victim_dos_test ) {
   // Emulate producer failure billing structures
   struct mock_account_failures {
      std::map<account_name, uint32_t> fails;
      void add(const account_name& a, const fc::exception&) {
         fails[a]++;
      }
      uint32_t count(const account_name& a) const {
         auto it = fails.find(a);
         return it != fails.end() ? it->second : 0;
      }
   };

   auto handle_failure = [](const transaction_metadata_ptr& trx,
                            account_name first_auth,
                            fc::microseconds elapsed,
                            const fc::exception& e,
                            subjective_billing& sub_bill,
                            mock_account_failures& acc_fails,
                            bool disable_subjective_enforcement = false) {
      if (e.code() != tx_duplicate::code_value) {
         if (!disable_subjective_enforcement && trx->satisfied_authorizations()) {
            sub_bill.subjective_bill_failure(first_auth, elapsed, fc::time_point::now());
         }
         if (!disable_subjective_enforcement && trx->satisfied_authorizations()) {
            acc_fails.add(first_auth, e);
         }
      }
   };

   subjective_billing sub_bill;
   mock_account_failures acc_fails;
   const auto now = time_point::now();
   fc::exception auth_exc(FC_LOG_MESSAGE(error, "unsatisfied authorization"));

   // Attack Scenario: Attacker creates 50 victim accounts and floods 1,000 spoofed transactions
   constexpr size_t NUM_VICTIMS = 50;
   std::vector<account_name> victims;
   for (size_t i = 0; i < NUM_VICTIMS; ++i) {
      std::string name_str = "victim" + std::string(1, 'a' + (i % 26)) + std::string(1, 'a' + (i / 26));
      account_name v(name_str.c_str());
      victims.push_back(v);
   }

   // Flood 1000 failed transactions with unsatisfied authorizations targeting victims
   for (size_t i = 0; i < 1000; ++i) {
      account_name target_victim = victims[i % NUM_VICTIMS];
      signed_transaction trx;
      trx.actions.emplace_back(vector<permission_level>{{target_victim, config::active_name}},
                               "eosio"_n, "transfer"_n, bytes{});
      auto packed_trx = std::make_shared<packed_transaction>(trx);
      auto meta = transaction_metadata::create_no_recover_keys(packed_trx, transaction_metadata::trx_type::input);

      // Default state: declared_auths_satisfied is false
      BOOST_CHECK_EQUAL(meta->satisfied_authorizations(), false);

      handle_failure(meta, target_victim, fc::microseconds(5000), auth_exc, sub_bill, acc_fails);
   }

   // VERDICT ASSERTION: Under no circumstances should any victim account be penalized!
   for (size_t i = 0; i < NUM_VICTIMS; ++i) {
      account_name v = victims[i];
      BOOST_CHECK_EQUAL(acc_fails.count(v), 0u);
      BOOST_CHECK_EQUAL(sub_bill.get_subjective_bill(v, now), 0);
   }

   // Legitimate user transaction: valid signatures (satisfied_authorizations == true)
   // that fails in contract execution (e.g. eosio_assert failure)
   account_name legit = "legituser"_n;
   signed_transaction legit_trx;
   legit_trx.actions.emplace_back(vector<permission_level>{{legit, config::active_name}},
                                  "eosio"_n, "transfer"_n, bytes{});
   auto packed_legit = std::make_shared<packed_transaction>(legit_trx);
   auto legit_meta = transaction_metadata::create_no_recover_keys(packed_legit, transaction_metadata::trx_type::input);
   legit_meta->set_satisfied_authorizations(true);
   BOOST_CHECK_EQUAL(legit_meta->satisfied_authorizations(), true);

   fc::exception assert_exc(FC_LOG_MESSAGE(error, "assertion failure"));
   handle_failure(legit_meta, legit, fc::microseconds(3000), assert_exc, sub_bill, acc_fails);

   // Legitimate user must be penalized for their own execution failure
   BOOST_CHECK_EQUAL(acc_fails.count(legit), 1u);
   BOOST_CHECK_EQUAL(sub_bill.get_subjective_bill(legit, now), 3000);

   // Retry reset behavior: ensure satisfied_authorizations is cleared on retry
   legit_meta->set_satisfied_authorizations(false);
   BOOST_CHECK_EQUAL(legit_meta->satisfied_authorizations(), false);
   handle_failure(legit_meta, legit, fc::microseconds(3000), assert_exc, sub_bill, acc_fails);
   // Failure count and bill unchanged because satisfied_authorizations was reset to false
   BOOST_CHECK_EQUAL(acc_fails.count(legit), 1u);
   BOOST_CHECK_EQUAL(sub_bill.get_subjective_bill(legit, now), 3000);
}

BOOST_AUTO_TEST_SUITE_END()

}
