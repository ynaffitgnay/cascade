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

#include "target/core/aos/f1/vivado_server.h"

#include <fstream>
#include <sstream>
#include <sys/file.h>
#include <sys/wait.h>
#include "common/sockserver.h"
#include "common/sockstream.h"
#include "common/system.h"

using namespace std;

namespace cascade::aos {

VivadoServer::VivadoServer() : Thread() { 
  set_cache_path("/tmp/f1");
  set_compile_path("/tmp/f1");
  set_port(9900);

  busy_ = false;
}

VivadoServer& VivadoServer::set_cache_path(const string& path) {
  cache_path_ = path;
  return *this;
}

VivadoServer& VivadoServer::set_compile_path(const string& path) {
  compile_dir_ = path;
  return *this;
}

VivadoServer& VivadoServer::set_port(uint32_t port) {
  port_ = port;
  return *this;
}

bool VivadoServer::error() const {
  // Return true if we can't locate any of the necessary vivado components
  if (System::execute("ls /home/centos/src/project_data/aws-fpga > /dev/null") != 0) {
  	cerr << "Build script expects aws-fpga repo in project_data directory" << endl;
  	return true;
  }
  if (System::execute("ls /opt/Xilinx/Vivado > /dev/null") != 0) {
  	cerr << "Cannot find Vivado installation" << endl;
  	return true;
  }
  return false;
}

void VivadoServer::run_logic() {
  // Initialize thread pool and comilation cache
  init_pool();
  init_cache();

  // Return immediately if we can't create a sockserver
  sockserver server(port_, 8);
  if (server.error()) {
    pool_.stop_now();
    return;
  }

  fd_set master_set;
  FD_ZERO(&master_set);
  FD_SET(server.descriptor(), &master_set);

  fd_set read_set;
  FD_ZERO(&read_set);

  struct timeval timeout = {0, 100000};

  while (!stop_requested()) {
    read_set = master_set;
    select(server.descriptor()+1, &read_set, nullptr, nullptr, &timeout);
    if (!FD_ISSET(server.descriptor(), &read_set)) {
      usleep(100);
      continue;
    }

    auto* sock = server.accept();
    const auto rpc = static_cast<VivadoServer::Rpc>(sock->get());
    
    // At most one compilation thread can be active at once. Issue kill-alls
    // until this is no longer the case.
    if (rpc == Rpc::KILL_ALL) {
      // Don't kill unless necessary
      while (false && busy_) {
        kill_all();
        this_thread::sleep_for(chrono::seconds(1));
      }
      sock->put(static_cast<uint8_t>(Rpc::OKAY));
      sock->flush();
      delete sock;
    } 
    // Kill the one compilation thread if necessary and then fire off a new thread to
    // attempt a recompilation. When the new thread is finished it will reset the busy
    // flag.
    else if (rpc == Rpc::COMPILE) {
      if (busy_) {
      	cout << "Killing old build..." << endl;
      }
      while (busy_) {
        kill_all();
        this_thread::sleep_for(chrono::seconds(1));
      }
      sock->put(static_cast<uint8_t>(Rpc::OKAY));
      sock->flush();
      busy_ = true;

      pool_.insert([this, sock]{
        string text = "";
        getline(*sock, text, '\0');
        
        string agfi, afi;
        const bool res = compile(text, agfi, afi);
        sock->put(static_cast<uint8_t>(res ? Rpc::OKAY : Rpc::ERROR));
        sock->flush();
        
        if (res) {
          // Send AGFI string for reconfig
          sock->write(agfi.c_str(), agfi.length());
          sock->put('\0');
          sock->flush();
          // Block until reconfig completes
          // TODO: delete if unnecessary
          sock->get();
        }

        busy_ = false;
        delete sock;
      });
    }
    // Unrecognized RPC
    else {
      cerr << "Bad RPC, disconnecting..." << endl;
      assert(false);
      delete sock;
    }
  }

  // Stop the thread pool
  pool_.stop_now();
}

void VivadoServer::init_pool() {
  // We have the invariant that there is exactly one compile thread out at any
  // given time, so no need to prime the pool with anything more than that.
  pool_.stop_now();
  pool_.set_num_threads(1);
  pool_.run();
}

void VivadoServer::init_cache() {
  // Create the cache if it doesn't already exist
  System::execute("mkdir -p " + cache_path_);
  System::execute("touch " + cache_path_ + "/cache.txt");
}

void VivadoServer::kill_all() {
  pid_t pid = getpid();
  System::execute("pkill -INT -P " + to_string(pid));
  //kill(pid_, SIGINT);
}

bool VivadoServer::compile(const std::string& text, std::string& agfi, std::string& afi) {
  string sport = to_string(port_);
  
  // Nothing to do if this code is already in the cache.
  if (cache_find(text, agfi, afi)) {
    cout << "Cache hit on port " << sport << endl;
    return true;
  }
  
  cout << "Starting compilation of length " << text.size() << " on port " << sport << endl;
  
  // Determine num apps based on text
  const char* prefix = "app_num == ";
  const int plen = strlen(prefix);
  const char* sp = strstr(text.c_str(), prefix);
  int num_apps = 0;
  while (sp != nullptr) {
    sp += plen;
    num_apps = max(num_apps, atoi(sp)+1);
    sp = strstr(sp, prefix);
  }
  assert(num_apps > 0);
  assert(num_apps <= 32);
  cout << "Detected " << num_apps << " application slot(s)" << endl;
  
  // Round num apps up to power of 2 for AmorphOS workaround
  int num_apps_rounded = 1;
  while (num_apps_rounded < num_apps) {
  	num_apps_rounded *= 2;
  }
  num_apps = num_apps_rounded;
  
  // Set up isolated compilation directory
  // TODO: use mkdtemp instead?
  string compile_path = compile_dir_ + "/" + sport;
  string source_path = System::src_root() + "/share/cascade/f1/cl";
  System::execute("rm -rf " + compile_path);
  System::execute("mkdir -p " + compile_path);
  System::execute("cp -r " + source_path + "/* " + compile_path + "/");
  
  // Save application code
  ofstream ofs1(compile_path + "/design/program_logic.v");
  ofs1 << text << endl;
  ofs1.flush();
  ofs1.close();
  
  // Generate AOS config file
  ofstream ofs2(compile_path + "/design/UserParams.sv");
  ofs2 << "`ifndef USER_PARAMS_SV_INCLUDED" << endl;
  ofs2 << "`define USER_PARAMS_SV_INCLUDED" << endl;
  ofs2 << endl;
  ofs2 << "package UserParams;" << endl;
  ofs2 << endl;
  ofs2 << "parameter NUM_APPS = " << num_apps << ";" << endl;
  ofs2 << "parameter CONFIG_APPS = 4;" << endl;
  ofs2 << endl;
  ofs2 << "endpackage" << endl;
  ofs2 << "`endif" << endl;
  ofs2.flush();
  ofs2.close();
  
  // Compile everything in separate process
  // Output redirection doesn't seeem to be working for some reason
  //string cmd = compile_path + "/compile.sh > " + compile_path + "/build/scripts/compile.log 2>&1";
  string cmd = compile_path + "/compile.sh";
  int rc = System::execute(cmd);
  if (rc != 0) {
    cout << "Build failed with rc: " << rc << endl;
    return false;
  }

  // Extract AGFI and AFI
  ifstream ifs1(compile_path + "/build/scripts/agfi.txt");
  ifs1 >> agfi;
  ifstream ifs2(compile_path + "/build/scripts/afi.txt");
  ifs2 >> afi;
  
  // Cache result
  cache_add(text, agfi, afi);
  
  cout << "Compilation succeeded for port " << sport << ": " << agfi << endl;
  
  // Clean up
  //System::execute("rm -rf " + compile_path);
  
  return true;
}

bool VivadoServer::cache_find(const std::string& text, string& agfi, string& afi) {
  string cache_file = cache_path_ + "/cache.txt";
  FILE* fp = fopen(cache_file.c_str(), "r");
  assert(fp != nullptr);
  int fd = fileno(fp);
  assert(fd != -1);
  int ret = flock(fd, LOCK_SH);
  assert(ret == 0);
  
  size_t n = 1024*1024;
  char* line = (char*)malloc(n);
  assert(line != nullptr);
  string s = "";
  
  bool found = false;
  while (true) {
    ret = getdelim(&line, &n, '\0', fp);
    if (ret == -1) break;
    int cmp = strncmp(line, text.c_str(), ret-1);
    ret = getdelim(&line, &n, '\0', fp);
    if (ret == -1) break;
    agfi = line;
    ret = getdelim(&line, &n, '\0', fp);
    if (ret == -1) break;
    afi = line;
    if (cmp == 0) {
      found = true;
      break;
    }
  }
  
  free(line);
  flock(fd, LOCK_UN);
  fclose(fp);
  return found;
}

void VivadoServer::cache_add(const string& text, const string& agfi, const string& afi) {
  string cache_file = cache_path_ + "/cache.txt";
  FILE* fp = fopen(cache_file.c_str(), "a");
  assert(fp != nullptr);
  int fd = fileno(fp);
  assert(fd != -1);
  int ret = flock(fd, LOCK_EX);
  assert(ret == 0);
  
  fwrite(text.c_str(), 1, text.size()+1, fp);
  fwrite(agfi.c_str(), 1, agfi.size()+1, fp);
  fwrite(afi.c_str(), 1, afi.size()+1, fp);
  assert(ferror(fp) == 0);
  
  flock(fd, LOCK_UN);
  fclose(fp);
}

} // namespace cascade::aos
