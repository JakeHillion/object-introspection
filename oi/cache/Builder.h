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

#include "oi/Features.h"
#include "oi/cache/Source.h"

namespace oi::detail::cache {

// Builder
//
// A builder which takes a request and satisfies it into a cache, usually
// remote. It is expected that you first check for presence in a Source, then
// call a remote Builder if an entry is not found.
class Builder {
 public:
  struct Request {
    Source::Request location;
  };
  enum class Response {
    Failure,
    Processing,
    Success,
  };

  virtual ~Builder() = default;

  virtual Response build(const Request&);
};

}  // namespace oi::detail::cache
