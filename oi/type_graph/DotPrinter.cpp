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
#include "DotPrinter.h"

#include <cmath>

namespace type_graph {

DotPrinter::DotPrinter(std::ostream& out) : out_{out} {
  out_ << "digraph {" << std::endl;
}

DotPrinter::~DotPrinter() {
  out_ << "}" << std::endl;
}

void DotPrinter::print(const Type& type) {
  type.accept(*this);
}

template <typename Fr, typename To>
void DotPrinter::edge(const Fr* from, const To* to) {
  out_ << (uintptr_t)from << " -> " << (uintptr_t)to << std::endl;
}

void DotPrinter::visit(const Class& c) {
  if (!hasPrinted_.insert(&c).second)
    return;

  std::string kind;
  switch (c.kind()) {
    case Class::Kind::Class:
      kind = "Class";
      break;
    case Class::Kind::Struct:
      kind = "Struct";
      break;
    case Class::Kind::Union:
      kind = "Union";
      break;
  }

  out_ << (uintptr_t)&c << " [label=\"";
  out_ << kind << ": " << c.name() << " (size: " << c.size()
       << align_str(c.align());
  if (c.packed()) {
    out_ << ", packed";
  }
  out_ << ")"
       << "\"]" << std::endl;

  for (const auto& param : c.templateParams) {
    edge(&c, &param);
    print_param(param);
  }
  for (const auto& parent : c.parents) {
    edge(&c, &parent);
    print_parent(parent);
  }
  for (const auto& member : c.members) {
    edge(&c, &member);
    print_member(member);
  }
  for (const auto& function : c.functions) {
    edge(&c, &function);
    print_function(function);
  }
  for (auto& child : c.children) {
    edge(&c, &child);
    print_child(child);
  }
}

void DotPrinter::visit(const Container& c) {
  if (!hasPrinted_.insert(&c).second)
    return;

  out_ << (uintptr_t)&c << " [label=\""
       << "Container: " << c.name() << " (size: " << c.size() << ")"
       << "\"]" << std::endl;

  for (const auto& param : c.templateParams) {
    edge(&c, &param);
    print_param(param);
  }
}

void DotPrinter::visit(const Primitive& p) {
  out_ << (uintptr_t)&p << " [label=\""
       << "Primitive: " << p.name() << "\"]" << std::endl;
}

void DotPrinter::visit(const Enum& e) {
  out_ << (uintptr_t)&e << " [label=\""
       << "Enum: " << e.name() << " (size: " << e.size() << ")"
       << "\"]" << std::endl;
}

void DotPrinter::visit(const Array& a) {
  out_ << (uintptr_t)&a << " [label=\""
       << "Array: (length: " << a.len() << ")"
       << "\"]" << std::endl;
  edge(&a, &a.elementType());
  print(a.elementType());
}

void DotPrinter::visit(const Typedef& td) {
  if (!hasPrinted_.insert(&td).second)
    return;

  out_ << (uintptr_t)&td << " [label=\""
       << "Typedef: " << td.name() << "\"]" << std::endl;
  edge(&td, &td.underlyingType());
  print(td.underlyingType());
}

void DotPrinter::visit(const Pointer& p) {
  out_ << (uintptr_t)&p << " [label=\"" << p.name() << "\"]" << std::endl;
  edge(&p, &p.pointeeType());
  print(p.pointeeType());
}

void DotPrinter::visit(const Dummy& d) {
  out_ << (uintptr_t)&d << " [label=\""
       << "Dummy (size: " << d.size() << align_str(d.align()) << ")"
       << "\"]" << std::endl;
}

void DotPrinter::visit(const DummyAllocator& d) {
  out_ << (uintptr_t)&d << " [label=\""
       << "DummyAllocator (size: " << d.size() << align_str(d.align()) << ")"
       << "\"]" << std::endl;

  print(d.allocType());
}

void DotPrinter::print_param(const TemplateParam& param) {
  out_ << (uintptr_t)&param << " [label=\"";
  out_ << "Param";
  if (param.value) {
    print_value(*param.value);
  }
  print_qualifiers(param.qualifiers);
  out_ << "\"]" << std::endl;

  if (!param.value) {
    edge(&param, param.type());
    print(*param.type());
  }
}

void DotPrinter::print_parent(const Parent& parent) {
  out_ << (uintptr_t)&parent << " [label=\""
       << "Parent (offset: " << static_cast<double>(parent.bitOffset) / 8 << ")"
       << "\"]" << std::endl;
  edge(&parent, &parent.type());
  print(parent.type());
}

void DotPrinter::print_member(const Member& member) {
  out_ << (uintptr_t)&member << " [label=\"";
  out_ << "Member: " << member.name
       << " (offset: " << static_cast<double>(member.bitOffset) / 8;
  out_ << align_str(member.align);
  if (member.bitsize != 0) {
    out_ << ", bitsize: " << member.bitsize;
  }
  out_ << ")"
       << "\"]" << std::endl;
  edge(&member, &member.type());
  print(member.type());
}

void DotPrinter::print_function(const Function& function) {
  out_ << (uintptr_t)&function << " [label=\"";
  out_ << "Function: " << function.name;
  if (function.virtuality != 0)
    out_ << " (virtual)";
  out_ << "\"]" << std::endl;
}

void DotPrinter::print_child(const Type& child) {
  // prefix();
  out_ << "Child" << std::endl;
  print(child);
}

void DotPrinter::print_value(const std::string& value) {
  out_ << "Value: " << value;
}

void DotPrinter::print_qualifiers(const QualifierSet& qualifiers) {
  if (qualifiers.none()) {
    return;
  }
  out_ << "Qualifiers:";
  if (qualifiers[Qualifier::Const]) {
    out_ << " const";
  }
}

std::string DotPrinter::align_str(uint64_t align) {
  if (align == 0)
    return "";
  return ", align: " + std::to_string(align);
}

}  // namespace type_graph
