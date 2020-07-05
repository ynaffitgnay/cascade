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

#ifndef CASCADE_SRC_RUNTIME_ISOLATE_H
#define CASCADE_SRC_RUNTIME_ISOLATE_H

#include <unordered_map>
#include <vector>
#include "runtime/ids.h"
#include "verilog/ast/ast.h"
#include "verilog/ast/visitors/builder.h"
#include "verilog/program/inline.h"

namespace cascade {

// This class is responsible for transforming an instantiated module into a
// stand-alone piece of code. This class also enforces useful invariants on the
// form of that code for down-stream passes.
//
// 1. Module instantiations are promoted to top-level input/outputs
// 2. Variables are given globally-unique non-hierarchical names 
// 3. Declarations are moved to the top-most scope.
// 4. Generate regions are flattened into their enclosing scope.
// 5. Generate constructs are flattened into their enclosing scope.
// 6. Attribute annotations which appear inside the module are removed (except for non-volatile).
// 7. The 'ignore' oldest initial blocks are deleted.
// 8. fork/join blocks are replaced by begin/end blocks.
// 9. debug statements have scopes attached to them.

class Isolate : public Builder {
  public:
    // Constructors:
    ~Isolate() override = default;

    // Deterministically tranforms a program variable into a globally-unique
    // variable ID.
    VId isolate(const Identifier* id);
    // Deterministically transforms a module instantiation into a
    // globally-unique module ID.
    MId isolate(const ModuleInstantiation* mi);
    // Transforms an instantiated module from a program into a stand-alone
    // declaration with equivalent semantics.
    ModuleDeclaration* isolate(const ModuleDeclaration* src, int ignore);

  private:
    // Compilation State:
    const ModuleDeclaration* src_;
    int ignore_;
    std::unordered_map<const Identifier*, size_t> symbol_table_;
    std::unordered_map<const Identifier*, size_t> module_table_;
  
    // Builder Interface:
    Attributes* build(const Attributes* as) override;
    Expression* build(const Identifier* i) override;
    ModuleDeclaration* build(const ModuleDeclaration* md) override; 
    ModuleItem* build(const InitialConstruct* ic) override;
    ModuleItem* build(const GenvarDeclaration* gd) override;
    ModuleItem* build(const LocalparamDeclaration* ld) override;
    ModuleItem* build(const ParameterDeclaration* pd) override;
    ModuleItem* build(const RegDeclaration* rd) override;
    ModuleItem* build(const PortDeclaration* pd) override;
    Statement* build(const ParBlock* pb) override;
    Statement* build(const SeqBlock* sb) override;
    Statement* build(const DebugStatement* db) override;

    // Returns a mangled identifier
    Identifier* to_mangled_id(const ModuleInstantiation* mi);
    // Returns a new locally unique identifier
    Identifier* to_local_id(const Identifier* id);
    // Returns a copy of the global identifier that corresponds to this id
    Identifier* to_global_id(const Identifier* id);

    // Returns a module declaration with a mangled id and global io ports
    ModuleDeclaration* get_shell();
    // Generates a list of declarations for local variables
    std::vector<ModuleItem*> get_local_decls();
    // Recursively process a list of items
    template <typename ItemsItr>
    std::vector<ModuleItem*> get_items(ItemsItr begin, ItemsItr end, bool top_level);

    // Replaces an instantiation with assignments
    void replace(std::vector<ModuleItem*>& res, const ModuleInstantiation* mi);
    // Flattens the elaborated branch of a case generate construct
    void flatten(std::vector<ModuleItem*>& res, const CaseGenerateConstruct* cgc);
    // Flattens the elaborated branch of an if generate construct
    void flatten(std::vector<ModuleItem*>& res, const IfGenerateConstruct* igc);
    // Flattens the elaborated branches of a loop generate construct
    void flatten(std::vector<ModuleItem*>& res, const LoopGenerateConstruct* lgc);
    // Flattens a generate block
    void flatten(std::vector<ModuleItem*>& res, const GenerateBlock* gb);
    // Flattens a generate region
    void flatten(std::vector<ModuleItem*>& res, const GenerateRegion* gr);
};

template <typename ItemsItr>
inline std::vector<ModuleItem*> Isolate::get_items(ItemsItr begin, ItemsItr end, bool top_level) {
  std::vector<ModuleItem*> res;
  for (auto mi = begin; mi != end; ++mi) {
    // If this is the top level, we're one step closer to allowing initial constructs
    if (top_level) {
      --ignore_;
    }
    // Ignore Declarations. We handle them at the top level.
    if ((*mi)->is_subclass_of(Node::Tag::declaration)) {
      continue;
    }
    // Flatten generate regions and generate constructs
    else if ((*mi)->is(Node::Tag::generate_region)) {
      auto* gr = static_cast<const GenerateRegion*>(*mi);
      flatten(res, gr);
    } else if ((*mi)->is(Node::Tag::case_generate_construct)) {
      auto* cgc = static_cast<const CaseGenerateConstruct*>(*mi);
      flatten(res, cgc);
    } else if ((*mi)->is(Node::Tag::if_generate_construct)) {
      auto* igc = static_cast<const IfGenerateConstruct*>(*mi);
      flatten(res, igc);
    } else if ((*mi)->is(Node::Tag::loop_generate_construct)) {
      auto* lgc = static_cast<const LoopGenerateConstruct*>(*mi);
      flatten(res, lgc);
    }
    // Either descend on instantiations or replace them with connections
    else if ((*mi)->is(Node::Tag::module_instantiation)) {
      auto* inst = static_cast<const ModuleInstantiation*>(*mi);
      if (Inline().is_inlined(inst)) {
        flatten(res, Inline().get_source(inst));
      } else {
        replace(res, inst); 
      }
    } 
    // Everything else goes through the normal build path. 
    else {
      auto temp = (*mi)->accept(this);
      if (temp != nullptr) {
        res.push_back(temp);
      }
    }
  }
  return res;
}

} // namespace cascade

#endif
