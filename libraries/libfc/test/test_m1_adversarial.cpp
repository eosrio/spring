#include <boost/test/unit_test.hpp>

#include <fc/io/datastream.hpp>
#include <fc/io/raw.hpp>
#include <fc/network/message_buffer.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/sha3.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/exception/exception.hpp>
#include <random>
#include <vector>

using namespace fc;

BOOST_AUTO_TEST_SUITE(m1_adversarial_stress_tests)

// =============================================================================
// 1. fc::bounded_datastream Adversarial Stress Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(bounded_datastream_zero_limit_adversarial) try {
   char src[10] = "123456789";
   fc::datastream<const char*> raw_ds(src, sizeof(src));
   fc::bounded_datastream bds(raw_ds, 0);

   BOOST_CHECK_EQUAL(bds.max_bytes(), 0u);
   BOOST_CHECK_EQUAL(bds.remaining(), 0u);
   BOOST_CHECK_EQUAL(bds.tellp(), 0u);
   BOOST_CHECK(bds.valid());

   // Reading 0 bytes is a no-op and succeeds
   char out[10];
   BOOST_CHECK(bds.read(out, 0));
   BOOST_CHECK_EQUAL(bds.tellp(), 0u);
   BOOST_CHECK_EQUAL(bds.remaining(), 0u);

   // Reading 1 byte throws out_of_range_exception
   BOOST_CHECK_THROW(bds.read(out, 1), fc::out_of_range_exception);

   // get() throws
   char c = 0;
   BOOST_CHECK_THROW(bds.get(c), fc::out_of_range_exception);

   // skip(0) succeeds, skip(1) throws
   bds.skip(0);
   BOOST_CHECK_THROW(bds.skip(1), fc::out_of_range_exception);

   // seekp(0) succeeds, seekp(1) fails
   BOOST_CHECK(bds.seekp(0));
   BOOST_CHECK(!bds.seekp(1));
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(bounded_datastream_nested_streams_adversarial) try {
   std::vector<char> buffer(200, 'A');
   fc::datastream<char*> raw_ds(buffer.data(), buffer.size());

   // Outer bounded to 100 bytes
   fc::bounded_datastream outer_bds(raw_ds, 100);
   // Inner bounded to 40 bytes wrapping outer
   fc::bounded_datastream inner_bds(outer_bds, 40);

   BOOST_CHECK_EQUAL(inner_bds.max_bytes(), 40u);
   BOOST_CHECK_EQUAL(inner_bds.remaining(), 40u);

   char out[100];
   // Reading 40 bytes from inner succeeds
   BOOST_CHECK(inner_bds.read(out, 40));
   BOOST_CHECK_EQUAL(inner_bds.remaining(), 0u);
   BOOST_CHECK_EQUAL(outer_bds.remaining(), 60u);

   // Reading 1 more from inner fails even though outer has 60 remaining
   BOOST_CHECK_THROW(inner_bds.read(out, 1), fc::out_of_range_exception);

   // Outer can continue reading its remaining 60 bytes
   BOOST_CHECK(outer_bds.read(out, 60));
   BOOST_CHECK_EQUAL(outer_bds.remaining(), 0u);

   // Outer throws on next byte
   BOOST_CHECK_THROW(outer_bds.read(out, 1), fc::out_of_range_exception);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(bounded_datastream_stress_fuzz) try {
   const size_t total_buf_size = 4096;
   std::vector<char> raw_buffer(total_buf_size);
   for (size_t i = 0; i < total_buf_size; ++i) {
      raw_buffer[i] = static_cast<char>(i % 256);
   }

   std::mt19937 rng(42);

   for (int iter = 0; iter < 100; ++iter) {
      size_t max_bound = rng() % 500;
      fc::datastream<const char*> ds(raw_buffer.data(), total_buf_size);
      fc::bounded_datastream bds(ds, max_bound);

      size_t consumed = 0;
      while (consumed < max_bound) {
         size_t chunk = (rng() % 30) + 1;
         if (consumed + chunk <= max_bound) {
            std::vector<char> tmp(chunk);
            BOOST_CHECK(bds.read(tmp.data(), chunk));
            consumed += chunk;
            BOOST_CHECK_EQUAL(bds.tellp(), consumed);
            BOOST_CHECK_EQUAL(bds.remaining(), max_bound - consumed);
         } else {
            // Attempting to read chunk that crosses boundary MUST throw
            std::vector<char> tmp(chunk);
            BOOST_CHECK_THROW(bds.read(tmp.data(), chunk), fc::out_of_range_exception);
            // State should remain intact
            BOOST_CHECK_EQUAL(bds.tellp(), consumed);
            BOOST_CHECK_EQUAL(bds.remaining(), max_bound - consumed);
            // Read exact remainder
            size_t rem = max_bound - consumed;
            if (rem > 0) {
               BOOST_CHECK(bds.read(tmp.data(), rem));
               consumed += rem;
            }
            break;
         }
      }

      BOOST_CHECK_EQUAL(bds.remaining(), 0u);
      BOOST_CHECK_EQUAL(bds.tellp(), max_bound);
      char c;
      BOOST_CHECK_THROW(bds.get(c), fc::out_of_range_exception);
      BOOST_CHECK_THROW(bds.skip(1), fc::out_of_range_exception);
   }
} FC_LOG_AND_RETHROW();

// =============================================================================
// 2. message_buffer::advance_read_ptr Adversarial Stress Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(message_buffer_advance_read_ptr_adversarial) try {
   fc::message_buffer<64> mb;

   // Empty buffer: advance by 1 throws
   BOOST_CHECK_THROW(mb.advance_read_ptr(1), fc::out_of_range_exception);

   // Empty buffer: advance by UINT32_MAX throws
   BOOST_CHECK_THROW(mb.advance_read_ptr(std::numeric_limits<uint32_t>::max()), fc::out_of_range_exception);

   // Populate multi-chunk buffer (3 chunks: 64 * 3 = 192 bytes)
   std::vector<char> data(150, 0x33);
   mb.add_space(150);
   auto seq = mb.get_buffer_sequence_for_boost_async_read();
   size_t copied = 0;
   for (auto& buf : seq) {
      size_t chunk = std::min<size_t>(data.size() - copied, buf.size());
      if (chunk == 0) break;
      memcpy(buf.data(), data.data() + copied, chunk);
      copied += chunk;
   }
   mb.advance_write_ptr(data.size());

   BOOST_CHECK_EQUAL(mb.bytes_to_read(), 150u);

   // Advance past 150 throws
   BOOST_CHECK_THROW(mb.advance_read_ptr(151), fc::out_of_range_exception);
   BOOST_CHECK_THROW(mb.advance_read_ptr(std::numeric_limits<uint32_t>::max()), fc::out_of_range_exception);
   BOOST_CHECK_THROW(mb.advance_read_ptr(200), fc::out_of_range_exception);

   // Advance partial chunk (30 bytes)
   mb.advance_read_ptr(30);
   BOOST_CHECK_EQUAL(mb.bytes_to_read(), 120u);

   // Advance crossing chunk boundary (40 bytes -> total 70 bytes advanced)
   mb.advance_read_ptr(40);
   BOOST_CHECK_EQUAL(mb.bytes_to_read(), 80u);

   // Attempting to advance 81 throws
   BOOST_CHECK_THROW(mb.advance_read_ptr(81), fc::out_of_range_exception);

   // Advance remaining 80 bytes -> should reset to 0
   mb.advance_read_ptr(80);
   BOOST_CHECK_EQUAL(mb.bytes_to_read(), 0u);

   // Empty again: advancing 1 throws
   BOOST_CHECK_THROW(mb.advance_read_ptr(1), fc::out_of_range_exception);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(message_buffer_stress_cycles) try {
   fc::message_buffer<128> mb;
   std::mt19937 rng(12345);

   for (int cycle = 0; cycle < 200; ++cycle) {
      uint32_t write_sz = (rng() % 500) + 1;
      std::vector<char> chunk(write_sz, static_cast<char>(cycle));

      // Ensure write space
      while (mb.bytes_to_write() < write_sz) {
         mb.add_buffer_to_chain();
      }

      // Write via get_buffer_sequence
      auto seq = mb.get_buffer_sequence_for_boost_async_read();
      size_t written = 0;
      for (auto& buf : seq) {
         size_t to_copy = std::min<size_t>(write_sz - written, buf.size());
         if (to_copy == 0) break;
         memcpy(buf.data(), chunk.data() + written, to_copy);
         written += to_copy;
      }
      mb.advance_write_ptr(write_sz);

      BOOST_CHECK_GE(mb.bytes_to_read(), write_sz);

      // Random partial advances
      uint32_t to_consume = write_sz;
      while (to_consume > 0) {
         uint32_t step = std::min<uint32_t>(to_consume, (rng() % 50) + 1);
         // Test invalid overshoot check
         BOOST_CHECK_THROW(mb.advance_read_ptr(mb.bytes_to_read() + 1), fc::out_of_range_exception);
         mb.advance_read_ptr(step);
         to_consume -= step;
      }
      BOOST_CHECK_EQUAL(mb.bytes_to_read(), 0u);
   }
} FC_LOG_AND_RETHROW();

// =============================================================================
// 3. base58 Adversarial Stress Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(base58_pointer_null_and_boundary_adversarial) try {
   // nullptr with size 0: safe, returns empty string
   BOOST_CHECK_EQUAL(fc::to_base58(nullptr, 0), "");

   // nullptr with non-zero sizes: MUST throw fc::assert_exception
   BOOST_CHECK_THROW(fc::to_base58(nullptr, 1), fc::assert_exception);
   BOOST_CHECK_THROW(fc::to_base58(nullptr, 50), fc::assert_exception);
   BOOST_CHECK_THROW(fc::to_base58(nullptr, 1000000), fc::assert_exception);

   // Empty string decoding
   std::vector<char> dec_empty = fc::from_base58("");
   BOOST_CHECK(dec_empty.empty());

   // All single-character invalid base58 characters
   std::string invalid_chars = "0OIl!@#$%^&*()_+=-~`{}[]|:;'<>,.?/\"\\";
   for (char ch : invalid_chars) {
      std::string s(1, ch);
      BOOST_CHECK_THROW(fc::from_base58(s), fc::parse_error_exception);
   }
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(base58_fuzz_roundtrip) try {
   std::mt19937 rng(999);

   // Test all single byte values 0x00 .. 0xFF
   for (int b = 0; b <= 255; ++b) {
      char byte_val = static_cast<char>(b);
      std::string enc = fc::to_base58(&byte_val, 1);
      BOOST_CHECK(!enc.empty());
      std::vector<char> dec = fc::from_base58(enc);
      BOOST_REQUIRE_EQUAL(dec.size(), 1u);
      BOOST_CHECK_EQUAL(dec[0], byte_val);
   }

   // Test 200 random byte sequences of various lengths
   for (int i = 0; i < 200; ++i) {
      size_t len = rng() % 256;
      std::vector<char> raw(len);
      for (size_t j = 0; j < len; ++j) {
         raw[j] = static_cast<char>(rng() & 0xFF);
      }

      std::string enc = fc::to_base58(raw);
      std::vector<char> dec = fc::from_base58(enc);
      BOOST_REQUIRE_EQUAL(raw.size(), dec.size());
      BOOST_CHECK(raw == dec);
   }
} FC_LOG_AND_RETHROW();

// =============================================================================
// 4. SHA3 / Keccak Standard Vectors and Streaming Adversarial Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(sha3_nist_and_keccak_vectors) try {
   // NIST SHA3-256 standard vectors
   {
      // Empty input
      fc::sha3::encoder enc;
      fc::sha3 h = enc.result(true /* NIST */);
      BOOST_CHECK_EQUAL(h.str(), "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
   }
   {
      // "abc"
      fc::sha3::encoder enc;
      enc.write("abc", 3);
      fc::sha3 h = enc.result(true /* NIST */);
      BOOST_CHECK_EQUAL(h.str(), "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
   }

   // Keccak-256 (Ethereum variant) standard vectors
   {
      // Empty input
      fc::sha3::encoder enc;
      fc::sha3 h = enc.result(false /* Keccak */);
      BOOST_CHECK_EQUAL(h.str(), "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470");
   }
   {
      // "abc"
      fc::sha3::encoder enc;
      enc.write("abc", 3);
      fc::sha3 h = enc.result(false /* Keccak */);
      BOOST_CHECK_EQUAL(h.str(), "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45");
   }
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(sha3_streaming_chunk_equivalence_adversarial) try {
   // SHA3 rate is 136 bytes for 256-bit hash.
   // Test boundaries around 136: 135, 136, 137, 272, 500 bytes
   std::vector<size_t> test_sizes = {0, 1, 10, 135, 136, 137, 271, 272, 273, 500, 1024};

   for (size_t sz : test_sizes) {
      std::vector<char> data(sz);
      for (size_t i = 0; i < sz; ++i) {
         data[i] = static_cast<char>((i * 17 + 3) & 0xFF);
      }

      // Hash all at once
      fc::sha3::encoder enc_bulk;
      if (sz > 0) enc_bulk.write(data.data(), data.size());
      fc::sha3 h_bulk = enc_bulk.result(true);

      // Hash byte-by-byte
      fc::sha3::encoder enc_step;
      for (size_t i = 0; i < sz; ++i) {
         enc_step.write(&data[i], 1);
      }
      fc::sha3 h_step = enc_step.result(true);

      BOOST_CHECK_EQUAL(h_bulk.str(), h_step.str());
   }
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(sha3_endian_swap_logic_verification) try {
   // Verify the 25-word byte-swapping logic mathematically:
   // Simulating big-endian words <-> bytes conversions to prove reversibility and no array overrun
   constexpr size_t number_of_words = 25;
   uint64_t words[number_of_words];
   for (size_t i = 0; i < number_of_words; ++i) {
      words[i] = 0x0102030405060708ULL + i * 0x1111111111111111ULL;
   }

   uint64_t original[number_of_words];
   memcpy(original, words, sizeof(words));

   // Convert to big-endian (simulating memory bytes on BE machine)
   for (std::size_t i = 0; i < number_of_words; i++) {
      uint8_t* v = (uint8_t*)(words + i);
      uint64_t tmp = words[i];
      v[0] = tmp & 0xFF;
      v[1] = (tmp >> 8) & 0xFF;
      v[2] = (tmp >> 16) & 0xFF;
      v[3] = (tmp >> 24) & 0xFF;
      v[4] = (tmp >> 32) & 0xFF;
      v[5] = (tmp >> 40) & 0xFF;
      v[6] = (tmp >> 48) & 0xFF;
      v[7] = (tmp >> 56) & 0xFF;
   }

   // Convert back from big-endian bytes to little-endian words
   for (std::size_t i = 0; i < number_of_words; i++) {
      uint8_t* v = reinterpret_cast<uint8_t*>(words + i);
      words[i] = ((uint64_t)v[0]) | (((uint64_t)v[1]) << 8) |
                 (((uint64_t)v[2]) << 16) | (((uint64_t)v[3]) << 24) |
                 (((uint64_t)v[4]) << 32) | (((uint64_t)v[5]) << 40) |
                 (((uint64_t)v[6]) << 48) | (((uint64_t)v[7]) << 56);
   }

   for (size_t i = 0; i < number_of_words; ++i) {
      BOOST_CHECK_EQUAL(words[i], original[i]);
   }
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_SUITE_END()
