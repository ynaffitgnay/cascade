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
#include <signal.h>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include "include/cascade.h"
#include "cl/cl.h"

using namespace cascade;
using namespace cascade::cl;
using namespace std;

namespace {

//__attribute__((unused)) auto& g1 = Group::create("Cascade Runtime Options");
//auto& march = StrArg<string>::create("--march")
//  .usage("sw|de10|f1|ulx3s")
//  .description("Target architecture")
//  .initial("sw");
//auto& fopen_dirs = StrArg<string>::create("-F")
//  .usage("<path1>:<path2>:...:<pathn>")
//  .description("Paths to search when resolving $fopen() statements")
//  .initial("");
//auto& inc_dirs = StrArg<string>::create("-I")
//  .usage("<path1>:<path2>:...:<pathn>")
//  .description("Paths to search when resolving `include directives")
//  .initial("");
//auto& input_path = StrArg<string>::create("-e")
//  .usage("path/to/file.v")
//  .description("Read input from file");
//
//__attribute__((unused)) auto& g2 = Group::create("Compiler Server Options");
//auto& compiler_host = StrArg<string>::create("--compiler_host")
//  .usage("<host>")
//  .description("Location of quartus / vivado server")
//  .initial("localhost");
//auto& compiler_port = StrArg<uint32_t>::create("--compiler_port")
//  .usage("<port>")
//  .description("Location of quartus / vivado server")
//  .initial(9900);
//auto& compiler_fpga = StrArg<uint32_t>::create("--compiler_fpga")
//  .usage("<fpga>")
//  .description("Index of target FPGA for deployment")
//  .initial(0);
//
//__attribute__((unused)) auto& g3 = Group::create("Logging Options");
//auto& profile = StrArg<int>::create("--profile")
//  .usage("<n>")
//  .description("Number of seconds to wait between profiling events; setting n to zero disables profiling; only effective with --enable_info")
//  .initial(0);
//auto& enable_info = FlagArg::create("--enable_info")
//  .description("Turn on info messages");
//auto& disable_warning = FlagArg::create("--disable_warning")
//  .description("Turn off warning messages");
//auto& disable_error = FlagArg::create("--disable_error")
//  .description("Turn off error messages");
//auto& enable_log = FlagArg::create("--enable_log")
//  .description("Prints debugging information to log file");
//
//__attribute__((unused)) auto& g4 = Group::create("Optimization Options");
//auto& disable_inlining = FlagArg::create("--disable_inlining")
//  .description("Prevents cascade from inlining modules");
//auto& open_loop_target = StrArg<size_t>::create("--open_loop_target")
//  .usage("<n>")
//  .description("Maximum number of seconds to run in open loop for before transferring control back to runtime")
//  .initial(1);
//
//__attribute__((unused)) auto& g5 = Group::create("REPL Options");
//auto& disable_repl = FlagArg::create("--disable_repl")
//  .description("Disables the REPL and treats user input as stdin");
//
//class inbuf : public streambuf {
//  public:
//    inbuf(streambuf* sb) : streambuf() { 
//      sb_ = sb;
//      prompt_ = false;
//    }
//    ~inbuf() override = default;
//
//  private:
//    streambuf* sb_;
//    bool prompt_;
//
//    int sync() override {
//      return sb_->pubsync();
//    }
//    int_type uflow() override {
//      const auto res =  sb_->sbumpc();
//      prompt(res);
//      prompt_ = false;
//      return res;
//    }
//    int_type underflow() override {
//      const auto res = sb_->sgetc();
//      prompt(res);
//      return res;
//    }
//    void prompt(int_type c) {
//      if ((c == '\n') && !prompt_) {
//        prompt_ = true;
//        cout << ">>> ";
//        cout.flush();
//      }
//    }
//};
//
//class outbuf : public streambuf {
//  public:
//    outbuf(const string& color = "") : streambuf() {
//      color_ = color;
//    }
//    ~outbuf() override = default;
//
//  private:
//    string color_;
//    stringstream ss_;
//
//    int_type overflow(int_type c = traits_type::eof()) override {
//      ss_.put(c);
//      if (c == '\n') {
//        ss_ << ((color_ != "") ? "\033[00m" : "") << ">>> " << color_;
//      }
//      return c;
//    }
//    int_type sync() override {
//      cout << color_ << ss_.str() << ((color_ != "") ? "\033[00m" : "");
//      cout.flush();
//      ss_.str(string());
//      return 0;
//    }
//};

// Allocate cascade on the heap so we can guarantee that it's torn down before
// stack or static variables.
Cascade* cascade_ = nullptr;

//// Signal Handlers:
//void int_handler(int sig) {
//  (void) sig;
//  ::cascade_->request_stop();
//}
//void segv_handler(int sig) {
//  (void) sig;
//  cerr << "\033[31mCASCADE SHUTDOWN UNEXPECTEDLY\033[00m" << endl;
//  cerr << "\033[31mPlease rerun with --enable_log and forward log file to developers\033[00m" << endl;
//  exit(1);
//}

} // namespace

int main(int argc, char** argv) {
  //// Parse command line
  //Simple::read(argc, argv);
  //
  //// Wrap cin in inbuf (re-prints the prompt when the user types \n)
  //inbuf ib(cin.rdbuf());
  //cin.rdbuf(&ib);
  //
  //// Install signal handlers
  //{ struct sigaction action;
  //  memset(&action, 0, sizeof(action));
  //  action.sa_handler = ::segv_handler;
  //  sigaction(SIGSEGV, &action, nullptr);
  //}
  //{ struct sigaction action;
  //  memset(&action, 0, sizeof(action));
  //  action.sa_handler = ::int_handler;
  //  sigaction(SIGINT, &action, nullptr);
  //}

  // Create a new cascade
  ::cascade_ = new Cascade();

  // Set command line flags
  //::cascade_->set_fopen_dirs(::fopen_dirs.value());
  //::cascade_->set_include_dirs(::inc_dirs.value());
  //::cascade_->set_enable_inlining(!::disable_inlining.value());
  //::cascade_->set_open_loop_target(::open_loop_target.value());
  //::cascade_->set_quartus_server(::compiler_host.value(), ::compiler_port.value());
  ::cascade_->set_vivado_server("localhost", 9903, 0);
  ::cascade_->set_profile_interval(1);
  ::cascade_->set_stdout(std::cout.rdbuf());
  ::cascade_->set_stderr(std::cout.rdbuf());
  ::cascade_->set_stdwarn(std::cout.rdbuf());
  ::cascade_->set_stdinfo(std::cout.rdbuf());
  

  //// Map standard streams to colored outbufs
  //if (::disable_repl.value()) {
  //  ::cascade_->set_stdin(cin.rdbuf());
  //}
  //::cascade_->set_stdout(new outbuf());
  //if (!::disable_error.value()) {
  //  ::cascade_->set_stderr(new outbuf("\033[31m"));
  //}
  //if (!::disable_warning.value()) {
  //  ::cascade_->set_stdwarn(new outbuf("\033[33m"));
  //}
  //if (::enable_info.value()) {
  //  ::cascade_->set_stdinfo(new outbuf("\033[37m"));
  //}
  auto* fb = new filebuf();
  fb->open("tiff_cascade.log", ios::app | ios::out);
  ::cascade_->set_stdlog(fb);

  //// Print the initial prompt
  //cout << ">>> ";
  
  // Start cascade, and read the march file and -e file (if provided)
  ::cascade_->run();
  *::cascade_ << "`include \"share/cascade/march/regression/f1_minimal.v\"\n"
              << "`include \"share/cascade/test/benchmark/mips32/\"\n"
	      << "`include \"share/cascade/test/benchmark/mips32/run_bubble_128_1024.v\"" << endl;
  //::cascade_->stop_now();
  //
  //// Switch to reading from cin if the REPL wasn't disable and wait for finish
  //if (!::disable_repl.value()) {
  //  ::cascade_->rdbuf(cin.rdbuf());
  //}
  //::cascade_->run();
  //::cascade_->wait_for_stop();
  //
  //// If cascade isn't finished by now, it's because we've caught a signal.
  //// Either way, we can delete it now.
  //if (!::cascade_->is_finished()) {
  //  cerr << "\033[31mCaught Signal\033[00m" << endl;
  //}
  //delete ::cascade_;
  //
  //cout << "Goodbye!" << endl;
  
  ::cascade_->flush();

  // Sleep for 30 seconds
  usleep(30000000);
  
  // Retarget
  *::cascade_ << "initial retarget(\"regression/f1_minimal_tif\");\n";
  

  return 0;
}
