#pragma once

#include <eosio/stream.hpp>
#include <optional>
#include <variant>

namespace eosio {

template <typename S>
result<void> varuint32_from_bin(uint32_t& dest, S& stream) {
   dest          = 0;
   int     shift = 0;
   uint8_t b     = 0;
   do {
      if (shift >= 35)
         return stream_error::invalid_varuint_encoding;
      auto r = from_bin(b, stream);
      if (!r)
         return r;
      dest |= uint32_t(b & 0x7f) << shift;
      shift += 7;
   } while (b & 0x80);
   return outcome::success();
}

template <typename S>
result<void> varuint64_from_bin(uint64_t& dest, S& stream) {
   dest          = 0;
   int     shift = 0;
   uint8_t b     = 0;
   do {
      if (shift >= 70)
         return stream_error::invalid_varuint_encoding;
      auto r = from_bin(b, stream);
      if (!r)
         return r;
      dest |= uint64_t(b & 0x7f) << shift;
      shift += 7;
   } while (b & 0x80);
   return outcome::success();
}

template <typename S>
result<void> varint32_from_bin(int32_t& result, S& stream) {
   uint32_t v;
   auto     r = varuint32_from_bin(v, stream);
   if (!r)
      return r;
   if (v & 1)
      result = ((~v) >> 1) | 0x8000'0000;
   else
      result = v >> 1;
   return outcome::success();
}

template <typename T, typename S>
result<void> from_bin(std::vector<T>& v, S& stream) {
   if constexpr (std::is_arithmetic_v<T>) {
      if constexpr (sizeof(size_t) >= 8) {
         uint64_t size;
         auto     r = varuint64_from_bin(size, stream);
         if (!r)
            return r;
         r = stream.check_available(v.size() * sizeof(T));
         if (!r)
            return r;
         v.resize(size);
         return stream.read((char*)v.data(), v.size() * sizeof(T));
      } else {
         uint32_t size;
         auto     r = varuint32_from_bin(size, stream);
         if (!r)
            return r;
         r = stream.check_available(v.size() * sizeof(T));
         if (!r)
            return r;
         v.resize(size);
         return stream.read((char*)v.data(), v.size() * sizeof(T));
      }
   } else {
      uint32_t size;
      auto     r = varuint32_from_bin(size, stream);
      if (!r)
         return r;
      v.resize(size);
      for (size_t i = 0; i < size; ++i) {
         r = from_bin(v[i], stream);
         if (!r)
            return r;
      }
   }
   return outcome::success();
}

template <typename First, typename Second, typename S>
result<void> from_bin(std::pair<First, Second>& obj, S& stream) {
   auto r = from_bin(obj.first, stream);
   if (!r)
      return r;
   return from_bin(obj.second, stream);
}

template <typename S>
inline result<void> from_bin(std::string& obj, S& stream) {
   uint32_t size;
   auto     r = varuint32_from_bin(size, stream);
   if (!r)
      return r;
   obj.resize(size);
   return stream.read(obj.data(), obj.size());
}

template <typename T, typename S>
result<void> from_bin(std::optional<T>& obj, S& stream) {
   bool present;
   auto r = from_bin(present, stream);
   if (!r)
      return r;
   if (!present) {
      obj.reset();
      return outcome::success();
   }
   obj.emplace();
   return from_bin(*obj, stream);
}

template <uint32_t I, typename... Ts, typename S>
result<void> variant_from_bin(std::variant<Ts...>& v, uint32_t i, S& stream) {
   if constexpr (I < std::variant_size_v<std::variant<Ts...>>) {
      if (i == I) {
         auto& x = v.template emplace<std::variant_alternative_t<I, std::variant<Ts...>>>();
         return from_bin(x, stream);
      } else {
         return variant_from_bin<I + 1>(v, i, stream);
      }
   } else {
      return stream_error::bad_variant_index;
   }
}

template <typename... Ts, typename S>
result<void> from_bin(std::variant<Ts...>& obj, S& stream) {
   uint32_t u;
   auto     r = varuint32_from_bin(u, stream);
   if (!r)
      return r;
   return variant_from_bin<0>(obj, u, stream);
}

template <typename T, typename S>
result<void> from_bin(T& obj, S& stream) {
   if constexpr (std::is_arithmetic_v<T>) {
      return stream.read((char*)(&obj), sizeof(obj));
   } else {
      result<void> r = outcome::success();
      for_each_field((T*)nullptr, [&](auto* name, auto member_ptr) {
         if (r)
            r = from_bin(member_from_void(member_ptr, &obj), stream);
      });
      return r;
   }
}

} // namespace eosio
