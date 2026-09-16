#include <boost/test/unit_test.hpp>

#include <fc/io/datastream.hpp>
#include <fc/io/raw.hpp>
#include <fc/network/message_buffer.hpp>
#include <fc/exception/exception.hpp>

using namespace fc;

BOOST_AUTO_TEST_SUITE(bounded_datastream_tests)

BOOST_AUTO_TEST_CASE(bounded_datastream_read_bounds) try {
   char buffer[100];
   memset(buffer, 0x42, sizeof(buffer));

   fc::datastream<const char*> raw_ds(buffer, sizeof(buffer));
   fc::bounded_datastream bds(raw_ds, 20);

   BOOST_CHECK_EQUAL(bds.max_bytes(), 20u);
   BOOST_CHECK_EQUAL(bds.remaining(), 20u);
   BOOST_CHECK_EQUAL(bds.tellp(), 0u);
   BOOST_CHECK(bds.valid());

   char out[30];
   // Read 10 bytes: success
   BOOST_CHECK(bds.read(out, 10));
   BOOST_CHECK_EQUAL(bds.remaining(), 10u);
   BOOST_CHECK_EQUAL(bds.tellp(), 10u);

   // Attempting to read 11 bytes when only 10 remain: throws out_of_range_exception
   BOOST_CHECK_THROW(bds.read(out, 11), fc::out_of_range_exception);

   // Read 8 bytes: success
   BOOST_CHECK(bds.read(out, 8));
   BOOST_CHECK_EQUAL(bds.remaining(), 2u);
   BOOST_CHECK_EQUAL(bds.tellp(), 18u);

   // Read remaining 2 bytes: success
   char c1 = 0;
   BOOST_CHECK(bds.get(c1));
   BOOST_CHECK_EQUAL(c1, 0x42);
   BOOST_CHECK_EQUAL(bds.remaining(), 1u);

   unsigned char uc = 0;
   BOOST_CHECK(bds.get(uc));
   BOOST_CHECK_EQUAL(uc, 0x42);
   BOOST_CHECK_EQUAL(bds.remaining(), 0u);
   BOOST_CHECK_EQUAL(bds.tellp(), 20u);

   // Reading beyond remaining 0: throws
   BOOST_CHECK_THROW(bds.read(out, 1), fc::out_of_range_exception);
   BOOST_CHECK_THROW(bds.get(c1), fc::out_of_range_exception);
   BOOST_CHECK_THROW(bds.get(uc), fc::out_of_range_exception);
   BOOST_CHECK_THROW(bds.skip(1), fc::out_of_range_exception);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(bounded_datastream_write_bounds) try {
   char buffer[100];
   memset(buffer, 0, sizeof(buffer));

   fc::datastream<char*> raw_ds(buffer, sizeof(buffer));
   fc::bounded_datastream bds(raw_ds, 15);

   BOOST_CHECK_EQUAL(bds.max_bytes(), 15u);
   BOOST_CHECK_EQUAL(bds.remaining(), 15u);

   const char* text = "1234567890";
   BOOST_CHECK(bds.write(text, 10));
   BOOST_CHECK_EQUAL(bds.remaining(), 5u);
   BOOST_CHECK_EQUAL(bds.tellp(), 10u);

   // Writing 6 bytes when 5 remain: throws
   BOOST_CHECK_THROW(bds.write(text, 6), fc::out_of_range_exception);

   // Put 4 bytes
   BOOST_CHECK(bds.put('a'));
   BOOST_CHECK(bds.put('b'));
   BOOST_CHECK(bds.put('c'));
   BOOST_CHECK(bds.put('d'));
   BOOST_CHECK_EQUAL(bds.remaining(), 1u);

   // Put last byte
   BOOST_CHECK(bds.put('e'));
   BOOST_CHECK_EQUAL(bds.remaining(), 0u);

   // Writing / putting when full: throws
   BOOST_CHECK_THROW(bds.put('z'), fc::out_of_range_exception);
   BOOST_CHECK_THROW(bds.write(text, 1), fc::out_of_range_exception);

   // Verify written content
   BOOST_CHECK_EQUAL(std::string(buffer, 15), "1234567890abcde");
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(bounded_datastream_skip_and_seek) try {
   char buffer[50];
   fc::datastream<char*> raw_ds(buffer, sizeof(buffer));
   fc::bounded_datastream bds(raw_ds, 30);

   bds.skip(10);
   BOOST_CHECK_EQUAL(bds.tellp(), 10u);
   BOOST_CHECK_EQUAL(bds.remaining(), 20u);

   // Skip past limit: throws
   BOOST_CHECK_THROW(bds.skip(21), fc::out_of_range_exception);

   // Seek forward
   BOOST_CHECK(bds.seekp(25));
   BOOST_CHECK_EQUAL(bds.tellp(), 25u);
   BOOST_CHECK_EQUAL(bds.remaining(), 5u);

   // Seek past limit returns false
   BOOST_CHECK(!bds.seekp(31));

   // Seek backward (supported since raw_ds supports seekp)
   BOOST_CHECK(bds.seekp(5));
   BOOST_CHECK_EQUAL(bds.tellp(), 5u);
   BOOST_CHECK_EQUAL(bds.remaining(), 25u);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(bounded_datastream_pack_unpack) try {
   char buffer[256];
   fc::datastream<char*> write_raw(buffer, sizeof(buffer));
   fc::bounded_datastream write_bds(write_raw, 256);

   std::string test_str = "hello bounded datastream";
   uint32_t val32 = 0x12345678;
   uint64_t val64 = 0xdeadbeefcafebabeULL;

   fc::raw::pack(write_bds, test_str);
   fc::raw::pack(write_bds, val32);
   fc::raw::pack(write_bds, val64);

   size_t packed_size = write_bds.tellp();

   // Unpack with exact bound
   {
      fc::datastream<const char*> read_raw(buffer, packed_size);
      fc::bounded_datastream read_bds(read_raw, packed_size);

      std::string out_str;
      uint32_t out_32 = 0;
      uint64_t out_64 = 0;

      fc::raw::unpack(read_bds, out_str);
      fc::raw::unpack(read_bds, out_32);
      fc::raw::unpack(read_bds, out_64);

      BOOST_CHECK_EQUAL(out_str, test_str);
      BOOST_CHECK_EQUAL(out_32, val32);
      BOOST_CHECK_EQUAL(out_64, val64);
      BOOST_CHECK_EQUAL(read_bds.remaining(), 0u);
   }

   // Unpack with truncated bound: must throw fc::out_of_range_exception
   {
      fc::datastream<const char*> read_raw(buffer, packed_size);
      fc::bounded_datastream read_bds(read_raw, packed_size - 4); // 4 bytes too short for val64

      std::string out_str;
      uint32_t out_32 = 0;
      uint64_t out_64 = 0;

      fc::raw::unpack(read_bds, out_str);
      fc::raw::unpack(read_bds, out_32);
      BOOST_CHECK_THROW(fc::raw::unpack(read_bds, out_64), fc::out_of_range_exception);
   }
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(bounded_datastream_with_message_buffer) try {
   fc::message_buffer<1024> mb;

   std::vector<char> payload(200, 'Z');
   memcpy(mb.write_ptr(), payload.data(), payload.size());
   mb.advance_write_ptr(payload.size());

   auto mb_ds = mb.create_datastream();
   // Bound parsing to 50 bytes even though message buffer has 200 bytes
   fc::bounded_datastream bds(mb_ds, 50);

   char read_buf[50];
   BOOST_CHECK(bds.read(read_buf, 50));
   BOOST_CHECK_EQUAL(bds.remaining(), 0u);

   // Attempting to read more through bds throws even though mb still has 150 bytes
   BOOST_CHECK_THROW(bds.read(read_buf, 1), fc::out_of_range_exception);
   BOOST_CHECK_EQUAL(mb.bytes_to_read(), 150u);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(bounded_datastream_with_mirror) try {
   char buffer[100];
   memset(buffer, 'M', sizeof(buffer));

   fc::datastream<const char*> raw_ds(buffer, sizeof(buffer));
   fc::bounded_datastream bds(raw_ds, 30);
   fc::datastream_mirror mirror_ds(bds, 30);

   char out[20];
   BOOST_CHECK(mirror_ds.read(out, 20));
   BOOST_CHECK_EQUAL(bds.remaining(), 10u);

   // Reading 15 bytes exceeds bds remaining 10: throws
   BOOST_CHECK_THROW(mirror_ds.read(out, 15), fc::out_of_range_exception);

   // Read remaining 10 bytes
   BOOST_CHECK(mirror_ds.read(out, 10));
   BOOST_CHECK_EQUAL(bds.remaining(), 0u);

   auto mirror = mirror_ds.extract_mirror();
   BOOST_CHECK_EQUAL(mirror.size(), 30u);
   BOOST_CHECK_EQUAL(std::string(mirror.data(), mirror.size()), std::string(30, 'M'));
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(bounded_datastream_zero_max_bytes) try {
   char buffer[10] = "123456789";
   fc::datastream<const char*> raw_read(buffer, sizeof(buffer));
   fc::bounded_datastream bds_read(raw_read, 0);

   BOOST_CHECK_EQUAL(bds_read.max_bytes(), 0u);
   BOOST_CHECK_EQUAL(bds_read.remaining(), 0u);
   BOOST_CHECK_EQUAL(bds_read.tellp(), 0u);
   BOOST_CHECK(bds_read.valid());

   char out[10];
   // Reading 0 bytes succeeds
   BOOST_CHECK(bds_read.read(out, 0));
   BOOST_CHECK_EQUAL(bds_read.tellp(), 0u);
   BOOST_CHECK_EQUAL(bds_read.remaining(), 0u);

   // Reading 1 byte throws out_of_range_exception
   BOOST_CHECK_THROW(bds_read.read(out, 1), fc::out_of_range_exception);
   char c = 0;
   BOOST_CHECK_THROW(bds_read.get(c), fc::out_of_range_exception);

   // Skip 0 succeeds, skip 1 throws
   bds_read.skip(0);
   BOOST_CHECK_THROW(bds_read.skip(1), fc::out_of_range_exception);

   // Seekp 0 succeeds, seekp 1 returns false
   BOOST_CHECK(bds_read.seekp(0));
   BOOST_CHECK(!bds_read.seekp(1));

   // Zero-max write stream
   char write_buf[10];
   fc::datastream<char*> raw_write(write_buf, sizeof(write_buf));
   fc::bounded_datastream bds_write(raw_write, 0);

   BOOST_CHECK(bds_write.write("a", 0));
   BOOST_CHECK_THROW(bds_write.write("a", 1), fc::out_of_range_exception);
   BOOST_CHECK_THROW(bds_write.put('a'), fc::out_of_range_exception);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(bounded_datastream_multibyte_boundary_crossing) try {
   char buffer[128];
   fc::datastream<char*> write_raw(buffer, sizeof(buffer));
   uint16_t u16 = 0x1234;
   uint32_t u32 = 0xdeadbeef;
   uint64_t u64 = 0x0123456789abcdefULL;
   double dbl = 3.141592653589793;
   std::vector<uint32_t> vec = {10, 20, 30, 40, 50};

   fc::raw::pack(write_raw, u16);
   fc::raw::pack(write_raw, u32);
   fc::raw::pack(write_raw, u64);
   fc::raw::pack(write_raw, dbl);
   fc::raw::pack(write_raw, vec);

   size_t total_packed = write_raw.tellp();

   // Truncate stream at 1 byte (less than uint16_t size 2)
   {
      fc::datastream<const char*> read_raw(buffer, total_packed);
      fc::bounded_datastream bds(read_raw, 1);
      uint16_t val;
      BOOST_CHECK_THROW(fc::raw::unpack(bds, val), fc::out_of_range_exception);
   }

   // Unpack uint16_t (2 bytes), then attempt uint32_t (4 bytes) with only 3 bytes remaining (total 5)
   {
      fc::datastream<const char*> read_raw(buffer, total_packed);
      fc::bounded_datastream bds(read_raw, 5);
      uint16_t val16;
      fc::raw::unpack(bds, val16);
      BOOST_CHECK_EQUAL(val16, u16);
      BOOST_CHECK_EQUAL(bds.remaining(), 3u);

      uint32_t val32;
      BOOST_CHECK_THROW(fc::raw::unpack(bds, val32), fc::out_of_range_exception);
   }

   // Unpack uint16_t (2), uint32_t (4), then attempt uint64_t (8) with only 7 bytes remaining (total 13)
   {
      fc::datastream<const char*> read_raw(buffer, total_packed);
      fc::bounded_datastream bds(read_raw, 13);
      uint16_t val16;
      uint32_t val32;
      fc::raw::unpack(bds, val16);
      fc::raw::unpack(bds, val32);
      BOOST_CHECK_EQUAL(val16, u16);
      BOOST_CHECK_EQUAL(val32, u32);
      BOOST_CHECK_EQUAL(bds.remaining(), 7u);

      uint64_t val64;
      BOOST_CHECK_THROW(fc::raw::unpack(bds, val64), fc::out_of_range_exception);
   }

   // Unpack up to double (2+4+8+8 = 22 bytes), then unpack vector with 1 byte short of full vector payload
   {
      fc::datastream<const char*> read_raw(buffer, total_packed);
      fc::bounded_datastream bds(read_raw, total_packed - 1);
      uint16_t val16;
      uint32_t val32;
      uint64_t val64;
      double val_dbl;
      fc::raw::unpack(bds, val16);
      fc::raw::unpack(bds, val32);
      fc::raw::unpack(bds, val64);
      fc::raw::unpack(bds, val_dbl);

      std::vector<uint32_t> val_vec;
      BOOST_CHECK_THROW(fc::raw::unpack(bds, val_vec), fc::out_of_range_exception);
   }
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(bounded_datastream_nested_streams) try {
   char buffer[100];
   memset(buffer, 'A', sizeof(buffer));

   // Outer bounded stream (limit 20) wrapping inner bounded stream (limit 10)
   {
      fc::datastream<const char*> raw_ds(buffer, sizeof(buffer));
      fc::bounded_datastream inner_bds(raw_ds, 10);
      fc::bounded_datastream outer_bds(inner_bds, 20);

      char out[30];
      // Can read up to inner bound 10
      BOOST_CHECK(outer_bds.read(out, 10));
      // Attempting 11th byte throws at inner_bds
      BOOST_CHECK_THROW(outer_bds.read(out, 1), fc::out_of_range_exception);
   }

   // Outer bounded stream (limit 10) wrapping inner bounded stream (limit 20)
   {
      fc::datastream<const char*> raw_ds(buffer, sizeof(buffer));
      fc::bounded_datastream inner_bds(raw_ds, 20);
      fc::bounded_datastream outer_bds(inner_bds, 10);

      char out[30];
      // Can read up to outer bound 10
      BOOST_CHECK(outer_bds.read(out, 10));
      // Attempting 11th byte throws at outer_bds
      BOOST_CHECK_THROW(outer_bds.read(out, 1), fc::out_of_range_exception);
   }
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(bounded_datastream_message_buffer_ring_wrap) try {
   fc::message_buffer<32> mb;

   // Fill 24 bytes
   std::string s1(24, 'X');
   memcpy(mb.write_ptr(), s1.data(), 24);
   mb.advance_write_ptr(24);

   // Consume 20 bytes
   mb.advance_read_ptr(20);
   BOOST_CHECK_EQUAL(mb.bytes_to_read(), 4u);

   // Write 20 bytes spanning chunks in message_buffer
   std::string s2(20, 'Y');
   mb.add_space(20);
   auto space = mb.get_buffer_sequence_for_boost_async_read();
   size_t copied = 0;
   for (auto b : space) {
      size_t chunk = std::min(b.size(), s2.size() - copied);
      memcpy(b.data(), s2.data() + copied, chunk);
      copied += chunk;
      if (copied == s2.size()) break;
   }
   mb.advance_write_ptr(20);
   BOOST_CHECK_EQUAL(mb.bytes_to_read(), 24u); // 4 'X' + 20 'Y'

   // Create datastream and bounded datastream over wrapped message_buffer
   auto mb_ds = mb.create_datastream();
   fc::bounded_datastream bds(mb_ds, 24);

   // Read 10 bytes crossing circular wrap
   char read_out[24];
   BOOST_CHECK(bds.read(read_out, 10));
   BOOST_CHECK_EQUAL(std::string(read_out, 4), "XXXX");
   BOOST_CHECK_EQUAL(std::string(read_out + 4, 6), "YYYYYY");
   BOOST_CHECK_EQUAL(bds.remaining(), 14u);

   // Read remaining 14 bytes
   BOOST_CHECK(bds.read(read_out + 10, 14));
   BOOST_CHECK_EQUAL(std::string(read_out + 10, 14), std::string(14, 'Y'));
   BOOST_CHECK_EQUAL(bds.remaining(), 0u);

   // Reading further throws
   BOOST_CHECK_THROW(bds.read(read_out, 1), fc::out_of_range_exception);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_SUITE_END()

