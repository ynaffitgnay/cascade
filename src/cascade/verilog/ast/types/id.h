// Copyright 2017-2019 VMware, Inc.
// SPDX-License-Identifier: BSD-2-Clause
//
// The BSD-2 license (the License) set forth below applies to all parts of the
// Cascade project.  You may not use this file except in compliance with the
// License.
//
// BSD-2 License
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CASCADE_SRC_VERILOG_AST_ID_H
#define CASCADE_SRC_VERILOG_AST_ID_H

#include <string>
#include "common/tokenize.h"
#include "verilog/ast/types/expression.h"
#include "verilog/ast/types/macro.h"
#include "verilog/ast/types/node.h"
#include "verilog/ast/types/string.h"

namespace cascade {

class Id : public Node {
  public:
    // Constructors:
    explicit Id(const std::string& sid__);
    Id(const std::string& sid__, Expression* isel__);
    explicit Id(Tokenize::Token sid__);
    Id(Tokenize::Token sid__, Expression* isel__);
    ~Id() override;

    // Node Interface:
    NODE(Id);
    Id* clone() const override;

    // Get/Set:
    VAL_GET_SET(Id, Tokenize::Token, sid)
    MAYBE_GET_SET(Id, Expression, isel)
    const std::string& get_readable_sid();
    const std::string& get_readable_sid() const;
    void set_sid(const std::string& sid);
    void assign_sid(const std::string& sid);

    // Comparison Operators:
    bool eq(const Id* rhs) const;
    bool eq(const std::string& rhs) const;
    bool eq(const String* rhs) const;

  private:
    VAL_ATTR(Tokenize::Token, sid);
    MAYBE_ATTR(Expression, isel);
};

inline Id::Id(const std::string& sid__) : Id(Tokenize().map(sid__)) { }

inline Id::Id(const std::string& sid__, Expression* isel__) : Id(Tokenize().map(sid__), isel__) { }

inline Id::Id(Tokenize::Token sid__) : Node(Node::Tag::id) {
  VAL_SETUP(sid);
  MAYBE_DEFAULT_SETUP(isel);
  parent_ = nullptr;
}

inline Id::Id(Tokenize::Token sid__, Expression* isel__) : Id(sid__) {
  MAYBE_SETUP(isel);
}

inline Id::~Id() {
  VAL_TEARDOWN(sid);
  MAYBE_TEARDOWN(isel);
}

inline Id* Id::clone() const {
  auto* res = new Id(sid_);
  MAYBE_CLONE(isel);
  return res;
}

inline const std::string& Id::get_readable_sid() {
  return Tokenize().unmap(sid_);
}

inline const std::string& Id::get_readable_sid() const {
  return Tokenize().unmap(sid_);
}

inline void Id::set_sid(const std::string& sid) {
  sid_ = Tokenize().map(sid);
}

inline void Id::assign_sid(const std::string& sid) {
  sid_ = Tokenize().map(sid);
}

inline bool Id::eq(const Id* rhs) const {
  return is_null_isel() && rhs->is_null_isel() && (sid_ == rhs->sid_);
}

inline bool Id::eq(const std::string& rhs) const {
  return sid_ == Tokenize().map(rhs) && is_null_isel();
}

inline bool Id::eq(const String* rhs) const {
  return sid_ == rhs->get_val() && is_null_isel();
}

} // namespace cascade 

#endif
