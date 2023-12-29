/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef INCLUDED_OI_EXPORTERS_JSON_H
#define INCLUDED_OI_EXPORTERS_JSON_H 1

#include <oi/IntrospectionResult.h>
#include <oi/result/SizedResult.h>

#include <ostream>
#include <string_view>

namespace oi::exporters {

class Json {
 public:
  Json(std::ostream& out);

  template <typename Res>
  void print(const Res& r) {
    auto begin = r.begin();
    auto end = r.end();
    return print(begin, end);
  }
  template <typename It>
  void print(It& it, const It& end);

  void setPretty(bool pretty) {
    pretty_ = pretty;
  }

 private:
  std::string_view tab() const;
  std::string_view space() const;
  std::string_view endl() const;
  static std::string makeIndent(size_t depth);

  void printStringField(std::string_view name, std::string_view value);
  void printBoolField(std::string_view name, bool value);
  void printUnsignedField(std::string_view name, uint64_t value);
  void printPointerField(std::string_view name, uintptr_t value);
  template <typename Rng>
  void printListField(std::string_view name, const Rng& range);

  void printFields(const result::Element&);
  template <typename El>
  void printFields(const result::SizedElement<El>&);

  bool pretty_ = false;
  std::ostream& out_;

  std::string indent_;
};

inline Json::Json(std::ostream& out) : out_(out) {
}

inline std::string_view Json::tab() const {
  return pretty_ ? "  " : "";
}
inline std::string_view Json::space() const {
  return pretty_ ? " " : "";
}
inline std::string_view Json::endl() const {
  return pretty_ ? "\n" : "";
}
inline std::string Json::makeIndent(size_t depth) {
  depth = std::max(depth, 1UL);
  return std::string((depth - 1) * 4, ' ');
}

inline void Json::printStringField(std::string_view name, std::string_view value) {
    out_ << tab() << '"' << name << '"' << ':' << space() << "\"" << value << "\"," << endl() << indent_;
}
inline void Json::printBoolField(std::string_view name, bool value) {
    out_ << tab() << '"' << name << '"' << ':' << space() << value << ',' << endl() << indent_;
}
inline void Json::printUnsignedField(std::string_view name, uint64_t value) {
    out_ << tab() << '"' << name << '"' << ':' << space() << value << ',' << endl() << indent_;
}
inline void Json::printPointerField(std::string_view name, uintptr_t value) {
      out_ << tab() << '"' << name << '"' << space() << "\"0x" << std::hex <<value
           << std::dec << "\"," << endl()
           << indent_;
}
template <typename Rng>
void Json::printListField(std::string_view name, const Rng& range) {
  out_ << tab() << '"' << name << '"' << ':' << space() << '[';
  bool first = true;
  for (const auto& el : range) {
    if (!std::exchange(first, false))
      out_ << ',' << space();
    out_ << '"' << el << '"';
  }
  out_ << "]," << endl() << indent_;
}

template <typename El>
void Json::printFields(const result::SizedElement<El>& el) {
  printUnsignedField("size", el.size);

  printFields(el.inner());
}

inline void Json::printFields(const result::Element& el) {
  printStringField("name", el.name);
  printListField("typePath", el.type_path);
  printListField("typeNames", el.type_names);
  printUnsignedField("staticSize", el.static_size);
  printUnsignedField("exclusiveSize", el.exclusive_size);
  if (el.pointer.has_value())
    printUnsignedField("pointer", *el.pointer);

  if (const auto* s = std::get_if<result::Element::Scalar>(&el.data)) {
    printUnsignedField("data", s->n);
  } else if (const auto* p = std::get_if<result::Element::Pointer>(&el.data)) {
    printPointerField("data", p->p);
  } else if (const auto* str = std::get_if<std::string>(&el.data)) {
    printStringField("data", *str);
  }

  if (el.container_stats.has_value()) {
    printUnsignedField("length", el.container_stats->length);
    printUnsignedField("capacity", el.container_stats->capacity);
  }
  if (el.is_set_stats.has_value())
    printUnsignedField("is_set", el.is_set_stats->is_set);
}

template <typename It>
void Json::print(It& it, const It& end) {
  const auto depth = it->type_path.size();

  indent_ = pretty_ ? makeIndent(depth) : "";
  const auto lastIndent =
      pretty_ ? makeIndent(depth - 1) : "";

  out_ << '[' << endl() << indent_;

  bool first = true;
  while (it != end) {
    if (it->type_path.size() < depth) {
      // no longer a sibling, must be a sibling of the parent
      break;
    }

    if (!std::exchange(first, false))
      out_ << ',' << endl() << indent_;

    out_ << '{' << endl() << indent_;

    printFields(*it);

    out_ << tab() << "\"members\":" << space();
    if (++it != end && it->type_path.size() > depth) {
      print(it, end);
    } else {
      out_ << "[]" << endl();
    }

    out_ << indent_ << "}";
  }
  if (depth == 1) {
    out_ << endl() << ']' << endl();
  } else {
    out_ << endl() << lastIndent << tab() << ']' << endl();
  }
}

}  // namespace oi::exporters

#endif
