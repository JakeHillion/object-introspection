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

#include "oi/OIGenerator.h"

#include <clang/AST/Mangle.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Sema/Sema.h>
#include <glog/logging.h>

#include <boost/core/demangle.hpp>
#include <fstream>
#include <iostream>
#include <range/v3/action/transform.hpp>
#include <range/v3/core.hpp>
#include <range/v3/view/cache1.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/drop_while.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <stdexcept>  // TODO: remove
#include <string_view>
#include <unordered_map>
#include <variant>

#include "oi/CodeGen.h"
#include "oi/Config.h"
#include "oi/Headers.h"
#include "oi/type_graph/ClangTypeParser.h"
#include "oi/type_graph/Printer.h"
#include "oi/type_graph/TypeGraph.h"
#include "oi/type_graph/Types.h"

namespace oi::detail {
namespace {

class CreateTypeGraphConsumer;
class CreateTypeGraphAction : public clang::ASTFrontendAction {
 public:
  CreateTypeGraphAction(clang::CompilerInstance& inst_) : inst{inst_} {
  }

  type_graph::TypeGraph typeGraph;
  std::unordered_map<std::string, type_graph::Type*> nameToTypeMap;

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& CI, clang::StringRef file) override;

 private:
  clang::CompilerInstance& inst;

  friend CreateTypeGraphConsumer;
};

}  // namespace

int OIGenerator::generate() {
  clang::CompilerInstance inst;
  inst.createDiagnostics();

  auto inv = std::make_shared<clang::CompilerInvocation>();

  std::vector<const char*> clangArgsCstr =
      clangArgs |
      ranges::views::transform([](const auto& s) { return s.c_str(); }) |
      ranges::to<std::vector>();
  if (!clang::CompilerInvocation::CreateFromArgs(
          *inv, clangArgsCstr, inst.getDiagnostics())) {
    LOG(ERROR) << "Failed to initialise compiler!";
    return -1;
  }

  inst.setInvocation(inv);

  CreateTypeGraphAction action{inst};
  if (!inst.ExecuteAction(action)) {
    LOG(ERROR) << "Action execution failed!";
    return -1;
  }
  if (action.nameToTypeMap.size() > 1)
    throw std::logic_error(
        "found more than one type to generate for but we can't currently "
        "handle this case");

  std::map<Feature, bool> featuresMap = {
      {Feature::TypeGraph, true},
      {Feature::TreeBuilderV2, true},
      {Feature::Library, true},
      {Feature::PackStructs, true},
      {Feature::PruneTypeGraph, true},
  };

  OICodeGen::Config generatorConfig{};
  OICompiler::Config compilerConfig{};
  compilerConfig.usePIC = pic;

  auto features = config::processConfigFiles(
      configFilePaths, featuresMap, compilerConfig, generatorConfig);
  if (!features) {
    LOG(ERROR) << "failed to process config file";
    return -1;
  }
  generatorConfig.features = *features;
  compilerConfig.features = *features;

  CodeGen codegen{generatorConfig};
  if (!codegen.registerContainers()) {
    LOG(ERROR) << "Failed to register containers";
    return -1;
  }
  codegen.transform(action.typeGraph);

  std::string code;
  codegen.generate(action.typeGraph, code, nullptr);

  std::string sourcePath = sourceFileDumpPath;
  if (sourceFileDumpPath.empty()) {
    // This is the path Clang acts as if it has compiled from e.g. for debug
    // information. It does not need to exist.
    sourcePath = "oil_jit.cpp";
  } else {
    std::ofstream outputFile(sourcePath);
    outputFile << code;
  }

  OICompiler compiler{{}, compilerConfig};
  return compiler.compile(code, sourcePath, outputPath) ? 0 : -1;
}

namespace {

class CreateTypeGraphConsumer : public clang::ASTConsumer {
 private:
  CreateTypeGraphAction& self;

 public:
  CreateTypeGraphConsumer(CreateTypeGraphAction& self_) : self(self_) {
  }

  void HandleTranslationUnit(clang::ASTContext& Context) override {
    auto* tu_decl = Context.getTranslationUnitDecl();
    auto decls = tu_decl->decls();
    auto oi_namespaces = decls | ranges::views::transform([](auto* p) {
                           return llvm::dyn_cast<clang::NamespaceDecl>(p);
                         }) |
                         ranges::views::filter([](auto* ns) {
                           return ns != nullptr && ns->getName() == "oi";
                         });
    if (oi_namespaces.empty()) {
      LOG(WARNING) << "Failed to find `oi` namespace. Does this input "
                      "include <oi/oi.h>?";
      return;
    }

    auto introspectImpl =
        std::move(oi_namespaces) |
        ranges::views::for_each([](auto* ns) { return ns->decls(); }) |
        ranges::views::transform([](auto* p) {
          return llvm::dyn_cast<clang::FunctionTemplateDecl>(p);
        }) |
        ranges::views::filter([](auto* td) {
          return td != nullptr && td->getName() == "introspectImpl";
        }) |
        ranges::views::take(1) | ranges::to<std::vector>();
    if (introspectImpl.empty()) {
      LOG(WARNING)
          << "Failed to find `oi::introspect` within the `oi` namespace. Did "
             "you compile with `OIL_AOT_COMPILATION=1`?";
      return;
    }

    auto nameToClangTypeMap =
        ranges::views::single(introspectImpl[0]) |
        ranges::views::for_each(
            [](auto* td) { return td->specializations(); }) |
        ranges::views::transform(
            [](auto* p) { return llvm::dyn_cast<clang::FunctionDecl>(p); }) |
        ranges::views::filter([](auto* p) { return p != nullptr; }) |
        ranges::views::transform(
            [](auto* fd) -> std::pair<std::string, const clang::Type*> {
              clang::ASTContext& Ctx = fd->getASTContext();
              clang::ASTNameGenerator ASTNameGen(Ctx);
              std::string name = ASTNameGen.getName(fd);

              assert(fd->getNumParams() == 1);
              const clang::Type* type =
                  fd->parameters()[0]->getType().getTypePtr();
              return {name, type};
            }) |
        ranges::to<std::unordered_map>();
    if (nameToClangTypeMap.empty())
      return;

    type_graph::ClangTypeParserOptions opts;
    type_graph::ClangTypeParser parser{self.typeGraph, opts};

    auto& Sema = self.inst.getSema();
    self.nameToTypeMap =
        nameToClangTypeMap |
        ranges::views::transform(
            [&parser, &Context, &Sema](
                auto& p) -> std::pair<std::string, type_graph::Type*> {
              return {p.first, &parser.parse(Context, Sema, *p.second)};
            }) |
        ranges::to<std::unordered_map>();

    for (const auto& [name, type] : self.nameToTypeMap)
      self.typeGraph.addRoot(*type);
  }
};

std::unique_ptr<clang::ASTConsumer> CreateTypeGraphAction::CreateASTConsumer(
    clang::CompilerInstance& CI, clang::StringRef file) {
  return std::make_unique<CreateTypeGraphConsumer>(*this);
}

}  // namespace
}  // namespace oi::detail
