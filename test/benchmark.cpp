#include "bloomfilter.h"
#include "count-min.h"
#include "cuckoo-hash.hpp"
#include "data-parser.h"
#include "fcm-sketch.hpp"
#include "simulator.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

TEST_CASE("Benchmark BF", "[BF][Benchmark]") {
  dataParser data_parser;
  string data_set = "data/equinix-chicago-20160121-130000.dat";
  TRACE trace;
  data_parser.get_traces(data_set.data(), trace);

  SECTION("Run BF") {
    auto sz = GENERATE(1, 8, 16, 32, 64, 128, 256, 512, 1024);

    vector<PDS *> stage;
    BloomFilter bfilter(sz * 1024, 4, "benchmark", 0, 0);
    bfilter.setName();
    bfilter.setupLogging();
    stage.push_back(&bfilter);
    Simulator simulator(stage, stage.size(), 1);
    simulator.run(trace, 60);
  }
}
TEST_CASE("Benchmark CHT", "[CHT][Benchmark]") {
  dataParser data_parser;
  string data_set = "data/equinix-chicago-20160121-130000.dat";
  TRACE trace;
  data_parser.get_traces(data_set.data(), trace);

  auto tables = GENERATE(2, 3, 4, 8, 16, 32);
  SECTION("Run CHT") {
    auto sz = GENERATE(1024, 4 * 1024, 8 * 1024, 16 * 1024);
    vector<PDS *> stage;
    CuckooHash cuckoo(tables, sz, "benchmark", 0, 0);
    stage.push_back(&cuckoo);
    Simulator simulator(stage, stage.size(), 1);
    simulator.run(trace, 60);
  }
}

TEST_CASE("Benchmark CM", "[CM][Benchmark]") {
  dataParser data_parser;
  string data_set = "data/equinix-chicago-20160121-130000.dat";
  TRACE trace;
  data_parser.get_traces(data_set.data(), trace);

  SECTION("Run CM") {
    auto sz = GENERATE(1, 8, 16, 32, 64, 128, 256, 512, 1024);

    vector<PDS *> stage;
    CountMin cm(sz, 1024, trace.size() * 0.0005 / 60, "benchmark", 0, 0);
    stage.push_back(&cm);
    Simulator simulator(stage, stage.size(), 1);
    simulator.run(trace, 60);
  }
}

TEST_CASE("Benchmark FCM Sketch", "[FCM][Benchmark]") {
  dataParser data_parser;
  string data_set = "data/equinix-chicago-20160121-130000.dat";
  TRACE trace;
  data_parser.get_traces(data_set.data(), trace);

  SECTION("Run FCM") {
    auto stages = GENERATE(3, 4, 5, 6, 8);
    SECTION("Run with increasing amount of roots") {
      auto roots = GENERATE(8, 16, 32, 64, 128);

      vector<PDS *> stage;
      FCM_Sketch fcm(roots, stages, 8, trace.size() * 0.0005 / 60, 1,
                     "benchmark", 0, 0);
      stage.push_back(&fcm);
      Simulator simulator(stage, stage.size(), 1);
      simulator.run(trace, 60);
    }
  }
}
