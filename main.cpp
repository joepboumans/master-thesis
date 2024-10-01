// #include "count-min.h"
#include "common.h"
#include "count-min.cpp"
#include "cuckoo-hash.hpp"
#include "data-parser.h"
#include "fcm-sketch.hpp"
#include "iblt.h"
#include "simulator.h"
#include "src/bloomfilter.h"
#include "waterfall-fcm.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <glob.h>
#include <iostream>
#include <iterator>
#include <malloc.h>
#include <ostream>
#include <unordered_map>
#include <vector>

int main() {
  dataParser data_parser;
  // Get data files
  glob_t *glob_res = new glob_t;
  if (memset(glob_res, 0, sizeof(glob_t)) == NULL) {
    std::cout << "Glob init failed" << std::endl;
    exit(1);
  }
  glob("data/*.dat", GLOB_TILDE, NULL, glob_res);
  vector<string> filenames(glob_res->gl_pathc);
  for (size_t i = 0; i < glob_res->gl_pathc; ++i) {
    filenames[i] = string(glob_res->gl_pathv[i]);
  }
  globfree(glob_res);

  // Load in traces
  vector<TRACE> traces(filenames.size());
  unordered_map<string, TRACE> data_traces;
  size_t i = 0;
  string dir = "data/";
  string dat = ".dat";
  for (string &f : filenames) {
    std::cout << f << std::endl;
    data_parser.get_traces(f.data(), traces[i]);
    string::size_type k = f.find(dir);
    if (k != string::npos) {
      f.erase(k, dir.length());
    }
    string::size_type j = f.find(dat);
    if (j != string::npos) {
      f.erase(j, dat.length());
    }
    data_traces[f] = traces[i++];
  }

  std::cout << "------" << std::endl;
  std::cout << "Finished parsing data, starting simulations..." << std::endl;
  std::cout << "------" << std::endl;

  uint32_t sim_length = 60;
  uint32_t iter = 60;
  for (const auto &[name_set, trace] : data_traces) {
    vector<PDS *> stages;
    uint32_t hh_threshold = trace.size() * 0.0005 / sim_length;
    // CuckooHash cuckoo(10, 1024, name_set, 0, 0);
    // stages.push_back(&cuckoo);
    // CountMin cm(128, 1024, hh_threshold, name_set, 0, 0);
    // stages.push_back(&cm);
    // FCM_Sketch fcm(6241, 3, 8, hh_threshold, 1, name_set, 0, 0);
    // stages.push_back(&fcm);
    WaterfallFCM waterfall =
        WaterfallFCM(128, 4, 32, hh_threshold, 2, 10, 1024, name_set, 0, 0);
    stages.push_back(&waterfall);
    Simulator sim(stages, stages.size(), sim_length);
    // Default length of CAIDA traces is 60s
    sim.run(trace, iter);
  }

  std::cout << "------" << std::endl;
  std::cout << "Finished simulations!" << std::endl;
  std::cout << "------" << std::endl;

  return 0;
}
