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

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <cstdlib>
#include <cxxopts.hpp>
#include <filesystem>
#include <iostream>
#include <range/v3/action/drop_while.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/all.hpp>
#include <vector>

#include "oi/OICodeGen.h"
#include "oi/OIGenerator.h"

namespace fs = std::filesystem;
using namespace oi::detail;
using namespace ranges;

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_minloglevel = 0;
  FLAGS_stderrthreshold = 0;

  cxxopts::Options options("oilgen",
                           "Generate OIL object code from an input file");

  options.add_options()
    ("h,help", "Print usage")
    ("o,output", "Write output(s) to file(s) with this prefix", cxxopts::value<fs::path>()->default_value("a.o"))
    ("c,config-file", "Path to OI configuration file(s)", cxxopts::value<std::vector<fs::path>>())
    ("d,debug-level", "Verbose level for logging", cxxopts::value<int>())
    ("j,dump-jit", "Write generated code to a file (for debugging)", cxxopts::value<fs::path>()->default_value("jit.cpp"))
    ("e,exit-code", "Return a bad exit code if nothing is generated")
    ("p,pic", "Generate position independent code")
      ;
  options.positional_help("CLANG_ARGS...");
  options.allow_unrecognised_options();

  auto args = options.parse(argc, argv);
  if (args.count("help") > 0) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  if (args.count("debug-level") > 0) {
    int level = args["debug-level"].as<int>();
    google::LogToStderr();
    google::SetStderrLogging(google::INFO);
    google::SetVLOGLevel("*", level);
    // Upstream glog defines `GLOG_INFO` as 0 https://fburl.com/ydjajhz0,
    // but internally it's defined as 1 https://fburl.com/code/9fwams75
    gflags::SetCommandLineOption("minloglevel", "0");
  }

  OIGenerator oigen;

  oigen.setOutputPath(args["output"].as<fs::path>());

  oigen.setUsePIC(args["pic"].as<bool>());
  oigen.setFailIfNothingGenerated(args["exit-code"].as<bool>());

  if (args.count("config-file") > 0)
    oigen.setConfigFilePaths(args["config-file"].as<std::vector<fs::path>>());
  if (args.count("dump-jit") > 0)
    oigen.setSourceFileDumpPath(args["dump-jit"].as<fs::path>());

  oigen.setClangArgs(args.unmatched());

  return oigen.generate();
}
