#include "src/common.h"
#include "src/data-parser.h"
#include "src/fcmsketch.h"
#include <cstddef>
#include <cstring>
#include <glob.h>
#include <iostream>
#include <iterator>

int main() {
  dataParser data_parser;
  // Get data files
  glob_t glob_res;
  memset(&glob_res, 0, sizeof(glob_res));
  glob("data/*.dat", GLOB_TILDE, NULL, &glob_res);
  vector<std::string> filenames;
  for (size_t i = 0; i < glob_res.gl_pathc; ++i) {
    filenames.push_back(std::string(glob_res.gl_pathv[i]));
  }

  // Load in traces
  TRACE traces[std::size(filenames)];
  std::size_t i = 0;
  for (auto f : filenames) {
    std::cout << f << std::endl;
    data_parser.get_traces(f.data(), traces[i++]);
  }

  // FCM_Sketch fcmsketch(4);
  // Loop over traces
  for (auto trace : traces) {
    std::cout << "Trace data:" << std::endl;
    for (int i = 0; i < 10; i++) {
      print_five_tuple(trace.at(i));
    }
  }
  return 0;
}
