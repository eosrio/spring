#pragma once

#include <eosio/chain/exceptions.hpp>

#include <cstddef>
#include <deque>

namespace eosio::state_history {

FC_DECLARE_DERIVED_EXCEPTION( status_request_queue_limit_exceeded, chain::plugin_exception,
                              3240001, "State history status request queue limit exceeded" );

class status_request_queue {
public:
   static constexpr size_t default_max_size = 100;

   explicit status_request_queue(size_t max_size = default_max_size) : _max_size(max_size) {}

   bool try_append(bool is_v1) {
      if (_queue.size() >= _max_size) {
         return false;
      }
      _queue.emplace_back(is_v1);
      return true;
   }

   std::deque<bool> extract() {
      std::deque<bool> extracted;
      _queue.swap(extracted);
      return extracted;
   }

   size_t size() const { return _queue.size(); }
   bool empty() const { return _queue.empty(); }
   size_t max_size() const { return _max_size; }
   void clear() { _queue.clear(); }

private:
   std::deque<bool> _queue;
   size_t _max_size;
};

} // namespace eosio::state_history
