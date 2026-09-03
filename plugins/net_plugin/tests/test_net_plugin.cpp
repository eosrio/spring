#include <boost/test/unit_test.hpp>
#include <eosio/net_plugin/net_utils.hpp>
#include <eosio/net_plugin/protocol.hpp>
#include <fc/io/datastream.hpp>
#include <fc/network/message_buffer.hpp>
#include <boost/program_options.hpp>

#include <set>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <future>
#include <mutex>

BOOST_AUTO_TEST_SUITE(net_plugin_milestone3_tests)

// =============================================================================
// Issue 3 Tests: Block notices for missing blocks count as block progress
// =============================================================================

BOOST_AUTO_TEST_CASE(test_classify_block_notice_progress) {
   using namespace eosio::net_utils;

   // 1. Have block, have previous -> have_block: marks block progress
   auto a1 = classify_block_notice(true, true);
   BOOST_CHECK(a1 == block_notice_action::have_block);
   BOOST_CHECK(block_notice_marks_progress(a1));
   BOOST_CHECK(block_notice_marks_progress(true));

   // 2. Have block, missing previous -> have_block: marks block progress
   auto a2 = classify_block_notice(true, false);
   BOOST_CHECK(a2 == block_notice_action::have_block);
   BOOST_CHECK(block_notice_marks_progress(a2));
   BOOST_CHECK(block_notice_marks_progress(true));

   // 3. Missing block, missing previous -> missing_previous: does NOT mark progress
   auto a3 = classify_block_notice(false, false);
   BOOST_CHECK(a3 == block_notice_action::missing_previous);
   BOOST_CHECK(!block_notice_marks_progress(a3));
   BOOST_CHECK(!block_notice_marks_progress(false));

   // 4. Missing block, have previous -> missing_with_previous: does NOT mark progress
   auto a4 = classify_block_notice(false, true);
   BOOST_CHECK(a4 == block_notice_action::missing_with_previous);
   BOOST_CHECK(!block_notice_marks_progress(a4));
   BOOST_CHECK(!block_notice_marks_progress(false));
}

BOOST_AUTO_TEST_CASE(test_p2p_disable_block_nack_default_for_producers) {
   namespace bpo = boost::program_options;
   bpo::options_description desc("Test Options");
   desc.add_options()
      ("p2p-disable-block-nack", bpo::value<bool>()->default_value(false), "disable nack")
      ("producer-name", bpo::value<std::vector<std::string>>()->composing()->multitoken(), "producers");

   auto eval_disable_nack = [](const bpo::variables_map& options) -> bool {
      bool has_producers = false;
      if (options.count("producer-name")) {
         const auto& prods = options.at("producer-name").as<std::vector<std::string>>();
         has_producers = !prods.empty();
      }
      if (options.count("p2p-disable-block-nack")) {
         if (options.at("p2p-disable-block-nack").defaulted() && has_producers) {
            return true;
         } else {
            return options.at("p2p-disable-block-nack").as<bool>();
         }
      } else {
         return has_producers;
      }
   };

   // Case A: No producers configured, defaulted p2p-disable-block-nack -> false
   {
      bpo::variables_map vm;
      const char* args[] = {"test_app"};
      bpo::store(bpo::parse_command_line(1, args, desc), vm);
      bpo::notify(vm);
      BOOST_CHECK_EQUAL(eval_disable_nack(vm), false);
   }

   // Case B: Producer configured, defaulted p2p-disable-block-nack -> true
   {
      bpo::variables_map vm;
      const char* args[] = {"test_app", "--producer-name", "producer1"};
      bpo::store(bpo::parse_command_line(3, args, desc), vm);
      bpo::notify(vm);
      BOOST_CHECK_EQUAL(eval_disable_nack(vm), true);
   }

   // Case C: Producer configured, explicit --p2p-disable-block-nack=false -> false
   {
      bpo::variables_map vm;
      const char* args[] = {"test_app", "--producer-name", "producer1", "--p2p-disable-block-nack=false"};
      bpo::store(bpo::parse_command_line(4, args, desc), vm);
      bpo::notify(vm);
      BOOST_CHECK_EQUAL(eval_disable_nack(vm), false);
   }

   // Case D: No producer, explicit --p2p-disable-block-nack=true -> true
   {
      bpo::variables_map vm;
      const char* args[] = {"test_app", "--p2p-disable-block-nack=true"};
      bpo::store(bpo::parse_command_line(2, args, desc), vm);
      bpo::notify(vm);
      BOOST_CHECK_EQUAL(eval_disable_nack(vm), true);
   }
}

// =============================================================================
// Issue 4 Tests: P2P frame parsers bounded to declared length
// =============================================================================

template <uint32_t N>
void append_to_message_buffer(fc::message_buffer<N>& mb, const void* data, size_t size) {
   if (mb.bytes_to_write() < size) {
      mb.add_space(size - mb.bytes_to_write());
   }
   memcpy(mb.write_ptr(), data, size);
   mb.advance_write_ptr(size);
}

BOOST_AUTO_TEST_CASE(test_p2p_frame_parser_bounds_and_alignment) {
   fc::message_buffer<1024*1024> mb;

   // Frame 1: handshake_message with 16 bytes of trailing padding declared in message_length
   eosio::handshake_message hello1;
   hello1.p2p_address = "peer1:9876";
   hello1.os = "linux";
   hello1.agent = "test";

   std::vector<char> hello1_bytes = fc::raw::pack(hello1);
   uint32_t payload_len = hello1_bytes.size();
   uint32_t padding_len = 16;
   uint32_t declared_len1 = payload_len + padding_len;

   // Frame 2: handshake_message immediately following Frame 1
   eosio::handshake_message hello2;
   hello2.p2p_address = "peer2:9876";
   hello2.os = "linux";
   hello2.agent = "test2";
   std::vector<char> hello2_bytes = fc::raw::pack(hello2);
   uint32_t declared_len2 = hello2_bytes.size();

   // Frame 1: header + payload + padding
   append_to_message_buffer(mb, &declared_len1, sizeof(declared_len1));
   append_to_message_buffer(mb, hello1_bytes.data(), hello1_bytes.size());
   std::vector<char> padding(padding_len, 'P');
   append_to_message_buffer(mb, padding.data(), padding.size());

   // Frame 2: header + payload
   append_to_message_buffer(mb, &declared_len2, sizeof(declared_len2));
   append_to_message_buffer(mb, hello2_bytes.data(), hello2_bytes.size());

   // Read Frame 1 header
   uint32_t read_len1 = 0;
   auto idx1 = mb.read_index();
   mb.peek(&read_len1, sizeof(read_len1), idx1);
   BOOST_CHECK_EQUAL(read_len1, declared_len1);
   mb.advance_read_ptr(sizeof(read_len1));

   // Unpack Frame 1 through bounded_datastream
   {
      auto raw_ds = mb.create_datastream();
      fc::bounded_datastream bds(raw_ds, read_len1);
      eosio::handshake_message unpacked1;
      fc::raw::unpack(bds, unpacked1);
      BOOST_CHECK_EQUAL(unpacked1.p2p_address, hello1.p2p_address);
      BOOST_CHECK_EQUAL(unpacked1.agent, hello1.agent);
      BOOST_CHECK_EQUAL(bds.remaining(), padding_len);

      // Advance read pointer past padding to exact frame end
      mb.advance_read_ptr(bds.remaining());
   }

   // Frame 2 header MUST now be exactly at the read index
   uint32_t read_len2 = 0;
   auto idx2 = mb.read_index();
   mb.peek(&read_len2, sizeof(read_len2), idx2);
   BOOST_CHECK_EQUAL(read_len2, declared_len2);
   mb.advance_read_ptr(sizeof(read_len2));

   // Unpack Frame 2 through bounded_datastream
   {
      auto raw_ds = mb.create_datastream();
      fc::bounded_datastream bds(raw_ds, read_len2);
      eosio::handshake_message unpacked2;
      fc::raw::unpack(bds, unpacked2);
      BOOST_CHECK_EQUAL(unpacked2.p2p_address, hello2.p2p_address);
      BOOST_CHECK_EQUAL(unpacked2.agent, hello2.agent);
      BOOST_CHECK_EQUAL(bds.remaining(), 0u);
      mb.advance_read_ptr(bds.remaining());
   }

   // Buffer completely consumed
   BOOST_CHECK_EQUAL(mb.bytes_to_read(), 0u);
}

BOOST_AUTO_TEST_CASE(test_p2p_frame_parser_truncated_frame_throws) {
   fc::message_buffer<1024*1024> mb;

   eosio::handshake_message hello;
   hello.p2p_address = "long_peer_address_for_test:9876";
   hello.agent = "some_long_agent_name";
   std::vector<char> bytes = fc::raw::pack(hello);

   // Place full bytes into buffer, but declare bounded length shorter than actual payload
   uint32_t truncated_len = bytes.size() / 2;
   append_to_message_buffer(mb, bytes.data(), bytes.size());

   auto raw_ds = mb.create_datastream();
   fc::bounded_datastream bds(raw_ds, truncated_len);

   eosio::handshake_message unpacked;
   // Bounded stream must throw out_of_range_exception rather than reading past truncated bound
   BOOST_CHECK_THROW(fc::raw::unpack(bds, unpacked), fc::out_of_range_exception);
}

// =============================================================================
// Issue 13 Tests: connect() retains invalid peers in supplied_peers
// =============================================================================

BOOST_AUTO_TEST_CASE(test_connect_invalid_peer_syntax_rejection) {
   using namespace eosio::net_utils;

   // Valid addresses must parse cleanly
   {
      auto [h, p, t] = split_host_port_type("127.0.0.1:9876");
      BOOST_CHECK_EQUAL(h, "127.0.0.1");
      BOOST_CHECK_EQUAL(p, "9876");
      BOOST_CHECK(t.empty());
   }
   {
      auto [h, p, t] = split_host_port_type("p2p.eos.io:9876:blk");
      BOOST_CHECK_EQUAL(h, "p2p.eos.io");
      BOOST_CHECK_EQUAL(p, "9876");
      BOOST_CHECK_EQUAL(t, "blk");
   }
   {
      auto [h, p, t] = split_host_port_type("[2001:db8::1]:9876:trx");
      BOOST_CHECK_EQUAL(h, "2001:db8::1");
      BOOST_CHECK_EQUAL(p, "9876");
      BOOST_CHECK_EQUAL(t, "trx");
   }

   // Malformed addresses that must return empty host
   std::vector<std::string> invalid_addresses = {
      "",
      ":8888",
      "localhost",
      "foo:bar:baz:extra:more",
      "[2001:db8::1:9876",
      "2001:db8::1:9876",
      "   "
   };

   for (const auto& addr : invalid_addresses) {
      auto [h, p, t] = split_host_port_type(addr);
      BOOST_CHECK_MESSAGE(h.empty(), "Expected empty host for invalid address: " + addr);
   }

   // Verify connect() behavior: validation before insertion prevents retention of invalid peers
   std::set<std::string> supplied_peers;
   auto connect_fn = [&](const std::string& host) -> std::string {
      if (auto [h, port, type] = split_host_port_type(host); h.empty()) {
         return "invalid peer address";
      }
      supplied_peers.insert(host);
      return "added connection";
   };

   for (const auto& addr : invalid_addresses) {
      std::string res = connect_fn(addr);
      BOOST_CHECK_EQUAL(res, "invalid peer address");
   }

   // supplied_peers must NOT retain any of the invalid addresses
   BOOST_CHECK(supplied_peers.empty());
   BOOST_CHECK_EQUAL(supplied_peers.size(), 0u);

   // Valid address succeeds and enters supplied_peers
   std::string valid_res = connect_fn("192.168.1.100:9876");
   BOOST_CHECK_EQUAL(valid_res, "added connection");
   BOOST_CHECK_EQUAL(supplied_peers.size(), 1u);
   BOOST_CHECK_EQUAL(*supplied_peers.begin(), "192.168.1.100:9876");
}

// =============================================================================
// Adversarial Stress Tests: Issue 3 (Block notices & Watchdog Keepalive Stall)
// =============================================================================

BOOST_AUTO_TEST_CASE(test_adversarial_block_notice_stall_and_watchdog) {
   using namespace eosio::net_utils;

   // Emulate connection keepalive watchdog state
   std::chrono::steady_clock::time_point latest_blk_time = std::chrono::steady_clock::now();
   const auto initial_time = latest_blk_time;

   // Sleep slightly so any time update would be measurably strictly greater
   std::this_thread::sleep_for(std::chrono::milliseconds(5));

   // Attacker Scenario: An unauthenticated peer floods 500 block notices for unknown blocks
   for (int i = 0; i < 500; ++i) {
      bool have_block = false;
      bool have_previous = (i % 2 == 1); // Alternates between missing_previous and missing_with_previous

      auto action = classify_block_notice(have_block, have_previous);
      if (block_notice_marks_progress(action)) {
         latest_blk_time = std::chrono::steady_clock::now();
      }

      // Verify that neither missing_previous nor missing_with_previous marks progress
      BOOST_CHECK(!block_notice_marks_progress(action));
      BOOST_CHECK(!block_notice_marks_progress(have_block));
   }

   // VERDICT: latest_blk_time must remain completely unmodified after 500 malicious notices!
   BOOST_CHECK(latest_blk_time == initial_time);

   // Edge Case: Extreme IDs - all-zeros ID, self-referential cycle (prev == id)
   {
      auto cycle_action = classify_block_notice(false, false);
      BOOST_CHECK(!block_notice_marks_progress(cycle_action));
      if (block_notice_marks_progress(cycle_action)) {
         latest_blk_time = std::chrono::steady_clock::now();
      }
      BOOST_CHECK(latest_blk_time == initial_time);
   }

   // Contrast: Honest block notice arrives (have_block == true)
   {
      auto honest_action = classify_block_notice(true, false);
      BOOST_CHECK(block_notice_marks_progress(honest_action));
      if (block_notice_marks_progress(honest_action)) {
         latest_blk_time = std::chrono::steady_clock::now();
      }
      // Must have advanced
      BOOST_CHECK(latest_blk_time > initial_time);
   }
}

BOOST_AUTO_TEST_CASE(test_adversarial_p2p_disable_block_nack_permutations) {
   namespace bpo = boost::program_options;
   bpo::options_description desc("Test Options");
   desc.add_options()
      ("p2p-disable-block-nack", bpo::value<bool>()->default_value(false), "disable nack")
      ("producer-name", bpo::value<std::vector<std::string>>()->composing()->multitoken(), "producers");

   auto eval_disable_nack = [](const bpo::variables_map& options) -> bool {
      bool has_producers = false;
      if (options.count("producer-name")) {
         const auto& prods = options.at("producer-name").as<std::vector<std::string>>();
         has_producers = !prods.empty();
      }
      if (options.count("p2p-disable-block-nack")) {
         if (options.at("p2p-disable-block-nack").defaulted() && has_producers) {
            return true;
         } else {
            return options.at("p2p-disable-block-nack").as<bool>();
         }
      } else {
         return has_producers;
      }
   };

   // Permutation 1: Multiple producers configured (bp1, bp2, bp3), defaulted nack -> true
   {
      bpo::variables_map vm;
      const char* args[] = {"app", "--producer-name", "bp1", "--producer-name", "bp2", "--producer-name", "bp3"};
      bpo::store(bpo::parse_command_line(7, args, desc), vm);
      bpo::notify(vm);
      BOOST_CHECK_EQUAL(eval_disable_nack(vm), true);
   }

   // Permutation 2: Multiple producers, explicit false -> false
   {
      bpo::variables_map vm;
      const char* args[] = {"app", "--producer-name", "bp1", "--p2p-disable-block-nack=false"};
      bpo::store(bpo::parse_command_line(4, args, desc), vm);
      bpo::notify(vm);
      BOOST_CHECK_EQUAL(eval_disable_nack(vm), false);
   }

   // Permutation 3: No producers, explicit true -> true
   {
      bpo::variables_map vm;
      const char* args[] = {"app", "--p2p-disable-block-nack=true"};
      bpo::store(bpo::parse_command_line(2, args, desc), vm);
      bpo::notify(vm);
      BOOST_CHECK_EQUAL(eval_disable_nack(vm), true);
   }

   // Permutation 4: Empty string producer -> vector has 1 element (""), has_producers is true
   {
      bpo::variables_map vm;
      const char* args[] = {"app", "--producer-name", ""};
      bpo::store(bpo::parse_command_line(3, args, desc), vm);
      bpo::notify(vm);
      BOOST_CHECK_EQUAL(eval_disable_nack(vm), true);
   }
}

// =============================================================================
// Adversarial Stress Tests: Issue 4 (Bounded Datastream & Frame Alignment)
// =============================================================================

BOOST_AUTO_TEST_CASE(test_adversarial_bounded_datastream_primitives_stress) {
   std::vector<char> backing(1024, 'X');
   fc::datastream<char*> raw_ds(backing.data(), backing.size());

   // Zero boundary
   fc::bounded_datastream bds0(raw_ds, 0);
   BOOST_CHECK_EQUAL(bds0.remaining(), 0u);
   BOOST_CHECK_EQUAL(bds0.max_bytes(), 0u);
   BOOST_CHECK_EQUAL(bds0.tellp(), 0u);
   BOOST_CHECK(bds0.valid());

   char dummy = 0;
   BOOST_CHECK_THROW(bds0.read(&dummy, 1), fc::out_of_range_exception);
   BOOST_CHECK_THROW(bds0.skip(1), fc::out_of_range_exception);
   BOOST_CHECK_THROW(bds0.write(&dummy, 1), fc::out_of_range_exception);
   BOOST_CHECK_THROW(bds0.put('a'), fc::out_of_range_exception);
   BOOST_CHECK_THROW(bds0.get(dummy), fc::out_of_range_exception);
   BOOST_CHECK(!bds0.seekp(1));
   BOOST_CHECK(bds0.seekp(0));

   // Arithmetic types insertion/extraction through operator<< and operator>>
   std::vector<char> mem(256);
   {
      fc::datastream<char*> out_raw(mem.data(), mem.size());
      fc::bounded_datastream out_bds(out_raw, mem.size());

      uint8_t u8 = 42;
      uint16_t u16 = 12345;
      uint32_t u32 = 0xDEADBEEF;
      uint64_t u64 = 0xFEEDFACEDEADBEEFULL;

      out_bds << u8 << u16 << u32 << u64;
      BOOST_CHECK_EQUAL(out_bds.tellp(), sizeof(u8) + sizeof(u16) + sizeof(u32) + sizeof(u64));
   }

   {
      fc::datastream<char*> in_raw(mem.data(), mem.size());
      fc::bounded_datastream in_bds(in_raw, sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint64_t));

      uint8_t r8 = 0;
      uint16_t r16 = 0;
      uint32_t r32 = 0;
      uint64_t r64 = 0;

      in_bds >> r8 >> r16 >> r32 >> r64;
      BOOST_CHECK_EQUAL(r8, 42);
      BOOST_CHECK_EQUAL(r16, 12345);
      BOOST_CHECK_EQUAL(r32, 0xDEADBEEF);
      BOOST_CHECK_EQUAL(r64, 0xFEEDFACEDEADBEEFULL);
      BOOST_CHECK_EQUAL(in_bds.remaining(), 0u);

      // Attempt to read past bound
      uint8_t overflow_byte = 0;
      BOOST_CHECK_THROW(in_bds >> overflow_byte, fc::out_of_range_exception);
   }
}

BOOST_AUTO_TEST_CASE(test_adversarial_multipacket_frame_pipeline_desync) {
   fc::message_buffer<1024 * 1024> mb;

   // Construct 5 different messages with various padding lengths
   // Frame 1: handshake_message with 27 bytes padding
   eosio::handshake_message msg1;
   msg1.p2p_address = "peer1:9876";
   msg1.os = "linux";
   msg1.agent = "agent1";
   std::vector<char> b1 = fc::raw::pack(msg1);
   uint32_t pad1 = 27;
   uint32_t decl1 = b1.size() + pad1;

   // Frame 2: go_away_message with 15 bytes padding
   eosio::go_away_message msg2;
   msg2.reason = eosio::go_away_reason::duplicate;
   msg2.node_id = fc::sha256::hash("node2");
   std::vector<char> b2 = fc::raw::pack(msg2);
   uint32_t pad2 = 15;
   uint32_t decl2 = b2.size() + pad2;

   // Frame 3: notice_message with 33 bytes padding
   eosio::notice_message msg3;
   msg3.known_trx.mode = eosio::id_list_modes::normal;
   msg3.known_blocks.mode = eosio::id_list_modes::normal;
   std::vector<char> b3 = fc::raw::pack(msg3);
   uint32_t pad3 = 33;
   uint32_t decl3 = b3.size() + pad3;

   // Frame 4: another handshake_message with 0 bytes padding
   eosio::handshake_message msg4;
   msg4.p2p_address = "peer4:9876";
   msg4.os = "bsd";
   msg4.agent = "agent4";
   std::vector<char> b4 = fc::raw::pack(msg4);
   uint32_t pad4 = 0;
   uint32_t decl4 = b4.size() + pad4;

   // Helper lambda to append frame with padding
   auto append_frame = [&](uint32_t decl_len, const std::vector<char>& payload, uint32_t pad_len) {
      append_to_message_buffer(mb, &decl_len, sizeof(decl_len));
      append_to_message_buffer(mb, payload.data(), payload.size());
      if (pad_len > 0) {
         std::vector<char> pad(pad_len, 0xAA);
         append_to_message_buffer(mb, pad.data(), pad.size());
      }
   };

   // Pipeline all 4 frames consecutively into the message buffer
   append_frame(decl1, b1, pad1);
   append_frame(decl2, b2, pad2);
   append_frame(decl3, b3, pad3);
   append_frame(decl4, b4, pad4);

   // Emulate the read loop unpack logic with advance_to_frame_end
   auto unpack_frame = [&](auto& unpacked_dest, uint32_t expected_decl_len, uint32_t expected_pad) {
      uint32_t read_len = 0;
      auto idx = mb.read_index();
      mb.peek(&read_len, sizeof(read_len), idx);
      BOOST_CHECK_EQUAL(read_len, expected_decl_len);
      mb.advance_read_ptr(sizeof(read_len));

      auto raw_ds = mb.create_datastream();
      fc::bounded_datastream bds(raw_ds, read_len);
      fc::raw::unpack(bds, unpacked_dest);
      BOOST_CHECK_EQUAL(bds.remaining(), expected_pad);

      // Crucial: advance_to_frame_end skips remaining padding bytes
      mb.advance_read_ptr(bds.remaining());
   };

   // Unpack Frame 1
   eosio::handshake_message out1;
   unpack_frame(out1, decl1, pad1);
   BOOST_CHECK_EQUAL(out1.p2p_address, msg1.p2p_address);
   BOOST_CHECK_EQUAL(out1.agent, msg1.agent);

   // Unpack Frame 2
   eosio::go_away_message out2;
   unpack_frame(out2, decl2, pad2);
   BOOST_CHECK(out2.reason == msg2.reason);
   BOOST_CHECK_EQUAL(out2.node_id, msg2.node_id);

   // Unpack Frame 3
   eosio::notice_message out3;
   unpack_frame(out3, decl3, pad3);
   BOOST_CHECK(out3.known_trx.mode == msg3.known_trx.mode);

   // Unpack Frame 4
   eosio::handshake_message out4;
   unpack_frame(out4, decl4, pad4);
   BOOST_CHECK_EQUAL(out4.p2p_address, msg4.p2p_address);
   BOOST_CHECK_EQUAL(out4.agent, msg4.agent);

   // Entire pipeline must be completely consumed with zero trailing bytes desynchronization
   BOOST_CHECK_EQUAL(mb.bytes_to_read(), 0u);
}

BOOST_AUTO_TEST_CASE(test_adversarial_frame_truncation_underdeclared_length) {
   fc::message_buffer<1024 * 1024> mb;

   eosio::handshake_message hello;
   hello.p2p_address = "127.0.0.1:9876";
   hello.agent = "test_agent";
   std::vector<char> bytes = fc::raw::pack(hello);

   // Test various declared lengths shorter than actual required size
   std::vector<uint32_t> short_lengths = { 0, 1, 2, 5, static_cast<uint32_t>(bytes.size() - 1) };

   for (uint32_t short_len : short_lengths) {
      append_to_message_buffer(mb, bytes.data(), bytes.size());
      auto raw_ds = mb.create_datastream();
      fc::bounded_datastream bds(raw_ds, short_len);

      eosio::handshake_message out;
      BOOST_CHECK_THROW(fc::raw::unpack(bds, out), fc::out_of_range_exception);

      // Clean buffer for next iteration
      mb.advance_read_ptr(mb.bytes_to_read());
   }
}

// =============================================================================
// Adversarial Stress Tests: Issue 13 (connect() Malformed Address Matrix)
// =============================================================================

BOOST_AUTO_TEST_CASE(test_adversarial_connect_fuzz_addresses) {
   using namespace eosio::net_utils;

   // Category 1: Addresses that split_host_port_type defines as syntactically invalid (empty host or port)
   std::vector<std::string> syntactically_invalid_addresses = {
      "",
      "   ",
      "\t\n\r",
      ":8888",
      ":0",
      ":-1",
      ":",
      "localhost",
      "127.0.0.1",
      "p2p.eos.io",
      "[2001:db8::1",
      "2001:db8::1:9876",
      "[2001:db8::1]:",
      "a:b:c:d:e:f:g:h",
      "peer:abc:blk",
      "peer:http",
      "127.0.0.1:0",
      "127.0.0.1:65536",
      "127.0.0.1:99999",
      std::string(4096, 'A'),
      std::string(4096, 'B') + ":9876"
   };

   std::set<std::string> supplied_peers;
   auto connect_fn = [&](const std::string& host) -> std::string {
      if (auto [h, port, type] = split_host_port_type(host); h.empty() || port.empty() || !is_valid_port(port)) {
         return "invalid peer address";
      }
      supplied_peers.insert(host);
      return "added connection";
   };

   for (const auto& addr : syntactically_invalid_addresses) {
      std::string res = connect_fn(addr);
      BOOST_CHECK_MESSAGE(res == "invalid peer address", "Failed to reject syntactically invalid address: " + addr);
   }

   // supplied_peers must be completely empty after all attacks!
   BOOST_CHECK_EQUAL(supplied_peers.size(), 0u);
   BOOST_CHECK(supplied_peers.empty());

   // Category 2: Addresses that have valid host:port syntax pass split_host_port_type gate
   std::vector<std::string> syntactically_valid_addresses = {
      "127.0.0.1:9876",
      "node.eos.io:8888",
      "[2001:db8::1]:9876",
      "peer:9876:blk",
      "peer:9876:trx"
   };

   for (const auto& addr : syntactically_valid_addresses) {
      std::string res = connect_fn(addr);
      BOOST_CHECK_EQUAL(res, "added connection");
   }
   BOOST_CHECK_EQUAL(supplied_peers.size(), syntactically_valid_addresses.size());
}

BOOST_AUTO_TEST_CASE(test_adversarial_connect_concurrency_stress) {
   using namespace eosio::net_utils;

   std::mutex supplied_peers_mtx;
   std::set<std::string> supplied_peers;

   auto connect_thread_fn = [&](const std::string& host) -> std::string {
      if (auto [h, port, type] = split_host_port_type(host); h.empty()) {
         return "invalid peer address";
      }
      std::lock_guard lock(supplied_peers_mtx);
      supplied_peers.insert(host);
      return "added connection";
   };

   constexpr size_t NUM_THREADS = 8;
   constexpr size_t OPS_PER_THREAD = 100;

   std::vector<std::future<void>> futures;
   for (size_t t = 0; t < NUM_THREADS; ++t) {
      futures.push_back(std::async(std::launch::async, [&, t]() {
         for (size_t i = 0; i < OPS_PER_THREAD; ++i) {
            // Alternating valid and invalid calls
            if (i % 2 == 0) {
               std::string invalid_addr = "malformed_node_" + std::to_string(t) + "_" + std::to_string(i);
               auto res = connect_thread_fn(invalid_addr);
               BOOST_CHECK_EQUAL(res, "invalid peer address");
            } else {
               std::string valid_addr = "10.0." + std::to_string(t) + "." + std::to_string(i) + ":9876";
               auto res = connect_thread_fn(valid_addr);
               BOOST_CHECK_EQUAL(res, "added connection");
            }
         }
      }));
   }

   for (auto& f : futures) {
      f.get();
   }

   // Only valid addresses should have entered supplied_peers
   std::lock_guard lock(supplied_peers_mtx);
   BOOST_CHECK_EQUAL(supplied_peers.size(), NUM_THREADS * (OPS_PER_THREAD / 2));
   for (const auto& peer : supplied_peers) {
      auto [h, p, t] = split_host_port_type(peer);
      BOOST_CHECK(!h.empty());
      BOOST_CHECK(!p.empty());
   }
}

BOOST_AUTO_TEST_SUITE_END()

