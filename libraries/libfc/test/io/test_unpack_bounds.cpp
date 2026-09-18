#include <boost/test/unit_test.hpp>

#include <fc/exception/exception.hpp>
#include <fc/io/datastream.hpp>
#include <fc/io/raw.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/container/flat.hpp>
#include <fc/reflect/reflect.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

using namespace fc;

namespace {

#ifdef __linux__
uint64_t current_vm_rss_kb() {
   std::ifstream status("/proc/self/status");
   std::string   line;
   while (std::getline(status, line)) {
      if (line.rfind("VmRSS:", 0) == 0) {
         return static_cast<uint64_t>(std::stoull(line.substr(6)));
      }
   }
   return 0;
}
#endif

void append_unsigned_int(std::vector<char>& out, uint32_t value) {
   auto packed = fc::raw::pack(unsigned_int{value});
   out.insert(out.end(), packed.begin(), packed.end());
}

} // namespace

// Same layout as net_plugin select_ids<sha256> / notice_message.known_* .
struct notice_select_ids {
   int64_t             mode    = 0; // enums pack as int64
   uint32_t            pending = 0;
   std::vector<sha256> ids;
};

FC_REFLECT(notice_select_ids, (mode)(pending)(ids))

BOOST_AUTO_TEST_SUITE(unpack_remaining_bounds_tests)

BOOST_AUTO_TEST_CASE(vector_sha256_huge_claim_on_short_stream_fails_without_large_alloc) {
   // 3-byte unsigned_int(MAX_NUM_ARRAY_ELEMENTS) and no element payload.
   std::vector<char> payload;
   append_unsigned_int(payload, MAX_NUM_ARRAY_ELEMENTS);
   BOOST_REQUIRE_LT(payload.size(), 8u);

#ifdef __linux__
   const uint64_t rss_before = current_vm_rss_kb();
#endif
   const auto t0 = std::chrono::steady_clock::now();

   datastream<const char*> ds(payload.data(), payload.size());
   std::vector<sha256>     ids;
   BOOST_CHECK_THROW(fc::raw::unpack(ds, ids), fc::out_of_range_exception);

   const auto elapsed = std::chrono::steady_clock::now() - t0;
   BOOST_CHECK_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 200);

#ifdef __linux__
   const uint64_t rss_after = current_vm_rss_kb();
   // Unfixed resize(1<<20) of sha256 is ~32MiB. Allow modest exception/test overhead.
   BOOST_CHECK_LT(rss_after > rss_before ? (rss_after - rss_before) : 0, 4096u);
#endif
   BOOST_CHECK(ids.empty());
}

BOOST_AUTO_TEST_CASE(vector_char_huge_claim_on_short_stream_fails_without_large_alloc) {
   std::vector<char> payload;
   append_unsigned_int(payload, MAX_SIZE_OF_BYTE_ARRAYS); // 20 MiB claim

#ifdef __linux__
   const uint64_t rss_before = current_vm_rss_kb();
#endif

   datastream<const char*> ds(payload.data(), payload.size());
   std::vector<char>       bytes;
   BOOST_CHECK_THROW(fc::raw::unpack(ds, bytes), fc::out_of_range_exception);

#ifdef __linux__
   const uint64_t rss_after = current_vm_rss_kb();
   BOOST_CHECK_LT(rss_after > rss_before ? (rss_after - rss_before) : 0, 4096u);
#endif
}

BOOST_AUTO_TEST_CASE(vector_uint32_valid_and_truncated_payload) {
   std::vector<uint32_t> src{1, 2, 3, 4, 5};
   auto                  packed = fc::raw::pack(src);

   {
      auto out = fc::raw::unpack<std::vector<uint32_t>>(packed);
      BOOST_CHECK(out == src);
   }

   // Drop the last element payload but keep the claimed count. Remaining check
   // (5 * 4 > remaining) must fail before resize of a lie.
   BOOST_REQUIRE_GT(packed.size(), 4u);
   packed.resize(packed.size() - 4);
   datastream<const char*> ds(packed.data(), packed.size());
   std::vector<uint32_t>   out;
   BOOST_CHECK_THROW(fc::raw::unpack(ds, out), fc::out_of_range_exception);
}

BOOST_AUTO_TEST_CASE(vector_string_huge_claim_uses_min_element_size) {
   std::vector<char> payload;
   append_unsigned_int(payload, MAX_NUM_ARRAY_ELEMENTS);
   datastream<const char*>  ds(payload.data(), payload.size());
   std::vector<std::string> strings;
   BOOST_CHECK_THROW(fc::raw::unpack(ds, strings), fc::out_of_range_exception);
}

BOOST_AUTO_TEST_CASE(deque_and_flat_vector_huge_claim) {
   std::vector<char> payload;
   append_unsigned_int(payload, MAX_NUM_ARRAY_ELEMENTS);

   {
      datastream<const char*> ds(payload.data(), payload.size());
      std::deque<sha256>      q;
      BOOST_CHECK_THROW(fc::raw::unpack(ds, q), fc::out_of_range_exception);
   }
   {
      datastream<const char*>          ds(payload.data(), payload.size());
      boost::container::vector<sha256> v;
      BOOST_CHECK_THROW(fc::raw::unpack(ds, v), fc::out_of_range_exception);
   }
}

BOOST_AUTO_TEST_CASE(bounded_datastream_notice_like_short_frame) {
   notice_select_ids valid;
   valid.mode    = 1;
   valid.pending = 0;
   valid.ids     = {sha256::hash(std::string("a")), sha256::hash(std::string("b"))};
   auto packed   = fc::raw::pack(valid);
   auto unpacked = fc::raw::unpack<notice_select_ids>(packed);
   BOOST_CHECK_EQUAL(unpacked.mode, valid.mode);
   BOOST_CHECK_EQUAL(unpacked.ids.size(), 2u);
   BOOST_CHECK_EQUAL(unpacked.ids[0], valid.ids[0]);

   // Craft a ~25-byte frame: mode + pending + claimed 1M ids, no id bytes.
   char              frame[25] = {};
   datastream<char*> write_ds(frame, sizeof(frame));
   fc::raw::pack(write_ds, int64_t{0});
   fc::raw::pack(write_ds, uint32_t{0});
   fc::raw::pack(write_ds, unsigned_int{MAX_NUM_ARRAY_ELEMENTS});
   const size_t crafted = write_ds.tellp();
   BOOST_CHECK_LT(crafted, sizeof(frame));
   BOOST_CHECK_LE(crafted, 25u);

#ifdef __linux__
   const uint64_t rss_before = current_vm_rss_kb();
#endif

   datastream<const char*> raw_ds(frame, crafted);
   bounded_datastream      bds(raw_ds, crafted);
   notice_select_ids       evil;
   BOOST_CHECK_THROW(fc::raw::unpack(bds, evil), fc::out_of_range_exception);

#ifdef __linux__
   const uint64_t rss_after = current_vm_rss_kb();
   BOOST_CHECK_LT(rss_after > rss_before ? (rss_after - rss_before) : 0, 4096u);
#endif
}

BOOST_AUTO_TEST_CASE(empty_vector_still_unpacks) {
   auto packed = fc::raw::pack(std::vector<sha256>{});
   auto out    = fc::raw::unpack<std::vector<sha256>>(packed);
   BOOST_CHECK(out.empty());
}

BOOST_AUTO_TEST_SUITE_END()
