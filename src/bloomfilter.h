#ifndef _BLOOMFILTER_H
#define _BLOOMFILTER_H

#include "BOBHash32.h"
#include "common.h"
#include "pds.h"
#include <algorithm>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

class BloomFilter : public PDS {
public:
  char filename_dat[400];
  char filename_csv[400];
  vector<bool> array;
  set<string> tuples;
  BOBHash32 *hash;
  uint32_t n_hash;
  uint32_t length;
  double avg_f1 = 0.0;
  double avg_recall = 0.0;

  FILE *fdata;
  FILE *fcsv;

  BloomFilter(uint32_t sz, uint32_t n, uint32_t k) {
    sprintf(this->filename_dat, "results/%s_%i.dat", typeid(this).name(), n);
    sprintf(this->filename_csv, "results/%s_%i.csv", typeid(this).name(), n);
    // Remove previous data file
    std::remove(filename_dat);
    std::remove(filename_csv);
    // Open files

    std::cout << "Removed files, opening new files..." << std::endl;
    std::cout << this->filename_dat << std::endl;
    this->fdata = fopen(this->filename_dat, "wb");
    this->fcsv = fopen(this->filename_csv, "wb");
    std::cout << "Opened files" << std::endl;

    for (size_t i = 0; i < sz; i++) {
      array.push_back(false);
    }

    this->hash = new BOBHash32[k];
    for (size_t i = 0; i < k; i++) {
      this->hash[i].initialize(n * k + i);
    }
    this->length = sz;
    this->n_hash = k;
  }
  ~BloomFilter() {
    this->array.clear();
    fclose(this->fdata);
    fclose(this->fcsv);
  }

  int insert(FIVE_TUPLE tuple) {
    bool tuple_inserted = false;
    for (size_t i = 0; i < this->n_hash; i++) {
      int hash_idx = this->hashing(tuple, i);
      if (!array[hash_idx]) {
        array[hash_idx] = true;
        tuple_inserted = true;
      }
    }

    if (tuple_inserted) {
      tuples.insert((string)tuple);
    }
    return 0;
  }

  int lookup(FIVE_TUPLE tuple) {
    for (size_t i = 0; i < this->n_hash; i++) {
      int hash_idx = this->hashing(tuple, i);
      if (!array[hash_idx]) {
        return 0;
      }
    }
    return 1;
  }

  void reset() { std::fill(this->array.begin(), this->array.end(), false); }

  void analyze(unordered_map<string, uint32_t> true_data) {
    int false_pos = 0;
    int total_pkt = true_data.size();
    std::cout << "Total found " << this->tuples.size() << std::endl;
    for (const auto &[s_tuple, count] : true_data) {
      FIVE_TUPLE tup(s_tuple);
      // std::cout << tup << std::endl;
      if (auto search = this->tuples.find((string)tup);
          search != this->tuples.end()) {
        // Recorded correctly
      } else {
        false_pos++;
      }
    }
    std::cout << "False Positives: " << false_pos << std::endl;
    std::cout << "Total num_pkt " << total_pkt << std::endl;
    int true_pos = total_pkt - false_pos;
    double recall = (double)true_pos / (true_pos + false_pos);
    double precision = 1; // IN BF can never result in false negatives
    double f1_score = 2 * ((recall * precision) / (precision + recall));
    char msg[1000];
    sprintf(msg, "Recall: %.3f\tPrecision: %.3f\nF1-Score: %.3f", recall,
            precision, f1_score);
    std::cout << msg << std::endl;
  }

  void print_sketch() {
    uint32_t count = 0;
    for (auto i : this->array) {
      // std::cout << i;
      if (i) {
        count++;
      }
    }
    std::cout << std::endl;
    std::cout << "Total filled indexes: " << count << std::endl;
  }

  int hashing(FIVE_TUPLE key, uint32_t k) {
    char c_ftuple[sizeof(FIVE_TUPLE)];
    memcpy(c_ftuple, &key, sizeof(FIVE_TUPLE));
    return hash[k].run(c_ftuple, 4) % this->length;
  }

  void store_data(int epoch) {
    if (!this->fdata) {
      std::cout << "Cannot open file " << this->filename_dat << std::endl;
      throw;
    }
    try {
      char msg[3];
      sprintf(msg, "%i:", epoch);
      fwrite(msg, sizeof(msg), 1, this->fdata);
    } catch (exception e) {
      std::cout << "Failed writing to file" << std::endl;
      std::cout << e.what() << std::endl;
      throw e;
    }

    // Print data to file
    vector<char> carray(this->array.begin(), this->array.end());
    fwrite(reinterpret_cast<const char *>(&carray), sizeof(carray), 1,
           this->fdata);
    char endl = '\n';
    fwrite(&endl, sizeof(endl), 1, this->fdata);
    return;
  }
};

class LazyBloomfilter : public BloomFilter {
public:
  using BloomFilter::BloomFilter;
  // LazyBloomfilter(uint32_t sz, uint32_t n, uint32_t k)
  //     : BloomFilter(sz, n, k) {}

  int insert(FIVE_TUPLE tuple) {
    for (size_t i = 0; i < this->n_hash; i++) {
      int hash_idx = this->hashing(tuple, i);
      if (!array[hash_idx]) {
        array[hash_idx] = true;
        tuples.insert((string)tuple);
        return 0;
      }
    }
    return 1;
  }
};
#endif // !_BLOOMFILTER_H
