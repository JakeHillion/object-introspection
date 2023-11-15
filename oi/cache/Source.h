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

#include <folly/hash/Hash.h>

#include <filesystem>
#include <optional>
#include <string>

#include "oi/Features.h"
#include "oi/OIParser.h"
#include "oi/cache/Entity.h"

namespace oi::detail::cache {

// Source
//
// A source of cached Entities. May be implemented as a local or remote
// cache. Not expected to build a cache if one is not found.
class Source {
 public:
  struct Request {
    FeatureSet features;
    irequest probe;

    pid_t traceePid = -1;
    std::optional<std::string> buildId;
  };

  virtual ~Source() = default;

  // Read and fill an Entity from the cache
  //
  // Return true if the operation did not fail. This includes if this Source is
  // intentionally write-only.
  virtual bool read(const Request&, Entity&) {
    return true;
  }

  // Write a filled Entity to the cache
  //
  // Return true if the operation did not fail. This includes if this Source is
  // intentionally read-only.
  virtual bool write(const Request&, const Entity&) {
    return true;
  }
};

}  // namespace oi::detail::cache

namespace std {

template <>
struct hash<oi::detail::cache::Source::Request> {
  size_t operator()(const oi::detail::cache::Source::Request& req) const {
    return folly::hash::hash_combine(req.features, req.probe, req.buildId);
  }
};

}  // namespace std
