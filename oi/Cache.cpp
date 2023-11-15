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
#include "Cache.h"

namespace oi::detail {

using cache::Builder;
using cache::Entity;
using cache::Source;

void Cache::registerSource(std::unique_ptr<cache::Source> s) {
  sources.emplace_back(std::move(s));
}
void Cache::registerBuilder(std::unique_ptr<cache::Builder> b) {
  builders.emplace_back(std::move(b));
}

bool Cache::read(const Source::Request& req, Entity& en) {
  for (auto& s : sources) {
    if (s->read(req, en))
      return true;
  }
  return false;
}

bool Cache::write(const Source::Request& req, const Entity& en) {
  for (auto& s : sources) {
    if (s->write(req, en))
      return true;
  }
  return false;
}

Builder::Response Cache::build(const Builder::Request& req) {
  for (auto& b : builders) {
    if (auto ret = b->build(req); ret != Builder::Response::Failure)
      return ret;
  }
  return Builder::Response::Failure;
}

}  // namespace oi::detail
