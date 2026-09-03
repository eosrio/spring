#include <boost/test/unit_test.hpp>

#include <fc/crypto/base58.hpp>
#include <fc/exception/exception.hpp>

using namespace fc;
using namespace std::literals;

BOOST_AUTO_TEST_SUITE(base58_tests)

BOOST_AUTO_TEST_CASE(base58_null_and_empty_safety) try {
   // to_base58 with nullptr and 0 length should return empty string without UB
   BOOST_CHECK_EQUAL(to_base58(nullptr, 0), "");

   // to_base58 with nullptr and non-zero length should throw fc::assert_exception
   BOOST_CHECK_THROW(to_base58(nullptr, 1), fc::assert_exception);
   BOOST_CHECK_THROW(to_base58(nullptr, 100), fc::assert_exception);

   // to_base58 with empty std::vector<char> should return empty string
   std::vector<char> empty_vec;
   BOOST_CHECK_EQUAL(to_base58(empty_vec), "");

   // from_base58 on empty string
   std::vector<char> decoded_empty = from_base58("");
   BOOST_CHECK(decoded_empty.empty());
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(base58_round_trip) try {
   std::string input = "Hello World!";
   std::string encoded = to_base58(input.data(), input.size());
   BOOST_CHECK(!encoded.empty());

   std::vector<char> decoded = from_base58(encoded);
   std::string roundtrip(decoded.data(), decoded.size());
   BOOST_CHECK_EQUAL(input, roundtrip);

   // Vector overload
   std::vector<char> input_vec(input.begin(), input.end());
   std::string encoded_vec = to_base58(input_vec);
   BOOST_CHECK_EQUAL(encoded, encoded_vec);

   // Test with leading zero bytes (Base58 uses '1' for leading zero bytes)
   std::vector<char> with_zeros = {0, 0, 'a', 'b', 'c'};
   std::string encoded_zeros = to_base58(with_zeros);
   BOOST_CHECK_EQUAL(encoded_zeros.substr(0, 2), "11");
   std::vector<char> decoded_zeros = from_base58(encoded_zeros);
   BOOST_CHECK(with_zeros == decoded_zeros);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(base58_invalid_decode) try {
   // Characters not in Base58 alphabet (0, O, I, l)
   BOOST_CHECK_THROW(from_base58("0OIl"), fc::parse_error_exception);
   BOOST_CHECK_THROW(from_base58("invalid!character?"), fc::parse_error_exception);
   // Whitespace alone is valid empty input in Bitcoin/libfc Base58 decoder
   BOOST_CHECK(from_base58(" ").empty());
   BOOST_CHECK_THROW(from_base58("abc\ndef"), fc::parse_error_exception);
   BOOST_CHECK_THROW(from_base58("\x80\xff\xfe"), fc::parse_error_exception);
   BOOST_CHECK_THROW(from_base58("@#$"), fc::parse_error_exception);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(base58_large_payload_and_yield) try {
   // Large byte payload (5000 bytes)
   std::vector<char> large_input(5000);
   for (size_t i = 0; i < large_input.size(); ++i) {
      large_input[i] = static_cast<char>((i * 101 + 37) % 256);
   }

   size_t yield_calls = 0;
   auto yield_func = [&yield_calls]() {
      yield_calls++;
   };

   std::string encoded = to_base58(large_input, yield_func);
   BOOST_CHECK(!encoded.empty());
   BOOST_CHECK_GT(yield_calls, 0u);

   std::vector<char> decoded = from_base58(encoded);
   BOOST_CHECK(large_input == decoded);

   // Fixed output buffer overload
   std::vector<char> fixed_buf(decoded.size());
   size_t decoded_len = from_base58(encoded, fixed_buf.data(), fixed_buf.size());
   BOOST_CHECK_EQUAL(decoded_len, large_input.size());
   BOOST_CHECK(large_input == fixed_buf);

   // Fixed output buffer undersized throws assert_exception
   BOOST_CHECK_THROW(from_base58(encoded, fixed_buf.data(), fixed_buf.size() - 1), fc::assert_exception);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_SUITE_END()

