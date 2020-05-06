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

#include <cstring>
#include <iostream>
#include <signal.h>
#include <string>
#include "cl/cl.h"
#include "target/core/aos/f1/vivado_server.h"

using namespace cascade;
using namespace cascade::cl;
using namespace std;

namespace {

__attribute__((unused)) auto& g = Group::create("Vivado Server Options");
auto& cache = StrArg<string>::create("--cache")
  .usage("<path/to/cache>")
  .description("Path to directory to use as compilation cache")
  .initial("");
//  .initial("/home/centos/src/project_data/cascade-f1/src/target/core/f1/device/");
auto& path = StrArg<string>::create("--path")
  .usage("<path to aws-fpga/.../cl_aos>")
  .description("Prefix of path to F1 custom logic directory")
  .initial("");
//  .initial("/home/centos/src/project_data/cascade-f1/src/target/core/f1/device/builds/");
auto& port = StrArg<uint32_t>::create("--port")
  .usage("<int>")
  .description("Port to run server on")
  .initial(0);
//  .initial(9900);

aos::VivadoServer* vs = nullptr;

void handler(int sig) {
  (void) sig;
  vs->request_stop();
}

} // namespace

int main(int argc, char** argv) {
  // Parse command line:
  Simple::read(argc, argv);

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = ::handler;
  sigaction(SIGINT, &action, nullptr);

  ::vs = new aos::VivadoServer();
  if (!::cache.value().empty()) ::vs->set_cache_path(::cache.value());
  if (!::path.value().empty()) ::vs->set_compile_path(::path.value());
  if (::port.value() != 0) ::vs->set_port(::port.value());

  if (::vs->error()) {
    cout << "Unable to locate core components!" << endl;
  } else {
    ::vs->run();
    ::vs->wait_for_stop();
  }
  delete ::vs;

  cout << "Goodbye!" << endl;
  return 0;
}
