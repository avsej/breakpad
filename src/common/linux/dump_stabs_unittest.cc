// Copyright (c) 2010 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// dump_stabs_unittest.cc: Unit tests for DumpStabsHandler.

#include <vector>

#include "breakpad_googletest_includes.h"
#include "common/linux/dump_stabs.h"

using google_breakpad::DumpStabsHandler;
using google_breakpad::Module;
using std::vector;

TEST(DumpStabsHandler, SimpleCU) {
  Module m("name", "os", "arch", "id");
  DumpStabsHandler h(&m);

  // Feed in a simple compilation unit that defines a function with
  // one line.
  EXPECT_TRUE(h.StartCompilationUnit("compilation-unit", 0x9f4d1271e50db93bLL,
                                     "build-directory"));
  EXPECT_TRUE(h.StartFunction("function", 0xfde4abbed390c394LL));
  EXPECT_TRUE(h.Line(0xfde4abbed390c394LL, "source-file-name", 174823314));
  EXPECT_TRUE(h.EndFunction(0xfde4abbed390c3a4LL));
  EXPECT_TRUE(h.EndCompilationUnit(0xfee4abbed390c3a4LL));
  h.Finalize();

  // Now check to see what has been added to the Module.
  Module::File *file = m.FindExistingFile("source-file-name");
  ASSERT_TRUE(file != NULL);

  vector<Module::Function *> functions;
  m.GetFunctions(&functions, functions.end());
  ASSERT_EQ((size_t) 1, functions.size());
  Module::Function *function = functions[0];
  EXPECT_STREQ("function", function->name.c_str());
  EXPECT_EQ(0xfde4abbed390c394LL, function->address);
  EXPECT_EQ(0x10U, function->size);
  EXPECT_EQ(0U, function->parameter_size);
  ASSERT_EQ((size_t) 1, function->lines.size());
  Module::Line *line = &function->lines[0];
  EXPECT_EQ(0xfde4abbed390c394LL, line->address);
  EXPECT_EQ(0x10U, line->size); // derived from EndFunction
  EXPECT_TRUE(line->file == file);
  EXPECT_EQ(174823314, line->number);
}

TEST(InferSizes, LineSize) {
  Module m("name", "os", "arch", "id");
  DumpStabsHandler h(&m);

  // Feed in a simple compilation unit that defines a function with
  // one line.
  EXPECT_TRUE(h.StartCompilationUnit("compilation-unit", 0xb4513962eff94e92LL,
                                     "build-directory"));
  EXPECT_TRUE(h.StartFunction("function", 0xb4513962eff94e92LL));
  EXPECT_TRUE(h.Line(0xb4513962eff94e92LL, "source-file-name-1", 77396614));
  EXPECT_TRUE(h.Line(0xb4513963eff94e92LL, "source-file-name-2", 87660088));
  EXPECT_TRUE(h.EndFunction(0));  // unknown function end address
  EXPECT_TRUE(h.EndCompilationUnit(0)); // unknown CU end address
  EXPECT_TRUE(h.StartCompilationUnit("compilation-unit-2", 0xb4523963eff94e92LL,
                                     "build-directory-2")); // next boundary  
  EXPECT_TRUE(h.EndCompilationUnit(0));
  h.Finalize();

  // Now check to see what has been added to the Module.
  Module::File *file1 = m.FindExistingFile("source-file-name-1");
  ASSERT_TRUE(file1 != NULL);
  Module::File *file2 = m.FindExistingFile("source-file-name-2");
  ASSERT_TRUE(file2 != NULL);

  vector<Module::Function *> functions;
  m.GetFunctions(&functions, functions.end());
  ASSERT_EQ((size_t) 1, functions.size());

  Module::Function *function = functions[0];
  EXPECT_STREQ("function", function->name.c_str());
  EXPECT_EQ(0xb4513962eff94e92LL, function->address);
  EXPECT_EQ(0x1000100000000ULL, function->size); // inferred from CU end
  EXPECT_EQ(0U, function->parameter_size);
  ASSERT_EQ((size_t) 2, function->lines.size());

  Module::Line *line1 = &function->lines[0];
  EXPECT_EQ(0xb4513962eff94e92LL, line1->address);
  EXPECT_EQ(0x100000000ULL, line1->size); // derived from EndFunction
  EXPECT_TRUE(line1->file == file1);
  EXPECT_EQ(77396614, line1->number);

  Module::Line *line2 = &function->lines[1];
  EXPECT_EQ(0xb4513963eff94e92LL, line2->address);
  EXPECT_EQ(0x1000000000000ULL, line2->size); // derived from EndFunction
  EXPECT_TRUE(line2->file == file2);
  EXPECT_EQ(87660088, line2->number);
}

TEST(FunctionNames, Mangled) {
  Module m("name", "os", "arch", "id");
  DumpStabsHandler h(&m);

  // Compilation unit with one function, mangled name.
  EXPECT_TRUE(h.StartCompilationUnit("compilation-unit", 0xf2cfda63cef7f46cLL,
                                     "build-directory"));
  EXPECT_TRUE(h.StartFunction("_ZNSt6vectorIySaIyEE9push_backERKy",
                              0xf2cfda63cef7f46dLL));
  EXPECT_TRUE(h.EndFunction(0));
  EXPECT_TRUE(h.EndCompilationUnit(0));

  h.Finalize();

  // Now check to see what has been added to the Module.
  Module::File *file = m.FindExistingFile("compilation-unit");
  ASSERT_TRUE(file != NULL);

  vector<Module::Function *> functions;
  m.GetFunctions(&functions, functions.end());
  ASSERT_EQ(1U, functions.size());

  Module::Function *function = functions[0];
  // This is GCC-specific, but we shouldn't be seeing STABS data anywhere
  // but Linux.
  EXPECT_STREQ("std::vector<unsigned long long, "
               "std::allocator<unsigned long long> >::"
               "push_back(unsigned long long const&)",
               function->name.c_str());
  EXPECT_EQ(0xf2cfda63cef7f46dLL, function->address);
  EXPECT_LT(0U, function->size); // should have used dummy size
  EXPECT_EQ(0U, function->parameter_size);
  ASSERT_EQ(0U, function->lines.size());
}

// TODO --- if we actually cared about STABS. Even without these we've
// got full coverage of non-failure source lines in dump_stabs.cc.

// Line size from next line
// Line size from function end
// Line size from next function start
// line size from cu end
// line size from next cu start
// fallback size is something plausible

// function size from function end
// function size from next function start
// function size from cu end
// function size from next cu start
// fallback size is something plausible

// omitting functions outside the compilation unit's address range
// zero-line, one-line, many-line functions
