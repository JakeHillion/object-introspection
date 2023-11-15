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
#include "Entity.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "oi/Serialize.h"

namespace oi::detail::cache {

void SourceCode::store(std::ostream& os) const {
  os << code;
}
void SourceCode::load(std::istream& is) {
  is >> code;
}

void ObjectCode::store(std::ostream& os) const {
  std::copy(code.begin(), code.end(), std::ostreambuf_iterator(os));
}
void ObjectCode::load(std::istream& is) {
  std::copy(std::istreambuf_iterator<char>(is),
            std::istreambuf_iterator<char>(), std::back_inserter(code));
}

void FuncDescs::store(std::ostream& os) const {
  boost::archive::text_oarchive oa{os};
  oa << descs;
}
void FuncDescs::load(std::istream& is) {
  boost::archive::text_iarchive ia{is};
  ia >> descs;
}

void GlobalDescs::store(std::ostream& os) const {
  boost::archive::text_oarchive oa{os};
  oa << descs;
}
void GlobalDescs::load(std::istream& is) {
  boost::archive::text_iarchive ia{is};
  ia >> descs;
}

void TypeHierarchy::store(std::ostream& os) const {
  boost::archive::text_oarchive oa{os};
  oa << root;
  oa << th;
}
void TypeHierarchy::load(std::istream& is) {
  boost::archive::text_iarchive ia{is};
  ia >> root;
  ia >> th;
}

void PaddingInfo::store(std::ostream& os) const {
  boost::archive::text_oarchive oa{os};
  oa << info;
}
void PaddingInfo::load(std::istream& is) {
  boost::archive::text_iarchive ia{is};
  ia >> info;
}

}  // namespace oi::detail::cache
