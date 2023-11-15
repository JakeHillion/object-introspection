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

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "oi/cache/Builder.h"
#include "oi/cache/Source.h"

namespace oi::detail {

class Cache {
 public:
  void registerSource(std::unique_ptr<cache::Source>);
  void registerBuilder(std::unique_ptr<cache::Builder>);

  bool read(const cache::Source::Request&, cache::Entity&);
  bool write(const cache::Source::Request&, const cache::Entity&);

  cache::Builder::Response build(const cache::Builder::Request&);

 private:
  std::vector<std::unique_ptr<cache::Source>> sources;
  std::vector<std::unique_ptr<cache::Builder>> builders;
};

}  // namespace oi::detail
