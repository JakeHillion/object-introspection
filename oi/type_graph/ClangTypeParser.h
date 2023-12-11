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
#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "oi/type_graph/TypeGraph.h"

namespace clang {
class ASTContext;
class BuiltinType;
class ConstantArrayType;
class ElaboratedType;
class LValueReferenceType;
class PointerType;
class RecordType;
class Sema;
class SubstTemplateTypeParmType;
class TemplateArgument;
class TemplateSpecializationType;
class Type;
class TypedefType;
}  // namespace clang

namespace oi::detail::type_graph {

class Array;
class Class;
class Member;
class Primitive;
class Reference;
class Type;
class TypeGraph;
class Typedef;
struct TemplateParam;

struct ClangTypeParserOptions {
  bool chaseRawPointers = false;
};

/*
 * SourceParser
 *
 * Reads source information from a source file to build a type graph.
 * Returns a reference to the Type node corresponding to the given drgn_type.
 */
class ClangTypeParser {
 public:
  ClangTypeParser(TypeGraph& typeGraph, ClangTypeParserOptions options)
      : typeGraph_(typeGraph), options_(options) {
  }

  // Parse from a clang type.
  Type& parse(clang::ASTContext&, clang::Sema&, const clang::Type&);

 private:
  TypeGraph& typeGraph_;
  ClangTypeParserOptions options_;
  clang::ASTContext* ast;
  clang::Sema* sema;

  uint_fast32_t depth_;
  std::unordered_map<const clang::Type*, std::reference_wrapper<Type>>
      clang_types_;

  Type& enumerateType(const clang::Type&);
  Class& enumerateClass(const clang::RecordType&);
  Type& enumerateReference(const clang::LValueReferenceType&);
  Type& enumeratePointer(const clang::PointerType&);
  Type& enumerateSubstTemplateTypeParm(const clang::SubstTemplateTypeParmType&);
  Primitive& enumeratePrimitive(const clang::BuiltinType&);
  Type& enumerateElaboratedType(const clang::ElaboratedType&);
  Type& enumerateTemplateSpecialization(
      const clang::TemplateSpecializationType&);
  Typedef& enumerateTypedef(const clang::TypedefType&);
  Array& enumerateArray(const clang::ConstantArrayType&);

  void enumerateClassTemplateParams(const clang::RecordType&,
                                    std::vector<TemplateParam>&);
  TemplateParam enumerateTemplateParam(const clang::TemplateArgument&);
  void enumerateClassMembers(const clang::RecordType&, std::vector<Member>&);

  template <typename T, typename... Args>
  T& makeType(const clang::Type& clangType, Args&&... args) {
    auto& newType = typeGraph_.makeType<T>(std::forward<Args>(args)...);
    clang_types_.insert({&clangType, newType});
    return newType;
  }

  bool chasePointer() const;
};

}  // namespace oi::detail::type_graph
