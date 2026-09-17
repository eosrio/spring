#pragma once

#include <fc/io/datastream.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/raw_fwd.hpp>

#include <cstdint>
#include <type_traits>
#include <utility>

namespace fc { namespace raw { namespace detail {

   /**
    * True when Stream::remaining() reports a trustworthy byte count of unread
    * payload (in-memory datastreams and bounded_datastream). Skips:
    *  - datastream<size_t> (size-calculation stream; remaining() is always 0)
    *  - streams whose remaining() is bool (streambuf in_avail() wrapper)
    *  - streams with no remaining() (cfile, etc.)
    *
    * This is a defensive bound only: valid payloads still unpack unchanged.
    */
   template<typename Stream>
   inline constexpr bool stream_has_trusted_remaining() {
      using S = std::remove_cvref_t<Stream>;
      if constexpr (std::is_same_v<S, datastream<size_t>>) {
         return false;
      } else if constexpr (requires(const S& s) { s.remaining(); }) {
         using rem_t = std::remove_cvref_t<decltype(std::declval<const S&>().remaining())>;
         return std::is_integral_v<rem_t> && !std::is_same_v<rem_t, bool>;
      } else {
         return false;
      }
   }

   template<typename T>
   inline uint64_t default_instance_packed_size() {
      if constexpr (!std::is_default_constructible_v<T>) {
         return 0;
      } else {
         datastream<size_t> ps;
         // Default-initialize (T dummy;), not T dummy{}. Copy-list-initialization
         // cannot invoke explicit default constructors such as
         // chainbase::shared_cow_vector().
         T dummy;
         fc::raw::pack(ps, dummy);
         return static_cast<uint64_t>(ps.tellp());
      }
   }

   /**
    * Fail before resize/reserve when a claimed element count cannot fit in the
    * remaining stream even if every element serializes at its minimum size
    * (default-constructed T). Prevents allocation amplification from a short
    * frame that advertises MAX_NUM_ARRAY_ELEMENTS.
    *
    * Types whose default instance packs to 0 bytes (empty structs) skip the
    * check so valid zero-payload vectors are not rejected.
    */
   template<typename Stream, typename T>
   inline void assert_claimed_container_fits(Stream& s, uint64_t count) {
      if (count == 0)
         return;
      if constexpr (stream_has_trusted_remaining<Stream>()) {
         const uint64_t rem      = static_cast<uint64_t>(s.remaining());
         const uint64_t min_elem = default_instance_packed_size<T>();
         if (min_elem == 0)
            return;
         FC_ASSERT(count <= rem / min_elem,
                   "claimed container size ${c} exceeds remaining stream (${r} bytes, min ${m} per element)",
                   ("c", count)("r", rem)("m", min_elem));
      }
   }

}}} // namespace fc::raw::detail
