//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Codec/bencode.h>

#include <charconv>
#include <stdexcept>

namespace blocxxi::codec::bencode {

namespace {

template <typename Number>
auto ParseNumber(std::string_view input, char const* error_message) -> Number
{
  auto value = Number {};
  auto const* begin = input.data();
  auto const* end = begin + input.size();
  auto [ptr, error] = std::from_chars(begin, end, value);
  if (error != std::errc() || ptr != end) {
    throw std::invalid_argument(error_message);
  }
  return value;
}

void AppendEncodedString(std::string_view value, std::string& out)
{
  out.append(std::to_string(value.size()));
  out.push_back(':');
  out.append(value);
}

void AppendEncoded(Value const& value, std::string& out);

auto DecodeValue(std::string_view encoded, std::size_t& offset) -> Value
{
  if (offset >= encoded.size()) {
    throw std::invalid_argument("unexpected end of bencode input");
  }

  auto const token = encoded[offset];
  if (token == 'i') {
    ++offset;
    auto const end = encoded.find('e', offset);
    if (end == std::string_view::npos) {
      throw std::invalid_argument("unterminated bencode integer");
    }

    auto const number = encoded.substr(offset, end - offset);
    auto const integer
      = ParseNumber<std::int64_t>(number, "invalid bencode integer");
    offset = end + 1;
    return Value(integer);
  }

  if (token == 'l') {
    ++offset;
    Value::ListType items;
    while (offset < encoded.size() && encoded[offset] != 'e') {
      items.emplace_back(DecodeValue(encoded, offset));
    }
    if (offset >= encoded.size()) {
      throw std::invalid_argument("unterminated bencode list");
    }
    ++offset;
    return Value(std::move(items));
  }

  if (token == 'd') {
    ++offset;
    Value::DictionaryType entries;
    while (offset < encoded.size() && encoded[offset] != 'e') {
      auto const key = DecodeValue(encoded, offset);
      if (key.GetType() != Value::Type::String) {
        throw std::invalid_argument("bencode dictionary key must be a string");
      }
      entries.emplace(key.AsString(), DecodeValue(encoded, offset));
    }
    if (offset >= encoded.size()) {
      throw std::invalid_argument("unterminated bencode dictionary");
    }
    ++offset;
    return Value(std::move(entries));
  }

  if (token < '0' || token > '9') {
    throw std::invalid_argument("invalid bencode token");
  }

  auto const colon = encoded.find(':', offset);
  if (colon == std::string_view::npos) {
    throw std::invalid_argument("unterminated bencode string length");
  }

  auto const length_field = encoded.substr(offset, colon - offset);
  auto const length
    = ParseNumber<std::size_t>(length_field, "invalid bencode string length");

  offset = colon + 1;
  if (offset + length > encoded.size()) {
    throw std::invalid_argument("bencode string exceeds input size");
  }

  auto result = std::string(encoded.substr(offset, length));
  offset += length;
  return Value(std::move(result));
}

void AppendEncoded(Value const& value, std::string& out)
{
  switch (value.GetType()) {
  case Value::Type::Integer:
    out.push_back('i');
    out.append(std::to_string(value.AsInteger()));
    out.push_back('e');
    return;
  case Value::Type::String:
    AppendEncodedString(value.AsString(), out);
    return;
  case Value::Type::List:
    out.push_back('l');
    for (auto const& item : value.AsList()) {
      AppendEncoded(item, out);
    }
    out.push_back('e');
    return;
  case Value::Type::Dictionary:
    out.push_back('d');
    for (auto const& [key, item] : value.AsDictionary()) {
      AppendEncodedString(key, out);
      AppendEncoded(item, out);
    }
    out.push_back('e');
    return;
  }

  throw std::invalid_argument("unsupported bencode value type");
}

} // namespace

Value::Value(std::int64_t integer)
  : type_(Type::Integer)
  , integer_(integer)
{
}

Value::Value(std::string string)
  : type_(Type::String)
  , string_(std::move(string))
{
}

Value::Value(const char* string)
  : Value(std::string(string))
{
}

Value::Value(ListType list)
  : type_(Type::List)
  , list_(std::move(list))
{
}

Value::Value(DictionaryType dictionary)
  : type_(Type::Dictionary)
  , dictionary_(std::move(dictionary))
{
}

auto Value::AsInteger() const -> std::int64_t
{
  if (type_ != Type::Integer) {
    throw std::logic_error("bencode value is not an integer");
  }
  return integer_;
}

auto Value::AsString() const -> std::string const&
{
  if (type_ != Type::String) {
    throw std::logic_error("bencode value is not a string");
  }
  return string_;
}

auto Value::AsList() const -> ListType const&
{
  if (type_ != Type::List) {
    throw std::logic_error("bencode value is not a list");
  }
  return list_;
}

auto Value::AsDictionary() const -> DictionaryType const&
{
  if (type_ != Type::Dictionary) {
    throw std::logic_error("bencode value is not a dictionary");
  }
  return dictionary_;
}

auto Value::AsList() -> ListType&
{
  if (type_ != Type::List) {
    throw std::logic_error("bencode value is not a list");
  }
  return list_;
}

auto Value::AsDictionary() -> DictionaryType&
{
  if (type_ != Type::Dictionary) {
    throw std::logic_error("bencode value is not a dictionary");
  }
  return dictionary_;
}

auto operator==(Value const& lhs, Value const& rhs) -> bool
{
  if (lhs.GetType() != rhs.GetType()) {
    return false;
  }

  switch (lhs.GetType()) {
  case Value::Type::Integer:
    return lhs.AsInteger() == rhs.AsInteger();
  case Value::Type::String:
    return lhs.AsString() == rhs.AsString();
  case Value::Type::List:
    return lhs.AsList() == rhs.AsList();
  case Value::Type::Dictionary:
    return lhs.AsDictionary() == rhs.AsDictionary();
  }

  return false;
}

auto Encode(Value const& value) -> std::string
{
  auto out = std::string();
  AppendEncoded(value, out);
  return out;
}

auto Decode(std::string_view encoded) -> Value
{
  auto offset = std::size_t { 0 };
  auto value = DecodeValue(encoded, offset);
  if (offset != encoded.size()) {
    throw std::invalid_argument("trailing bencode data");
  }
  return value;
}

} // namespace blocxxi::codec::bencode
