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
#include "ClangTypeParser.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/QualTypeNames.h>
#include <clang/AST/Type.h>
#include <clang/Sema/Sema.h>
#include <glog/logging.h>

#include <stdexcept>  // TODO: remove

#include "oi/type_graph/Types.h"

namespace oi::detail::type_graph {
namespace {

bool requireCompleteType(clang::Sema& sema, const clang::Type& ty);

}

Type& ClangTypeParser::parse(clang::ASTContext& ast_,
                             clang::Sema& sema_,
                             const clang::Type& ty) {
  ast = &ast_;
  sema = &sema_;

  depth_ = 0;
  return enumerateType(ty);
}

Type& ClangTypeParser::enumerateType(const clang::Type& ty) {
  // Avoid re-enumerating an already-processsed type
  if (auto it = clang_types_.find(&ty); it != clang_types_.end())
    return it->second;

  struct DepthTracker {
    DepthTracker(ClangTypeParser& self_) : self{self_} {
      self.depth_++;
    }
    ~DepthTracker() {
      self.depth_--;
    }

   private:
    ClangTypeParser& self;
  } d{*this};

  if (requireCompleteType(*sema, ty))
    return makeType<Incomplete>(ty, "incomplete (TODO naming)");

  switch (ty.getTypeClass()) {
    case clang::Type::Record:
      return enumerateClass(*ty.getAs<const clang::RecordType>());
    case clang::Type::LValueReference:
      return enumerateReference(*ty.getAs<const clang::LValueReferenceType>());
    case clang::Type::Pointer:
      return enumeratePointer(*ty.getAs<const clang::PointerType>());
    case clang::Type::SubstTemplateTypeParm:
      return enumerateSubstTemplateTypeParm(
          *ty.getAs<const clang::SubstTemplateTypeParmType>());
    case clang::Type::Builtin:
      return enumeratePrimitive(*ty.getAs<const clang::BuiltinType>());
    case clang::Type::Elaborated:
      return enumerateElaboratedType(*ty.getAs<const clang::ElaboratedType>());
    case clang::Type::TemplateSpecialization:
      return enumerateTemplateSpecialization(
          *ty.getAs<const clang::TemplateSpecializationType>());
    default:
      throw std::logic_error(std::string("unsupported TypeClass `") +
                             ty.getTypeClassName() + '`');
  }
}

Type& ClangTypeParser::enumerateTemplateSpecialization(
    const clang::TemplateSpecializationType& ty) {
  if (ty.isSugared())
    return enumerateType(*ty.desugar());

  LOG(WARNING) << "failed on a TemplateSpecializationType";
  ty.dump();
  return makeType<Primitive>(ty, Primitive::Kind::Int32);
}

Class& ClangTypeParser::enumerateClass(const clang::RecordType& ty) {
  auto* decl = ty.getDecl();

  std::string name = decl->getNameAsString();
  std::string fqName = clang::TypeName::getFullyQualifiedName(
      clang::QualType(&ty, 0), *ast, {ast->getLangOpts()});

  auto kind = Class::Kind::Struct;  // TODO: kind

  auto size = ast->getTypeSize(clang::QualType(&ty, 0)) / 8;
  int virtuality = 0;

  auto& c = makeType<Class>(
      ty, kind, std::move(name), std::move(fqName), size, virtuality);

  enumerateClassTemplateParams(ty, c.templateParams);
  // enumerateClassParents(type, c.parents);
  enumerateClassMembers(ty, c.members);
  // enumerateClassFunctions(type, c.functions);

  return c;
}

void ClangTypeParser::enumerateClassTemplateParams(
    const clang::RecordType& ty, std::vector<TemplateParam>& params) {
  assert(params.empty());

  auto* decl = dyn_cast<clang::ClassTemplateSpecializationDecl>(ty.getDecl());
  if (decl == nullptr)
    return;

  const auto& list = decl->getTemplateArgs();
  params.reserve(list.size());

  for (const auto& arg : list.asArray()) {
    params.emplace_back(enumerateTemplateParam(arg));
  }
}

TemplateParam ClangTypeParser::enumerateTemplateParam(
    const clang::TemplateArgument& p) {
  switch (p.getKind()) {
    case clang::TemplateArgument::Type: {
      auto qualType = p.getAsType();
      QualifierSet qualifiers;
      qualifiers[Qualifier::Const] = qualType.isConstQualified();
      Type& ttype = enumerateType(*qualType);
      return {ttype, qualifiers};
    }
    default:
      p.dump();
      throw std::logic_error("unsupported template argument kind");
  }
}

void ClangTypeParser::enumerateClassMembers(const clang::RecordType& ty,
                                            std::vector<Member>& members) {
  assert(members.empty());

  auto* decl = ty.getDecl();

  for (const auto* field : decl->fields()) {
    clang::QualType qualType = field->getType();
    std::string member_name = field->getNameAsString();

    size_t size_in_bits = 0;
    if (field->isBitField()) {
      size_in_bits = field->getBitWidthValue(*ast);
    }

    size_t offset_in_bits = decl->getASTContext().getFieldOffset(field);

    auto& mtype = enumerateType(*qualType);
    Member m{mtype, std::move(member_name), offset_in_bits, size_in_bits};
    members.push_back(m);
  }

  std::sort(members.begin(), members.end(), [](const auto& a, const auto& b) {
    return a.bitOffset < b.bitOffset;
  });
}

Type& ClangTypeParser::enumerateReference(
    const clang::LValueReferenceType& ty) {
  // TODO: function references
  Type& t = enumerateType(*ty.getPointeeType());
  if (dynamic_cast<Incomplete*>(&t))
    return makeType<Pointer>(ty, t);

  return makeType<Reference>(ty, t);
}

Type& ClangTypeParser::enumeratePointer(const clang::PointerType& ty) {
  // TODO: function references
  if (!chasePointer())
    return makeType<Primitive>(ty, Primitive::Kind::StubbedPointer);

  Type& t = enumerateType(*ty.getPointeeType());
  return makeType<Reference>(ty, t);
}

Type& ClangTypeParser::enumerateSubstTemplateTypeParm(
    const clang::SubstTemplateTypeParmType& ty) {
  // Clang wraps any type that was substituted from e.g. `T` in this type. It
  // should have no representation in the type graph.
  return enumerateType(*ty.getReplacementType());
}

Type& ClangTypeParser::enumerateElaboratedType(
    const clang::ElaboratedType& ty) {
  // Clang wraps any type that is name qualified in this type. It should have no
  // representation in the type graph.
  return enumerateType(*ty.getNamedType());
}

Primitive& ClangTypeParser::enumeratePrimitive(const clang::BuiltinType& ty) {
  switch (ty.getKind()) {
    case clang::BuiltinType::Void:
      return makeType<Primitive>(ty, Primitive::Kind::Void);

    case clang::BuiltinType::Bool:
      return makeType<Primitive>(ty, Primitive::Kind::Bool);

    case clang::BuiltinType::Char_U:
    case clang::BuiltinType::UChar:
      return makeType<Primitive>(ty, Primitive::Kind::UInt8);
    case clang::BuiltinType::WChar_U:
      return makeType<Primitive>(ty, Primitive::Kind::UInt32);

    case clang::BuiltinType::Char_S:
    case clang::BuiltinType::SChar:
      return makeType<Primitive>(ty, Primitive::Kind::Int8);
    case clang::BuiltinType::WChar_S:
      return makeType<Primitive>(ty, Primitive::Kind::Int32);

    case clang::BuiltinType::Char16:
      return makeType<Primitive>(ty, Primitive::Kind::Int16);
    case clang::BuiltinType::Char32:
      return makeType<Primitive>(ty, Primitive::Kind::Int32);

    case clang::BuiltinType::UShort:
      return makeType<Primitive>(ty, Primitive::Kind::UInt16);
    case clang::BuiltinType::UInt:
      return makeType<Primitive>(ty, Primitive::Kind::UInt32);
    case clang::BuiltinType::ULong:
      return makeType<Primitive>(ty, Primitive::Kind::UInt64);
    case clang::BuiltinType::ULongLong:
      return makeType<Primitive>(ty, Primitive::Kind::Int64);

    case clang::BuiltinType::Short:
      return makeType<Primitive>(ty, Primitive::Kind::Int16);
    case clang::BuiltinType::Int:
      return makeType<Primitive>(ty, Primitive::Kind::Int32);
    case clang::BuiltinType::Long:
    case clang::BuiltinType::LongLong:
      return makeType<Primitive>(ty, Primitive::Kind::Int64);

    case clang::BuiltinType::UInt128:
    case clang::BuiltinType::Int128:
    default:
      ty.dump();
      throw std::logic_error(std::string("unsupported BuiltinType::Kind"));
  }
}

bool ClangTypeParser::chasePointer() const {
  // Always chase top-level pointers
  if (depth_ == 1)
    return true;
  return options_.chaseRawPointers;
}

namespace {

bool requireCompleteType(clang::Sema& sema, const clang::Type& ty) {
  // TODO: SEGVs if the type can't be completed
  return sema.RequireCompleteType(
      sema.getASTContext().getTranslationUnitDecl()->getEndLoc(),
      clang::QualType{&ty, 0},
      0);
}

}  // namespace
}  // namespace oi::detail::type_graph
