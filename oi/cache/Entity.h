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

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "oi/Descs.h"
#include "oi/PaddingHunter.h"
#include "oi/TypeHierarchy.h"

namespace oi::detail::cache {

enum class EntityType {
  SourceCode,
  ObjectCode,
  FuncDescs,
  GlobalDescs,
  TypeHierarchy,
  PaddingInfo,
};

class Entity {
 public:
  virtual ~Entity() = default;

  virtual EntityType type() const = 0;

  virtual void store(std::ostream&) const = 0;
  virtual void load(std::istream&) = 0;
};

struct SourceCode final : public Entity {
  SourceCode() {
  }
  SourceCode(std::string code_) : code{std::move(code_)} {
  }

  std::string code;

  EntityType type() const override {
    return EntityType::SourceCode;
  }

  void store(std::ostream&) const override;
  void load(std::istream&) override;
};
struct ObjectCode final : public Entity {
  ObjectCode() {
  }
  ObjectCode(std::vector<uint8_t> code_) : code{std::move(code_)} {
  }

  std::vector<uint8_t> code;

  EntityType type() const override {
    return EntityType::ObjectCode;
  }

  void store(std::ostream&) const override;
  void load(std::istream&) override;
};
struct FuncDescs final : public Entity {
  FuncDescs() {
  }
  FuncDescs(std::unordered_map<std::string, std::shared_ptr<FuncDesc>> descs_)
      : descs{std::move(descs_)} {
  }

  std::unordered_map<std::string, std::shared_ptr<FuncDesc>> descs;

  EntityType type() const override {
    return EntityType::FuncDescs;
  }

  void store(std::ostream&) const override;
  void load(std::istream&) override;
};
struct GlobalDescs final : public Entity {
  GlobalDescs() {
  }
  GlobalDescs(
      std::unordered_map<std::string, std::shared_ptr<GlobalDesc>> descs_)
      : descs{std::move(descs_)} {
  }

  std::unordered_map<std::string, std::shared_ptr<GlobalDesc>> descs;

  EntityType type() const override {
    return EntityType::GlobalDescs;
  }

  void store(std::ostream&) const override;
  void load(std::istream&) override;
};
struct TypeHierarchy final : public Entity {
  TypeHierarchy() {
  }
  TypeHierarchy(RootInfo root_, ::TypeHierarchy th_)
      : root{std::move(root_)}, th{std::move(th_)} {
  }

  ::RootInfo root;
  ::TypeHierarchy th;

  EntityType type() const override {
    return EntityType::TypeHierarchy;
  }

  void store(std::ostream&) const override;
  void load(std::istream&) override;
};
struct PaddingInfo final : public Entity {
  PaddingInfo() {
  }
  PaddingInfo(std::map<std::string, ::PaddingInfo> info_)
      : info{std::move(info_)} {
  }

  std::map<std::string, ::PaddingInfo> info;

  EntityType type() const override {
    return EntityType::PaddingInfo;
  }

  void store(std::ostream&) const override;
  void load(std::istream&) override;
};

}  // namespace oi::detail::cache
