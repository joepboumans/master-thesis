#ifndef _COUNT_MIN_CPP
#define _COUNT_MIN_CPP

#include "count-min.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <math.h>
#include <set>
#include <sys/types.h>

uint32_t CountMin::insert(FIVE_TUPLE tuple) {
  this->true_data[tuple]++;
  bool hh_overflow = true;

  for (size_t i = 0; i < this->n_hash; i++) {
    int hash_idx = this->hashing(tuple, i);
    this->counters[i][hash_idx].increment();
    if (this->counters[i][hash_idx].count < this->hh_threshold) {
      hh_overflow = false;
    }
  }
  if (hh_overflow) {
    this->HH_candidates.insert((string)tuple);
  }
  return 0;
}

uint32_t CountMin::hashing(FIVE_TUPLE key, uint32_t k) {
  return this->hash[k].run((const char *)key.num_array, 4) % this->columns;
}

uint32_t CountMin::lookup(FIVE_TUPLE tuple) {
  uint32_t min = std::numeric_limits<uint32_t>::max();
  for (size_t i = 0; i < this->n_hash; i++) {
    int hash_idx = this->hashing(tuple, i);
    min = std::min(min, this->counters[i][hash_idx].count);
  }
  return min;
}

void CountMin::analyze(int epoch) {
  // Use lookup to find tuples
  this->average_absolute_error = 0;
  this->average_relative_error = 0;
  double n = this->true_data.size();

  // std::cout << std::endl;
  // Heavy Hitter Detection (F1 Score)
  int true_pos = 0, false_pos = 0, true_neg = 0, false_neg = 0;

  // Flow Size Distribution (Weighted Mean Relative Error)
  using pair_type = decltype(this->true_data)::value_type;
  auto max_count =
      std::max_element(this->true_data.begin(), this->true_data.end(),
                       [](const pair_type &p1, const pair_type &p2) {
                         return p1.second < p2.second;
                       });
  vector<uint32_t> true_fsd(max_count->second + 1);
  vector<uint32_t> e_fsd(max_count->second + 1);
  double wmre = 0.0;

  for (const auto &[tuple, count] : this->true_data) {
    // Flow Size Distribution (Weighted Mean Relative Error)
    true_fsd[count]++;
    e_fsd[this->lookup(tuple)]++;

    // Flow Size Estimation (Average Relative Error, Average Absolute Error)
    int diff = count - this->lookup(tuple);

    this->average_absolute_error += std::abs(diff);
    this->average_relative_error += ((double)std::abs(diff) / count);

    // Heavy Hitter Detection (F1 Score)
    if (count > this->hh_threshold) {
      if (auto search = this->HH_candidates.find(tuple);
          search != this->HH_candidates.end()) {
        true_pos++;
      } else {
        false_neg++;
      }
      continue;
    }
    if (auto search = this->HH_candidates.find(tuple);
        search == this->HH_candidates.end()) {
      true_neg++;
    } else {
      false_pos++;
    }
  }

  this->average_absolute_error = this->average_absolute_error / n;
  this->average_relative_error = this->average_relative_error / n;
  // std::cout << epoch << "\tAAE: " << this->average_absolute_error <<
  // std::endl; std::cout << epoch << "\tARE: " << this->average_relative_error
  // << std::endl; std::cout << "n = " << n << std::endl;

  // std::cout << epoch << " Heavy hitters" << std::endl;
  // char msg[200];
  // sprintf(msg, "\tTP:%i\tFP:%i\tTN:%i\tFN:%i", true_pos, false_pos, true_neg,
  //         false_neg);
  // std::cout << epoch << msg << std::endl;

  // Heavy Hitter Detection (F1 Score)
  if (true_pos == 0 && false_pos == 0) {
    this->recall = 1.0;
  } else {
    this->recall = (double)true_pos / (true_pos + false_pos);
  }
  if (true_neg == 0 && false_neg == 0) {
    this->precision = 1.0;
  } else {
    this->precision = (double)true_neg / (true_neg + false_neg);
  }
  this->f1 = 2 * ((recall * precision) / (precision + recall));

  // sprintf(msg, "\tRecall:%.3f\tPrecision:%.3f\tF1:%.3f", this->recall,
  //         this->precision, this->f1);
  // std::cout << epoch << msg << std::endl;

  // Flow Size Distribution (Weighted Mean Relative Error)
  double wmre_nom = 0.0;
  double wmre_den = 0.0;
  for (size_t i = 0; i < max(true_fsd.size(), e_fsd.size()); i++) {
    uint32_t diff = true_fsd[i] - e_fsd[i];
    wmre_nom += std::abs((double)diff);
    wmre_den += (double)(true_fsd[i] + e_fsd[i]) / 2;
  }
  this->wmre = wmre_nom / wmre_den;
  // std::cout << "\tWMRE: " << wmre << std::endl;

  // Save data into csv
  char csv[300];
  sprintf(csv, "%i,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f", epoch,
          this->average_relative_error, this->average_absolute_error,
          this->wmre, this->recall, this->precision, this->f1);
  this->fcsv << csv << std::endl;
}

void CountMin::reset() {
  this->true_data.clear();
  for (auto &r : this->counters) {
    for (auto &c : r) {
      c.count = 0;
      c.overflow = false;
    }
  }
}

void CountMin::print_sketch() {
  char msg[100];
  sprintf(msg, "Printing CM sketch of %ix%i with mem sz %i", this->n_hash,
          this->columns, this->mem_sz);
  std::cout << msg << std::endl;
  for (size_t i = 0; i < this->n_hash; i++) {
    std::cout << i << ":\t";
    for (const auto &c : this->counters[i]) {
      std::cout << c.count << " | ";
    }
    std::cout << std::endl;
  }
}
#endif // !_COUNT_MIN_CPP
