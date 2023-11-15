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
#include "LocalCache.h"

#include <fstream>
#include <stdexcept>  // TODO: remove
#include <string_view>

namespace oi::detail::cache {
namespace {

std::string genHash(const Source::Request& req);
std::string_view getExtension(const Entity& en);

}  // namespace

bool LocalCache::read(const Source::Request& req, Entity& en) {
  std::string name = genHash(req);
  name.append(getExtension(en));
  std::filesystem::path path = cache_dir_ / name;

  if (!std::filesystem::exists(path))
    return false;

  std::ifstream ifs{path};
  en.load(ifs);

  return true;
}

bool LocalCache::write(const Source::Request& req, const Entity& en) {
  std::string name = genHash(req);
  name.append(getExtension(en));
  std::filesystem::path path = cache_dir_ / name;

  std::filesystem::create_directories(cache_dir_);

  std::ofstream of{path};
  en.store(of);

  return true;
}

namespace {

std::string genHash(const Source::Request& req) {
  return std::to_string(std::hash<Source::Request>{}(req));
}

std::string_view getExtension(const Entity& en) {
  switch (en.type()) {
    case EntityType::SourceCode:
      return ".cc";
    case EntityType::ObjectCode:
      return ".o";
    case EntityType::FuncDescs:
      return ".fd";
    case EntityType::GlobalDescs:
      return ".gd";
    case EntityType::TypeHierarchy:
      return ".th";
    case EntityType::PaddingInfo:
      return ".pd";
  }
}

}  // namespace
}  // namespace oi::detail::cache
