#ifndef _FCM_SKETCH_H
#define _FCM_SKETCH_H

#include "BOBHash32.h"
#include "common.h"
#include "counter.h"
#include "pds.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <ostream>
#include <unordered_map>
#include <vector>

class FCM_Sketch : public PDS {
private:
  vector<Counter *> stages;
  BOBHash32 hash;
  uint32_t n_stages;
  uint32_t *stages_sz;
  uint32_t k;

  uint32_t hashing(FIVE_TUPLE key) {
    char c_ftuple[sizeof(FIVE_TUPLE)];
    memcpy(c_ftuple, &key, sizeof(FIVE_TUPLE));
    return hash.run(c_ftuple, 4) % stages_sz[0];
  }

public:
  FCM_Sketch(uint32_t n_roots, string trace, uint32_t n_stage,
             uint32_t n_struct, uint32_t n_stages = 3, uint32_t k = 8)
      : PDS(trace, n_stage, n_struct) {

    // Maximum 32 bit counter
    uint32_t max_bits = 16;
    uint32_t max_count = std::numeric_limits<uint32_t>::max();
    uint32_t max_counter[n_stages];
    uint32_t *sz_stages = new uint32_t[n_stages];
    for (int i = n_stages - 1; i >= 0; --i) {
      sz_stages[i] = n_roots;
      max_counter[i] = max_count;
      n_roots *= k;
      max_count = max_count >> max_bits;
      max_bits /= 2;
      std::cout << sz_stages[i] << " " << max_counter[i] << std::endl;
    }

    // Setup stages with appropriate sizes
    for (size_t i = 0; i < n_stages; i++) {
      Counter *stage = new Counter[sz_stages[i]];
      for (size_t j = 0; j < sz_stages[j]; j++) {
        stage[j] = Counter(max_counter[j]);
      }
      stages.push_back(stage);
    }
    this->hash.initialize(n_struct);
    this->stages_sz = sz_stages;
    this->k = k;
    this->n_stages = n_stages;
  }

  ~FCM_Sketch() {
    for (auto v : this->stages) {
      delete v;
    }
  }
  uint32_t insert(FIVE_TUPLE tuple) {
    uint32_t hash_idx = this->hashing(tuple);
    for (size_t s = 0; s < n_stages; s++) {
      if (this->stages[s][hash_idx].overflow) {
        // Check for complete overflow
        if (s == n_stages - 1) {
          return 1;
        }
        continue;
      }
      this->stages[s][hash_idx].increment();
      break;
    }
    return 0;
  }

  void print_sketch() {
    for (size_t s = 0; s < n_stages; s++) {
      std::cout << "Stage " << s << " with " << this->stages_sz[s]
                << " counters" << std::endl;
      for (size_t i = 0; i < this->stages_sz[s]; i++) {
        std::cout << this->stages[s][i].count << " ";
      }
      std::cout << std::endl;
    }
  }
};

#endif // !_FCM_SKETCH_H
