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

#include "verilog/analyze/module_info.h"

#include <unordered_set>
#include "verilog/ast/ast.h"
#include "verilog/analyze/read_set.h"
#include "verilog/analyze/resolve.h"
#include "verilog/program/elaborate.h"
#include "verilog/program/inline.h"

using namespace std;

namespace cascade {

ModuleInfo::ModuleInfo(const ModuleDeclaration* md) : Visitor() {
  md_ = const_cast<ModuleDeclaration*>(md);
  lhs_ = false;
}

void ModuleInfo::invalidate() {
  if (md_->next_update_ == 0) {
    return;
  }
  
  // NOTE: It's important that we don't *just* call clear here. There's a
  // potential for a pretty large soft-leak as we inline the user's program
  // from leaf to root and each node's module info comes to encompass
  // everything below it.

  md_->next_update_ = 0;
  unordered_set<const Identifier*>().swap(md_->locals_);
  unordered_set<const Identifier*>().swap(md_->inputs_);
  unordered_set<const Identifier*>().swap(md_->outputs_);
  unordered_set<const Identifier*>().swap(md_->stateful_);
  unordered_set<const Identifier*>().swap(md_->implied_wires_);
  unordered_set<const Identifier*>().swap(md_->implied_latches_);
  unordered_set<const Identifier*>().swap(md_->reads_);
  unordered_set<const Identifier*>().swap(md_->writes_);
  ModuleDeclaration::ParamSet().swap(md_->named_params_);
  Vector<const Identifier*>().swap(md_->ordered_params_);
  ModuleDeclaration::PortSet().swap(md_->named_ports_);
  Vector<const Identifier*>().swap(md_->ordered_ports_);
  ModuleDeclaration::ConnMap().swap(md_->connections_);
  ModuleDeclaration::ChildMap().swap(md_->children_);
  md_->uses_mixed_triggers_ = false;
  md_->clocks_ = 0;
  md_->uses_yield_ = false; 
}

bool ModuleInfo::is_declaration() {
  return md_->get_parent() == nullptr;
}

bool ModuleInfo::is_instantiated() {
  return md_->get_parent() != nullptr;
}

const Identifier* ModuleInfo::id() {
  auto* p = md_->get_parent();
  if (p == nullptr) {
    return nullptr;
  }
  assert(p->is(Node::Tag::module_instantiation));
  auto* mi = static_cast<ModuleInstantiation*>(p);
  return mi->get_iid();
}

bool ModuleInfo::is_local(const Identifier* id) {
  refresh();
  const auto* r = Resolve().get_resolution(id);
  return (r == nullptr) ? false : (md_->locals_.find(r) != md_->locals_.end());
}

bool ModuleInfo::is_input(const Identifier* id) {
  refresh();
  const auto* r = Resolve().get_resolution(id);
  return (r == nullptr) ? false : (md_->inputs_.find(r) != md_->inputs_.end());
}

bool ModuleInfo::is_stateful(const Identifier* id) {
  refresh();
  const auto* r = Resolve().get_resolution(id);
  return (r == nullptr) ? false : (md_->stateful_.find(r) != md_->stateful_.end());

}

bool ModuleInfo::is_volatile(const Identifier* id) {
  // Stateless elements cannot be volatile
  if (!is_stateful(id)) {
    return false;
  }
  // Grab this variable's annotations. If it's stateful, it must be resolvable.
  const auto* r = Resolve().get_resolution(id);
  assert(r != nullptr);
  const auto* attrs = static_cast<const Declaration*>(r->get_parent())->get_attrs();
  // Annotations override defaults
  if (attrs->find("non_volatile")) {
    return false;
  } 
  if (attrs->find("volatile")) {
    return true;
  }
  // If this program uses yield, we're volatile by default, otherwise not
  return uses_yield();
}

bool ModuleInfo::is_implied_wire(const Identifier* id) {
  refresh();
  const auto* r = Resolve().get_resolution(id);
  return (r == nullptr) ? false : (md_->implied_wires_.find(r) != md_->implied_wires_.end());
}

bool ModuleInfo::is_implied_latch(const Identifier* id) {
  refresh();
  const auto* r = Resolve().get_resolution(id);
  return (r == nullptr) ? false : (md_->implied_latches_.find(r) != md_->implied_latches_.end());
}

bool ModuleInfo::is_output(const Identifier* id) {
  refresh();
  const auto* r = Resolve().get_resolution(id);
  return (r == nullptr) ? false : (md_->outputs_.find(r) != md_->outputs_.end());
}

bool ModuleInfo::is_read(const Identifier* id) {
  refresh();
  const auto* r = Resolve().get_resolution(id);
  return (r == nullptr) ? false : (md_->reads_.find(r) != md_->reads_.end());
}

bool ModuleInfo::is_write(const Identifier* id) {
  refresh();
  const auto* r = Resolve().get_resolution(id);
  return (r == nullptr) ? false : (md_->writes_.find(r) != md_->writes_.end());
}

bool ModuleInfo::is_child(const Identifier* id) {
  refresh();
  const auto* r = Resolve().get_resolution(id);
  return (r == nullptr) ? false : (md_->children_.find(r) != md_->children_.end());
}

bool ModuleInfo::uses_mixed_triggers() {
  refresh();
  return md_->uses_mixed_triggers_;
}

bool ModuleInfo::uses_multiple_clocks() {
  refresh();
  return md_->clocks_ > 1;
}

bool ModuleInfo::uses_yield() {
  refresh();
  return md_->uses_yield_;
}

const unordered_set<const Identifier*>& ModuleInfo::locals() {
  refresh();
  return md_->locals_;
}

const unordered_set<const Identifier*>& ModuleInfo::inputs() {
  refresh();
  return md_->inputs_;
}

const unordered_set<const Identifier*>& ModuleInfo::outputs() {
  refresh();
  return md_->outputs_;
}

const unordered_set<const Identifier*>& ModuleInfo::stateful() {
  refresh();
  return md_->stateful_;
}

const unordered_set<const Identifier*>& ModuleInfo::implied_wires() {
  refresh();
  return md_->implied_wires_;
}

const unordered_set<const Identifier*>& ModuleInfo::implied_latches() {
  refresh();
  return md_->implied_latches_;
}

const unordered_set<const Identifier*>& ModuleInfo::reads() {
  refresh();
  return md_->reads_;
}

const unordered_set<const Identifier*>& ModuleInfo::writes() {
  refresh();
  return md_->writes_;
}

const unordered_map<const Identifier*, const ModuleDeclaration*>& ModuleInfo::children() {
  refresh();
  return md_->children_;
}

const unordered_set<const Identifier*, HashId, EqId>& ModuleInfo::named_params() {
  refresh();
  return md_->named_params_;
}

const Vector<const Identifier*>& ModuleInfo::ordered_params() {
  refresh();
  return md_->ordered_params_;
}

const unordered_set<const Identifier*, HashId, EqId>& ModuleInfo::named_ports() {
  refresh();
  return md_->named_ports_;
}

const Vector<const Identifier*>& ModuleInfo::ordered_ports() {
  refresh();
  return md_->ordered_ports_;
}

const unordered_map<const Identifier*, unordered_map<const Identifier*, const Expression*>>& ModuleInfo::connections() {
  refresh();
  return md_->connections_;
}

void ModuleInfo::named_parent_conn(const ModuleInstantiation* mi, const PortDeclaration* pd) {
  for (auto i = mi->begin_ports(), ie = mi->end_ports(); i != ie; ++i) {
    // This is a named connection, so explicit port should never be null.
    // Typechecking should enforce this.
    assert((*i)->is_non_null_exp());
    // Nothing to do for an empty named connection
    if ((*i)->is_null_imp()) {
      continue;
    }
    // Nothing to do if this isn't the right port
    const auto* r = Resolve().get_resolution((*i)->get_exp()); 
    if (r != pd->get_decl()->get_id()) {
      continue;
    }

    // Flag this variable as either a read or a write and return
    switch (pd->get_type()) {
      case PortDeclaration::Type::INPUT:
        record_local_write(r);
        return;
      case PortDeclaration::Type::OUTPUT:
        record_local_read(r);
        return;
      default:
        record_local_read(r);
        record_local_write(r);
        return;
    }
  }
}

void ModuleInfo::ordered_parent_conn(const ModuleInstantiation* mi, const PortDeclaration* pd, size_t idx) {
  // Do nothing if this port doesn't appear in mi's port list
  if (idx >= mi->size_ports()) {
    return;
  }
  auto* p = mi->get_ports(idx);

  // This is an ordered connection, so explicit port should always be null.
  // Typechecking should enforce this.
  assert(p->is_null_exp());
  // Nothing to do for an empty ordered connection
  if (p->is_null_imp()) {
    return;
  }

  // Flag this variable as either a read or a write
  const auto* r = pd->get_decl()->get_id();
  switch (pd->get_type()) {
    case PortDeclaration::Type::INPUT:
      record_local_write(r);
      break;
    case PortDeclaration::Type::OUTPUT:
      record_local_read(r);
      break;
    default:
      record_local_read(r);
      record_local_write(r);
      break;
  }
}

void ModuleInfo::named_child_conns(const ModuleInstantiation* mi) {
  unordered_map<const Identifier*, const Expression*> conn;
  for (auto i = mi->begin_ports(), ie = mi->end_ports(); i != ie; ++i) {
    // This is a named connection, so explicit port should never be null.
    // Typechecking should enforce this.
    assert((*i)->is_non_null_exp());
    // Nothing to do for an empty named connection
    if ((*i)->is_null_imp()) {
      continue;
    }

    // Grab the declaration that this explicit port corresponds to
    const auto* r = Resolve().get_resolution((*i)->get_exp()); 
    assert(r != nullptr);
    // Connect the variable tot he expression in this module
    conn.insert(make_pair(r, (*i)->get_imp()));

    // Anything that appears in a module's port list must be declared as
    // a port. Typechecking should enforce this.
    assert(r->get_parent()->get_parent()->is(Node::Tag::port_declaration));
    auto* pd = static_cast<const PortDeclaration*>(r->get_parent()->get_parent());

    switch (pd->get_type()) {
      case PortDeclaration::Type::INPUT:
        record_external_read(r);
        break;
      case PortDeclaration::Type::OUTPUT:
        record_external_write(r);
        break;
      default:
        record_external_read(r);
        record_external_write(r);
        break;
    }
  }
  md_->connections_.insert(make_pair(mi->get_iid(), conn));
}

void ModuleInfo::ordered_child_conns(const ModuleInstantiation* mi) {
  unordered_map<const Identifier*, const Expression*> conn;

  auto itr = Elaborate().get_elaboration(mi)->begin_items();
  for (size_t i = 0, ie = mi->size_ports(); i < ie; ++i) {
    const auto* p = mi->get_ports(i);
    // This is an ordered connection, so explicit port should always be null.
    // Typechecking should enforce this.
    assert(p->is_null_exp());

    // Track to the first port declaration. It's kind of ugly to have to iterate
    // over the entire text of this module every time we refresh, but it's the price
    // we pay for not having to rely on its module info.
    while (!(*itr)->is(Node::Tag::port_declaration)) {
      ++itr;
      assert(itr != Elaborate().get_elaboration(mi)->end_items());
    }
    // Anything that appears in a module's port list must be declared as
    // a port. Typechecking should enforce this.
    auto* pd = static_cast<const PortDeclaration*>(*itr++);

    // Nothing to do for an empty ordered connection
    if (p->is_null_imp()) {
      continue;
    }

    // Grab the declaration that this port corresponds to
    const auto* r = pd->get_decl()->get_id();
    // Connect the variable to the expression in this module
    conn.insert(make_pair(r, p->get_imp()));

    switch (pd->get_type()) {
      case PortDeclaration::Type::INPUT:
        record_external_read(r);
        break;
      case PortDeclaration::Type::OUTPUT:
        record_external_write(r);
        break;
      default:
        record_external_read(r);
        record_external_write(r);
        break;
    }
  }
  md_->connections_.insert(make_pair(mi->get_iid(), conn));
}

void ModuleInfo::named_external_conn(const ModuleInstantiation* mi, const ArgAssign* aa, const Identifier* id) {
  // This method should never be called for an empty named connection
  assert(aa->is_non_null_exp());

  // Grab the declaration that this explicit port corresponds to
  const auto* r = Resolve().get_resolution(aa->get_exp()); 
  assert(r != nullptr);

  // If this module hasn't been inlined, then we can extract the port type for
  // this variable from its declaration
  if (!Inline().is_inlined(mi)) {
    assert(r->get_parent()->get_parent()->is(Node::Tag::port_declaration));
    auto* pd = static_cast<const PortDeclaration*>(r->get_parent()->get_parent());

    switch (pd->get_type()) {
      case PortDeclaration::Type::INPUT:
        record_local_read(id);
        break;
      case PortDeclaration::Type::OUTPUT:
        record_local_write(id);
        break;
      default:
        record_local_read(id);
        record_local_write(id);
        break;
    }
  } 
  // Otherwise, we'll need to extract it from the inlining annotation we generated
  else {
    assert(r->get_parent()->is_subclass_of(Node::Tag::declaration));
    auto* d = static_cast<const Declaration*>(r->get_parent());
    const auto* inl = d->get_attrs()->get<String>("__inline");
    assert(inl != nullptr);

    if (inl->eq("input")) {
      record_local_read(id);
    } else if (inl->eq("output")) {
      record_local_write(id);
    } else {
      record_local_read(id);
      record_local_write(id);
    }
  }
}

void ModuleInfo::ordered_external_conn(const ModuleInstantiation* mi, const ArgAssign* aa, const Identifier* id) {
  // Nothing to do for modules which haven't been elaborated yet.
  if (!Elaborate().is_elaborated(mi)) {
    return;
  }
  // If this module hasn't been inlined, we'll have to scan through its declarations one at a time
  // to determine the type of this port connection
  if (!Inline().is_inlined(mi)) {
    auto itr = Elaborate().get_elaboration(mi)->begin_items();
    for (size_t i = 0, ie = (*mi->find_ports(aa) - *mi->begin_ports()); i <= ie; ++i) {
      while (!(*itr)->is(Node::Tag::port_declaration)) {
        ++itr;
        assert(itr != Elaborate().get_elaboration(mi)->end_items());
      }
      if (i != ie) {
        ++itr;
      } 
    }
    const auto* pd = static_cast<const PortDeclaration*>(*itr);
    switch (pd->get_type()) {
      case PortDeclaration::Type::INPUT:
        record_external_read(id);
        break;
      case PortDeclaration::Type::OUTPUT:
        record_external_write(id);
        break;
      default:
        record_external_read(id);
        record_external_write(id);
        break;
    }
  } 
  // Otherwise, we'll need to pick out port declarations by inspecting inlining annotations
  else {
    auto itr = Inline().get_source(mi)->front_clauses()->get_then()->begin_items();
    for (size_t i = 0, ie = (*mi->find_ports(aa) - *mi->begin_ports()); i <= ie; ++i) {
      while ((!(*itr)->is(Node::Tag::net_declaration) && !(*itr)->is(Node::Tag::reg_declaration)) ||
             (static_cast<const Declaration*>(*itr)->get_attrs()->get<String>("__inline") == nullptr)) {
        ++itr;
        assert(itr != Inline().get_source(mi)->front_clauses()->get_then()->end_items());
      }
      if (i != ie) {
        ++itr;
      } 
    }
    const auto* d = static_cast<const Declaration*>(*itr);
    const auto* inl = d->get_attrs()->get<String>("__inline");
    assert(inl != nullptr);

    if (inl->eq("input")) {
      record_local_read(id);
    } else if (inl->eq("output")) {
      record_local_write(id);
    } else {
      record_local_read(id);
      record_local_write(id);
    }
  }
}

void ModuleInfo::record_local_read(const Identifier* id) {
  if (md_->reads_.find(id) == md_->reads_.end()) {
    md_->reads_.insert(id);
  }
}

void ModuleInfo::record_external_read(const Identifier* id) {
  if (md_->reads_.find(id) == md_->reads_.end()) {
    md_->reads_.insert(id);
  }
}

void ModuleInfo::record_local_write(const Identifier* id) {
  if (md_->writes_.find(id) == md_->writes_.end()) {
    md_->writes_.insert(id);
  }
}

void ModuleInfo::record_external_write(const Identifier* id) {
  if (md_->writes_.find(id) == md_->writes_.end()) {
    md_->writes_.insert(id);
  }
}

void ModuleInfo::record_external_use(const Identifier* id) {
  for (auto i = Resolve().use_begin(id), ie = Resolve().use_end(id); i != ie; ++i) {
    // Nothing to do for expressions which aren't identifiers
    if (!(*i)->is(Node::Tag::identifier)) {
      continue;
    }
    const auto* eid = static_cast<const Identifier*>(*i);
    // Nothing to do if this variable appears in this module
    if (Resolve().get_parent(eid) == md_) {
      continue;
    }

    // Grab a pointer to this identifier's parent
    const auto* p = eid->get_parent();
    assert(p != nullptr);
    // Identifiers in ArgAssigns can be reads, writes, or neither
    if (p->is(Node::Tag::arg_assign)) {
      // Nothing to do if this is an explicit port
      const auto* aa = static_cast<const ArgAssign*>(p);
      if (aa->get_exp() == eid) {
        continue;
      }
      const auto* pp = aa->get_parent();
      assert(pp != nullptr);
      // Nothing to do if this ArgAssign appears in a ModuleDeclaration 
      if (pp->is(Node::Tag::module_declaration)) {
        continue;
      }
      if (pp->is(Node::Tag::module_instantiation)) {
        const auto* mi = static_cast<const ModuleInstantiation*>(pp);
        if (mi->find_params(aa) != mi->end_params()) {
          record_local_read(id);
        } else if (mi->uses_named_ports()) {
          named_external_conn(mi, aa, id);
        } else {
          ordered_external_conn(mi, aa, id);
        }
      }
    }    
    // Identifiers on the left hand side of VariableAssigns are writes Due to
    // AST refactorings, we need to check the three types that used to contain
    // VariableAssign's as well.
    else if (p->is(Node::Tag::variable_assign) && static_cast<const VariableAssign*>(p)->get_lhs() == eid) {
      record_local_write(id);
    } else if (p->is(Node::Tag::continuous_assign) && static_cast<const ContinuousAssign*>(p)->get_lhs() == eid) {
      record_local_write(id);
    } else if (p->is(Node::Tag::blocking_assign) && static_cast<const BlockingAssign*>(p)->get_lhs() == eid) {
      record_local_write(id);
    } else if (p->is(Node::Tag::nonblocking_assign) && static_cast<const NonblockingAssign*>(p)->get_lhs() == eid) {
      record_local_write(id);
    } 
    // Everything else is a read
    else {
      record_local_read(id);
    }
  }
}

void ModuleInfo::visit(const Attributes* as) {
  // Does nothing. There's nothing for us in here other than the opportunity to
  // blow a ton of time looking up variables that we can't resolve.
  (void) as;
  return;
}

void ModuleInfo::visit(const Identifier* i) {
  // Do nothing if this is a local or unresolvable variable
  const auto* r = Resolve().get_resolution(i);
  if (r == nullptr || (md_->locals_.find(r) != md_->locals_.end())) {
    return;
  }
  // This variable must be external, record read/write
  if (lhs_) {
    record_external_read(r);
  } else {
    record_external_write(r);
  }
}

void ModuleInfo::visit(const CaseGenerateConstruct* cgc) {
  cgc->accept_cond(this);
  if (Elaborate().is_elaborated(cgc)) {
    Elaborate().get_elaboration(cgc)->accept(this);
  }
}

void ModuleInfo::visit(const IfGenerateConstruct* igc) {
  for (auto i = igc->begin_clauses(), ie = igc->end_clauses(); i != ie; ++i) {
    (*i)->accept_if(this);
  }
  if (Elaborate().is_elaborated(igc)) {
    Elaborate().get_elaboration(igc)->accept(this);
  }
}

void ModuleInfo::visit(const LoopGenerateConstruct* lgc) {
  lgc->accept_init(this);
  lgc->accept_cond(this);
  lgc->accept_update(this);
  if (Elaborate().is_elaborated(lgc)) {
    for (auto* b : Elaborate().get_elaboration(lgc)) {
      b->accept(this);
    }
  }
}

void ModuleInfo::visit(const ContinuousAssign* ca) {
  lhs_ = true;
  ca->accept_lhs(this);
  lhs_ = false;
  ca->accept_rhs(this);
}

void ModuleInfo::visit(const GenvarDeclaration* gd) {
  md_->locals_.insert(gd->get_id());   
  // Nothing external should reference this
}

void ModuleInfo::visit(const LocalparamDeclaration* ld) {
  md_->locals_.insert(ld->get_id());   
  record_external_use(ld->get_id());
}

void ModuleInfo::visit(const NetDeclaration* nd) {
  md_->locals_.insert(nd->get_id());   
  record_external_use(nd->get_id());
}

void ModuleInfo::visit(const ParameterDeclaration* pd) {
  md_->locals_.insert(pd->get_id());   
  md_->named_params_.insert(pd->get_id());
  md_->ordered_params_.push_back(pd->get_id());
  record_external_use(pd->get_id());
}

void ModuleInfo::visit(const RegDeclaration* rd) {
  md_->locals_.insert(rd->get_id());   
  record_external_use(rd->get_id());
}

void ModuleInfo::visit(const ModuleInstantiation* mi) {
  // This module has been inlined, continue descending through here rather than
  // examine its connections.
  if (Inline().is_inlined(mi)) {
    return Inline().get_source(mi)->accept(this);
  }

  // Descend on implicit ports. These are syntactically part of this module.
  for (auto i = mi->begin_params(), ie = mi->end_params(); i != ie; ++i) {
    (*i)->accept_imp(this);
  }
  for (auto i = mi->begin_ports(), ie = mi->end_ports(); i != ie; ++i) {
    (*i)->accept_imp(this);
  }

  // Nothing else to do if this module wasn't instantiated.
  if (!Elaborate().is_elaborated(mi)) {
    return;
  }
  // Otherwise, descend on port bindings to establish connections and record
  // this child.
  if (mi->uses_named_ports()) {
    named_child_conns(mi);
  } else {
    ordered_child_conns(mi);
  }
  md_->children_.insert(make_pair(mi->get_iid(), Elaborate().get_elaboration(mi)));
}

void ModuleInfo::visit(const PortDeclaration* pd) {
  // Record input or output port
  switch(pd->get_type()) {
    case PortDeclaration::Type::INPUT:
      md_->inputs_.insert(pd->get_decl()->get_id());
      break;
    case PortDeclaration::Type::OUTPUT:
      md_->outputs_.insert(pd->get_decl()->get_id());
      break;
    default:
      md_->inputs_.insert(pd->get_decl()->get_id());
      md_->outputs_.insert(pd->get_decl()->get_id());
      break;
  }
  // Record port name and ordering information
  md_->named_ports_.insert(pd->get_decl()->get_id());
  md_->ordered_ports_.push_back(pd->get_decl()->get_id());
  
  // Descend on declaration
  pd->accept_decl(this);

  // Nothing else to do if this is a declaration
  if (is_declaration()) {
    return;
  }
  // Otherwise, update read/write information for this connection
  assert(md_->get_parent()->is(Node::Tag::module_instantiation));
  auto* mi = static_cast<const ModuleInstantiation*>(md_->get_parent());
  if (mi->uses_named_ports()) {
    named_parent_conn(mi, pd);
  } else {
    ordered_parent_conn(mi, pd, md_->ordered_ports_.size()-1);
  }
}

void ModuleInfo::visit(const BlockingAssign* ba) {
  lhs_ = true;
  ba->accept_lhs(this);
  lhs_ = false;
  ba->accept_rhs(this);
}

void ModuleInfo::visit(const NonblockingAssign* na) {
  lhs_ = true;
  na->accept_lhs(this);
  lhs_ = false;
  na->accept_rhs(this);
}

void ModuleInfo::visit(const EventControl* ec) {
  Visitor::visit(ec);

  // Check for the presence of both pos/neg edge and edge, and count clocks
  auto edge = false;
  auto var = false;
  for (auto i = ec->begin_events(), ie = ec->end_events(); i != ie; ++i) {
    switch ((*i)->get_type()) {
      case Event::Type::POSEDGE:
      case Event::Type::NEGEDGE:
        ++md_->clocks_;
        edge = true;
        break;
      case Event::Type::EDGE:
        var = true;
        break;
      default:
        assert(false);
        break;
    } 
  }
  md_->uses_mixed_triggers_ = md_->uses_mixed_triggers_ || (edge && var);
}

void ModuleInfo::visit(const VariableAssign* va) {
  lhs_ = true;
  va->accept_lhs(this);
  lhs_ = false;
  va->accept_rhs(this);
}

void ModuleInfo::refresh() {
  const auto size = md_->size_items();
  if (md_->next_update_ == size) {
    return;
  }

  Node* er = nullptr;
  for (er = md_; er->get_parent() != nullptr; er = er->get_parent());
  md_->uses_yield_ = YieldCheck().run(er);

  for (; md_->next_update_ < size; ++md_->next_update_) {
    md_->get_items(md_->next_update_)->accept(this);
  }
  for (auto* l : md_->locals_) {
    if (!l->get_parent()->is(Node::Tag::reg_declaration)) {
      continue;
    }
    switch (get_type(l)) {
      case Type::REG:
        md_->stateful_.insert(l);
        break;
      case Type::IMPLIED_WIRE:
        md_->implied_wires_.insert(l);
        break;
      case Type::IMPLIED_LATCH:
        md_->stateful_.insert(l);
        md_->implied_latches_.insert(l);
        break;
      default:
        assert(false);
        break;
    }
  }
}

ModuleInfo::Type ModuleInfo::get_type(const Identifier* id) {
  assert(id->get_parent()->is(Node::Tag::reg_declaration));
  const auto* rd = static_cast<const RegDeclaration*>(id->get_parent());

  // A register which is intiialized with an fopen can't be a wire
  if (rd->is_non_null_val() && rd->get_val()->is(Node::Tag::fopen_expression)) {
    return Type::REG;
  }

  const TimingControlStatement* tcs_use = nullptr;
  for (auto i = Resolve().use_begin(id), ie = Resolve().use_end(id); i != ie; ++i) {
    if (!(*i)->is(Node::Tag::identifier)) {
      continue;
    }
    const auto* id = static_cast<const Identifier*>(*i);

    switch(id->get_parent()->get_tag()) {
      // Regs which appear in get statements can't be wires
      case Node::Tag::get_statement:
        if (static_cast<const GetStatement*>(id->get_parent())->get_var() == id) {
          return Type::REG;
        }
        break;
      // Anything which is the target of a non-blocking assignment can't be a wire
      case Node::Tag::nonblocking_assign: {
        const auto* na = static_cast<const NonblockingAssign*>(id->get_parent());
        if (na->find_lhs(id) != na->end_lhs()) {
          return Type::REG;
        }
        break;
      }
      // The hard case: variables which are the targets of blocking assigns
      case Node::Tag::blocking_assign: {
        const auto* ba = static_cast<const BlockingAssign*>(id->get_parent());
        if (ba->find_lhs(id) == ba->end_lhs()) {
          break;
        }

        // Record the dependencies of this assignment 
        ReadSet rs1(ba->get_rhs());
        unordered_set<const Expression*> deps(rs1.begin(), rs1.end());

        // Walk up the AST until we find the enclosing timing control
        // statement.  Add dependencies from conditional and case statements.
        const auto* n = ba->get_parent();
        for (; !n->is(Node::Tag::timing_control_statement); n = n->get_parent()) {
          if (n->is(Node::Tag::conditional_statement)) {
            ReadSet rs2(static_cast<const ConditionalStatement*>(n)->get_if());
            deps.insert(rs2.begin(), rs2.end());
          } else if (n->is(Node::Tag::case_statement)) {
            ReadSet rs3(static_cast<const CaseStatement*>(n)->get_cond());
            deps.insert(rs3.begin(), rs3.end());
          } else if (n->is(Node::Tag::initial_construct)) {
            return Type::REG;
          }
        }
        unordered_set<const Identifier*> id_deps;
        for (const auto* d : deps) {
          if (d->is(Node::Tag::identifier)) {
            id_deps.insert(Resolve().get_resolution(static_cast<const Identifier*>(d)));
          }
        }
        const auto* tcs = static_cast<const TimingControlStatement*>(n);
        const auto* ec = static_cast<const EventControl*>(tcs->get_ctrl());

        // Walk along the event control and collect its triggers. If we see an
        // edge trigger this can't be a wire.
        unordered_set<const Identifier*> trigs;
        for (auto j = ec->begin_events(), je = ec->end_events(); j != je; ++j) {
          if ((*j)->get_type() != Event::Type::EDGE) {
            return Type::REG;
          }
          if ((*j)->get_expr()->is(Node::Tag::identifier)) {
            trigs.insert(Resolve().get_resolution(static_cast<const Identifier*>((*j)->get_expr())));
          }
        }

        // If we're here it's because this is a value-triggered block. If we've
        // been to a different one already, or we depend on a value that doesn't
        // appear in its trigger list, we can't be a register.
        if ((tcs_use != nullptr) && (tcs != tcs_use)) {
          return Type::IMPLIED_LATCH;
        }
        tcs_use = tcs;
        for (const auto* d : id_deps) {
          if (trigs.find(d) == trigs.end()) {
            return Type::IMPLIED_LATCH;
          }
        }
        break;
      }
      default:
        break;
    }
  }

  // If control has reached here, and we saw at least one use of a wire-style
  // assignment, then this is a wire. Otherwise, this is a register.
  return (tcs_use != nullptr) ? Type::IMPLIED_WIRE : Type::REG;
}

ModuleInfo::YieldCheck::YieldCheck() : Visitor() { }

bool ModuleInfo::YieldCheck::run(const Node* n) {
  res_ = false;
  n->accept(this);
  return res_;
}

void ModuleInfo::YieldCheck::visit(const CaseGenerateConstruct* cgc) {
  if (Elaborate().is_elaborated(cgc)) {
    Elaborate().get_elaboration(cgc)->accept(this);
  }
}

void ModuleInfo::YieldCheck::visit(const IfGenerateConstruct* igc) {
  if (Elaborate().is_elaborated(igc)) {
    Elaborate().get_elaboration(igc)->accept(this);
  }
}

void ModuleInfo::YieldCheck::visit(const LoopGenerateConstruct* lgc) {
  if (Elaborate().is_elaborated(lgc)) {
    for (auto* b : Elaborate().get_elaboration(lgc)) {
      b->accept(this);
    }
  }
}

void ModuleInfo::YieldCheck::visit(const ModuleInstantiation* mi) {
  if (Inline().is_inlined(mi)) {
    Inline().get_source(mi)->accept(this);
  } else if (Elaborate().is_elaborated(mi)) {
    Elaborate().get_elaboration(mi)->accept(this); 
  }
}

void ModuleInfo::YieldCheck::visit(const YieldStatement* ys) {
  res_ = true;
}

} // namespace cascade
