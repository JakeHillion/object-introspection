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
#include "MemoryFile.h"

#include <glog/logging.h>
#include <sys/mman.h>

#include <boost/format.hpp>
#include <cstring>
#include <stdexcept>
#include <string>

MemoryFile::MemoryFile(const char* name) {
  fd_ = memfd_create(name, 0);
  if (fd_ == -1)
    throw std::runtime_error(std::string("memfd creation failed: ") +
                             std::strerror(errno));
}

MemoryFile::~MemoryFile() {
  if (fd_ == -1)
    return;

  PLOG_IF(ERROR, close(fd_) == -1) << "memfd close failed";
}

std::filesystem::path MemoryFile::path() {
  return {(boost::format("/dev/fd/%1%") % fd_).str()};
}
