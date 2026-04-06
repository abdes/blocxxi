//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <Blocxxi/Codec/api_export.h>

namespace blocxxi::codec::bencode {

class BLOCXXI_CODEC_API Value {
public:
  enum class Type : std::uint8_t {
    Integer,
    String,
    List,
    Dictionary,
  };

  using ListType = std::vector<Value>;
  using DictionaryType = std::map<std::string, Value, std::less<>>;

  Value() = default;
  explicit Value(std::int64_t integer);
  explicit Value(std::string string);
  explicit Value(const char* string);
  explicit Value(ListType list);
  explicit Value(DictionaryType dictionary);

  [[nodiscard]] auto GetType() const noexcept -> Type { return type_; }

  [[nodiscard]] auto AsInteger() const -> std::int64_t;
  [[nodiscard]] auto AsString() const -> std::string const&;
  [[nodiscard]] auto AsList() const -> ListType const&;
  [[nodiscard]] auto AsDictionary() const -> DictionaryType const&;

  [[nodiscard]] auto AsList() -> ListType&;
  [[nodiscard]] auto AsDictionary() -> DictionaryType&;

private:
  Type type_ { Type::String };
  std::int64_t integer_ { 0 };
  std::string string_;
  ListType list_;
  DictionaryType dictionary_;
};

[[nodiscard]] BLOCXXI_CODEC_API auto operator==(
  Value const& lhs, Value const& rhs) -> bool;

[[nodiscard]] BLOCXXI_CODEC_API auto Encode(Value const& value) -> std::string;
[[nodiscard]] BLOCXXI_CODEC_API auto Decode(std::string_view encoded) -> Value;

} // namespace blocxxi::codec::bencode
