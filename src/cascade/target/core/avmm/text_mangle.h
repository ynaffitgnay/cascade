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

#ifndef CASCADE_SRC_TARGET_CORE_AVMM_TEXT_MANGLE_H
#define CASCADE_SRC_TARGET_CORE_AVMM_TEXT_MANGLE_H

#include <limits>
#include <stddef.h>
#include <vector>
#include "target/core/avmm/var_table.h"
#include "verilog/analyze/module_info.h"
#include "verilog/ast/ast.h"
#include "verilog/ast/visitors/builder.h"

namespace cascade::avmm {

// Pass 1: 
// 
// This pass performs several major text transformations:
// 1. Declarations are deleted.
// 2. Attribute annotations are deleted.
// 3. $feof() expressions are replaced their corresponding vtable entry
// 4. System tasks are transformed into state udpate operations
// 5. Non-blocking assignments are transformed into state update operations

template <size_t V, typename A, typename T>
class TextMangle : public Builder {
  public:
    TextMangle(const ModuleDeclaration* md, const VarTable<V,A,T>* vt);
    ~TextMangle() override = default;

  private:
    const ModuleDeclaration* md_;
    const VarTable<V,A,T>* vt_;
    size_t task_index_;

    Attributes* build(const Attributes* as) override;
    ModuleItem* build(const RegDeclaration* rd) override;
    ModuleItem* build(const PortDeclaration* pd) override;
    Expression* build(const FeofExpression* fe) override;
    Statement* build(const BlockingAssign* ba) override;
    Statement* build(const NonblockingAssign* na) override;
    Statement* build(const DebugStatement* ds) override;
    Statement* build(const FflushStatement* fs) override;
    Statement* build(const FinishStatement* fs) override;
    Statement* build(const FseekStatement* fs) override;
    Statement* build(const GetStatement* gs) override;
    Statement* build(const PutStatement* ps) override;
    Statement* build(const RestartStatement* rs) override;
    Statement* build(const RetargetStatement* rs) override;
    Statement* build(const SaveStatement* ss) override;
    Statement* build(const YieldStatement* ys) override;

    Expression* get_table_range(const Identifier* r, const Identifier* i);
};

template <size_t V, typename A, typename T>
inline TextMangle<V,A,T>::TextMangle(const ModuleDeclaration* md, const VarTable<V,A,T>* vt) : Builder() {
  md_ = md;
  vt_ = vt;
  task_index_ = 1;
}

template <size_t V, typename A, typename T>
inline Attributes* TextMangle<V,A,T>::build(const Attributes* as) {
  (void) as;
  return new Attributes();
}

template <size_t V, typename A, typename T>
inline ModuleItem* TextMangle<V,A,T>::build(const RegDeclaration* rd) {
  return ModuleInfo(md_).is_stateful(rd->get_id()) ? 
    nullptr : 
    new RegDeclaration(
      new Attributes(),
      rd->accept_id(this),
      rd->get_type(),
      rd->accept_dim(this),
      rd->accept_val(this)
    );
}

template <size_t V, typename A, typename T>
inline ModuleItem* TextMangle<V,A,T>::build(const PortDeclaration* pd) {
  ModuleInfo info(md_);
  if (info.is_stateful(pd->get_decl()->get_id()) || info.is_input(pd->get_decl()->get_id())) {
    return nullptr;
  } else {
    return new PortDeclaration(new Attributes(), pd->get_type(), pd->accept_decl(this));
  }
}

template <size_t V, typename A, typename T>
inline Expression* TextMangle<V,A,T>::build(const FeofExpression* fe) {
  return new Identifier(new Id("__feof"), fe->clone_fd());
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const BlockingAssign* ba) {
  // Look up the target of this assignment 
  const auto* r = Resolve().get_resolution(ba->get_lhs());
  assert(r != nullptr);

  // If this entry doesn't appear in the vtable, we can leave it as is
  const auto titr = vt_->find(r);
  if (titr == vt_->end()) {
    return ba->clone();
  }
  // Otherwise, replace the original assignment with an assignment to a concatenation
  std::vector<Identifier*> lhs;
  for (size_t i = 0, ie = titr->second.words_per_element; i < ie; ++i) {
    lhs.push_back(new Identifier(new Id("__var"), new Number(Bits(std::numeric_limits<T>::digits, titr->second.begin+ie-i-1))));
  }
  return new BlockingAssign(lhs.begin(), lhs.end(), ba->get_rhs()->clone());
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const NonblockingAssign* na) {
  auto* res = new SeqBlock();

  // Look up the target of this assignment 
  const auto* lhs = na->get_lhs();
  const auto* r = Resolve().get_resolution(lhs);
  assert(r != nullptr);
  
  // Replace the original assignment with an assignment to a temporary variable
  auto* next = lhs->clone();
  next->purge_ids();
  next->push_back_ids(new Id(lhs->front_ids()->get_readable_sid() + "_next"));
  res->push_back_stmts(new NonblockingAssign(
    na->clone_ctrl(),
    next,
    na->get_rhs()->clone()
  ));

  // Insert a new assignment to the next mask
  res->push_back_stmts(new NonblockingAssign(
    new Identifier(
      new Id("__update_mask"),
      get_table_range(r, lhs)
    ),
    new UnaryExpression(
      UnaryExpression::Op::TILDE,
      new Identifier(
        new Id("__prev_update_mask"),
        get_table_range(r, lhs)
      )
    )
  ));

  return res;
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const DebugStatement* ds) {
  return new BlockingAssign(
    new Identifier("__task_id"), 
    new Number(Bits(std::numeric_limits<T>::digits, task_index_++))
  );
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const FflushStatement* fs) {
  return new BlockingAssign(
    new Identifier("__task_id"), 
    new Number(Bits(std::numeric_limits<T>::digits, task_index_++))
  );
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const FinishStatement* fs) {
  return new BlockingAssign(
    new Identifier("__task_id"), 
    new Number(Bits(std::numeric_limits<T>::digits, task_index_++))
  );
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const FseekStatement* fs) {
  return new BlockingAssign(
    new Identifier("__task_id"), 
    new Number(Bits(std::numeric_limits<T>::digits, task_index_++))
  );
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const GetStatement* gs) {
  return new BlockingAssign(
    new Identifier("__task_id"), 
    new Number(Bits(std::numeric_limits<T>::digits, task_index_++))
  );
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const PutStatement* ps) {
  return new BlockingAssign(
    new Identifier("__task_id"), 
    new Number(Bits(std::numeric_limits<T>::digits, task_index_++))
  );
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const RestartStatement* rs) {
  return new BlockingAssign(
    new Identifier("__task_id"), 
    new Number(Bits(std::numeric_limits<T>::digits, task_index_++))
  );
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const RetargetStatement* rs) {
  return new BlockingAssign(
    new Identifier("__task_id"), 
    new Number(Bits(std::numeric_limits<T>::digits, task_index_++))
  );
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const SaveStatement* ss) {
  return new BlockingAssign(
    new Identifier("__task_id"), 
    new Number(Bits(std::numeric_limits<T>::digits, task_index_++))
  );
}

template <size_t V, typename A, typename T>
inline Statement* TextMangle<V,A,T>::build(const YieldStatement* ys) {
  return new BlockingAssign(
    new Identifier("__task_id"), 
    new Number(Bits(std::numeric_limits<T>::digits, task_index_++))
  );
}

template <size_t V, typename A, typename T>
inline Expression* TextMangle<V,A,T>::get_table_range(const Identifier* r, const Identifier* i) {
  // Look up r in the variable table
  const auto titr = vt_->find(r);
  assert(titr != vt_->end());

  // Start with an expression for where this variable begins in the variable table
  Expression* idx = new Number(Bits(std::numeric_limits<T>::digits, titr->second.begin));

  // Now iterate over the arity of r and compute a symbolic expression 
  auto mul = titr->second.elements;
  auto iitr = i->begin_dim();
  for (auto a : Evaluate().get_arity(titr->first)) {
    mul /= a;
    idx = new BinaryExpression(
      idx,
      BinaryExpression::Op::PLUS,
      new BinaryExpression(
        (*iitr++)->clone(),
        BinaryExpression::Op::TIMES,
        new Number(Bits(std::numeric_limits<T>::digits, mul*titr->second.words_per_element))
      )
    );
  }
  return new RangeExpression(idx, RangeExpression::Type::PLUS, new Number(Bits(std::numeric_limits<T>::digits, titr->second.words_per_element)));
}

} // namespace cascade::avmm

#endif
