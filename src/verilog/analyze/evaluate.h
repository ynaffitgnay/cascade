// Copyright 2017-2018 VMware, Inc.
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

#ifndef CASCADE_SRC_VERILOG_ANALYZE_EVALUATE_H
#define CASCADE_SRC_VERILOG_ANALYZE_EVALUATE_H

#include <cassert>
#include <utility>
#include "src/base/bits/bits.h"
#include "src/verilog/analyze/resolve.h"
#include "src/verilog/ast/ast.h"
#include "src/verilog/ast/visitors/editor.h"

namespace cascade {

// This class implements the semantics of expression evaluation as described in
// the 2005 Verilog spec. This class requires up-to-date resolution decorations
// (see resolve.h) to function correctly.

class Evaluate : public Editor {
  public:
    ~Evaluate() override = default;

    // Returns the bit-width of the values in an expression. Returns the same
    // value for scalars and arrays.
    size_t get_width(const Expression* e);
    // Returns true if the values in an expression are signed or unsigned.
    // Returns the same for scalars and arrays.
    bool get_signed(const Expression* e);

    // Returns the bit value of an expression. Invoking this method on an expression
    // which evaluates to an array returns the first element of that array.
    const Bits& get_value(const Expression* e);
    // Returns the array value of an identifier. Invoking this method on an identifier
    // which evaluates to a scalar returns a single element array.
    const std::vector<Bits>& get_array_value(const Identifier* i);
    // Returns upper and lower values for ranges, get_value() twice otherwise.
    std::pair<size_t, size_t> get_range(const Expression* e);

    // Sets the value a word slice within id. Invoking this method on an
    // unresolvable id or one which refers to an array is undefined.
    template <typename B>
    void assign_word(const Identifier* id, size_t n, B b);
    // Sets the value of id to val. Invoking this method on an unresolvable id
    // or one which refers to an array is undefined.
    void assign_value(const Identifier* id, const Bits& val);
    // Sets the value of id to val. Invoking this method on an unresolvable id
    // or one which refers to an array subscript is undefined.
    void assign_array_value(const Identifier* id, const std::vector<Bits>& val);

    // Invalidates bits, size, and type for this expression and the
    // sub-expressions that it consists of.
    void invalidate(const Expression* e);

  private:
    // Editor Interface:
    void edit(BinaryExpression* be) override;
    void edit(ConditionalExpression* ce) override;
    void edit(NestedExpression* ne) override;
    void edit(Concatenation* c) override;
    void edit(Identifier* id) override;
    void edit(MultipleConcatenation* mc) override;
    void edit(Number* n) override;
    void edit(String* s) override;
    void edit(UnaryExpression* ue) override;

    // Helper Methods:
    //
    // Returns the root of the expression tree containing e. See implementation
    // notes for what counts as a boundary between trees.
    const Node* get_root(const Expression* e) const;
    // Initializes the bit value associated with an identifier using the rules
    // of self- and context- determination to determine bit-width and sign.
    void init(Expression* e);
    // Updates the needs_update_ flag for this identifier and its dependencies.
    void flag_changed(const Identifier* id);
    // Finds the bit_value associated with a potentially subscripted identifier
    // returns a pointer to the last ununsed element in its dimensions so that
    // further operations may use it to compute a slice.
    std::pair<size_t, const Many<Expression>::const_iterator> deref(const Identifier* r, const Identifier* i);

    // Invalidates bit, size, and type info for the expressions in this subtree
    struct Invalidate : public Editor {
      ~Invalidate() override = default;
      void edit(BinaryExpression* be) override;
      void edit(ConditionalExpression* ce) override;
      void edit(NestedExpression* ne) override;
      void edit(Concatenation* c) override;
      void edit(Identifier* id) override;
      void edit(MultipleConcatenation* mc) override;
      void edit(Number* n) override;
      void edit(String* s) override;
      void edit(UnaryExpression* ue) override;
      void edit(GenvarDeclaration* gd) override;
      void edit(IntegerDeclaration* id) override;
      void edit(LocalparamDeclaration* ld) override;
      void edit(NetDeclaration* nd) override; 
      void edit(ParameterDeclaration* pd) override;
      void edit(RegDeclaration* rd) override;
      void edit(VariableAssign* va) override;
    };
    // Uses the self-determination to allocate bits, sizes, and types.
    struct SelfDetermine : public Editor {
      ~SelfDetermine() override = default;
      void edit(BinaryExpression* be) override;
      void edit(ConditionalExpression* ce) override;
      void edit(NestedExpression* ne) override;
      void edit(Concatenation* c) override;
      void edit(Identifier* id) override;
      void edit(MultipleConcatenation* mc) override;
      void edit(Number* n) override;
      void edit(String* s) override;
      void edit(UnaryExpression* ue) override;
      void edit(GenvarDeclaration* gd) override;
      void edit(IntegerDeclaration* id) override;
      void edit(LocalparamDeclaration* ld) override;
      void edit(NetDeclaration* nd) override; 
      void edit(ParameterDeclaration* pd) override;
      void edit(RegDeclaration* rd) override;
      void edit(VariableAssign* va) override;
    };
    // Propagates bit-width for context determined operators.
    struct ContextDetermine : public Editor {
      ~ContextDetermine() override = default;
      void edit(BinaryExpression* be) override;
      void edit(ConditionalExpression* ce) override;
      void edit(NestedExpression* ne) override;
      void edit(Concatenation* c) override;
      void edit(Identifier* id) override;
      void edit(MultipleConcatenation* mc) override;
      void edit(Number* n) override;
      void edit(String* s) override;
      void edit(UnaryExpression* ue) override;
      void edit(GenvarDeclaration* gd) override;
      void edit(IntegerDeclaration* id) override;
      void edit(LocalparamDeclaration* ld) override;
      void edit(NetDeclaration* nd) override; 
      void edit(ParameterDeclaration* pd) override;
      void edit(RegDeclaration* rd) override;
      void edit(VariableAssign* va) override;
    };
};

template <typename B>
inline void Evaluate::assign_word(const Identifier* id, size_t n, B b) {
  const auto r = Resolve().get_resolution(id);
  assert(r != nullptr);
  init(const_cast<Identifier*>(r));
  const_cast<Identifier*>(r)->bit_val_[0].write_word<B>(n, b);
  flag_changed(r);
}

} // namespace cascade

#endif
