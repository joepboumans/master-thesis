#ifndef _EMALGORITHM_FCM_HPP
#define _EMALGORITHM_FCM_HPP

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <numeric>
#include <ostream>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using std::unordered_map;
using std::vector;

class EMFSD {
  vector<vector<uint32_t>> counters;
  uint32_t w;                            // width of counters
  vector<double> dist_old, dist_new;     // for ratio \phi
  vector<vector<uint32_t>> counter_dist; // initial counter values
  vector<vector<vector<vector<uint32_t>>>> thresholds;
  vector<uint32_t> stage_sz;

public:
  vector<double> ns; // for integer n
  double n_sum;
  double card_init; // initial cardinality by MLE
  uint32_t iter = 0;
  bool inited = false;
  EMFSD(vector<uint32_t> szes, vector<vector<vector<vector<uint32_t>>>> thresh,
        uint32_t in_max_value, uint32_t max_degree,
        vector<vector<uint32_t>> counters)
      : stage_sz(szes), counters(counters), max_degree(max_degree) {

    for (size_t st = 0; st < this->counters.size(); st++) {
      if (this->counters[st].size() == 0) {
        continue;
      }
      std::cout << "Degree: " << st << std::endl;
      for (auto &val : this->counters[st]) {
        std::cout << " " << val;
      }
    }
    std::cout << std::endl;
    this->max_counter_value = 0;
    // Setup distribution and the thresholds for it
    this->counter_dist = vector<vector<uint32_t>>(
        counters.size(), vector<uint32_t>(this->max_counter_value, 0));
    thresholds.resize(counters.size());
    for (size_t d = 0; d < counters.size(); d++) {
      if (counters[d].size() == 0) {
        continue;
      }
      uint32_t max_val =
          (*std::max_element(counters[d].begin(), counters[d].end()));
      this->max_counter_value = std::max(max_val, this->max_counter_value);

      counter_dist[d].resize(this->max_counter_value + 1);
      std::fill(counter_dist[d].begin(), counter_dist[d].end(), 0);
      thresholds[d].resize(this->max_counter_value + 1);
    }
    this->max_counter_value = std::max(this->max_counter_value, in_max_value);
    printf("[EM_FCM] Maximum counter value : %d\n", this->max_counter_value);
    // Inital guess for # of flows
    n_new = 0.0; // # of flows (Cardinality)
    for (size_t d = 0; d < counters.size(); d++) {
      n_new += counters[d].size();
      for (size_t i = 0; i < counters[d].size(); i++) {
        counter_dist[d][counters[d][i]]++;
        thresholds[d][counters[d][i]] = thresh[d][i];
      }
    }
    w = this->stage_sz[0];

    // Inital guess for Flow Size Distribution (Phi)
    dist_new.resize(this->max_counter_value + 1);
    for (auto &degree : counters) {
      for (auto count : degree) {
        dist_new[count]++;
      }
    }
    ns.resize(this->max_counter_value + 1);
    for (size_t d = 0; d < counter_dist.size(); d++) {
      for (size_t i = 0; i < counter_dist[d].size(); i++) {
        if (counter_dist[d].size() == 0) {
          continue;
        }
        dist_new[i] += counter_dist[d][i];
        ns[i] += counter_dist[d][i];
      }
    }
    // Normalize over inital cardinality
    for (size_t i = 0; i < dist_new.size(); i++) {
      dist_new[i] /= n_new;
    }
    printf("[EM_FCM] Initial Cardinality : %9.1f\n", n_new);
    printf("[EM_FCM] Max Counter value : %d\n", this->max_counter_value);
  };

private:
  double n_old,
      n_new; // cardinality
  uint32_t max_counter_value, max_degree;
  struct BetaGenerator {
    int sum;
    int now_flow_num;
    int flow_num_limit;
    vector<int> now_result;

    explicit BetaGenerator(int _sum) : sum(_sum) {
      now_flow_num = 0;

      if (sum > 600) {
        flow_num_limit = 2;
      } else if (sum > 250)
        flow_num_limit = 3;
      else if (sum > 100)
        flow_num_limit = 4;
      else if (sum > 50)
        flow_num_limit = 5;
      else
        flow_num_limit = 6;
    }

    bool get_new_comb() {
      for (int j = now_flow_num - 2; j >= 0; --j) {
        int t = ++now_result[j];
        for (int k = j + 1; k < now_flow_num - 1; ++k) {
          now_result[k] = t;
        }
        int partial_sum = 0;
        for (int k = 0; k < now_flow_num - 1; ++k) {
          partial_sum += now_result[k];
        }
        int remain = sum - partial_sum;
        if (remain >= now_result[now_flow_num - 2]) {
          now_result[now_flow_num - 1] = remain;
          return true;
        }
      }

      return false;
    }

    bool get_next() {
      while (now_flow_num <= flow_num_limit) {
        switch (now_flow_num) {
        case 0:
          now_flow_num = 1;
          now_result.resize(1);
          now_result[0] = sum;
          return true;
        case 1:
          now_flow_num = 2;
          now_result[0] = 0;
          // fallthrough
        default:
          now_result.resize(now_flow_num);
          if (get_new_comb()) {
            return true;
          } else {
            now_flow_num++;
            for (int i = 0; i < now_flow_num - 2; ++i) {
              now_result[i] = 1;
            }
            now_result[now_flow_num - 2] = 0;
          }
        }
      }

      return false;
    }
  };

  struct BetaGenerator_HD {
    int sum;                            // value
    int beta_degree;                    // degree
    vector<vector<uint32_t>> thres;     // list of <. . .>
    vector<uint32_t> counter_overflows; // List of overflow values per stage
    bool use_reduction;
    int now_flow_num;
    int flow_num_limit;
    vector<int> now_result;
    explicit BetaGenerator_HD(int _sum, vector<vector<uint32_t>> _thres,
                              vector<uint32_t> _overflows, int _degree)
        : sum(_sum), beta_degree(_degree), thres(_thres),
          counter_overflows(_overflows) {
      // must initialize now_flow_num as 0
      now_flow_num = 0;
      use_reduction = false;

      /**** For large dataset and cutoff ****/
      if (sum > 1100)
        flow_num_limit = beta_degree;
      else if (sum > 550)
        flow_num_limit = std::max(beta_degree, 3);
      else
        flow_num_limit = std::max(beta_degree, 4);

      // Note that the counters of degree D (> 1) includes at least D flows.
      // But, the combinatorial complexity also increases to O(N^D), which takes
      // a lot of time. To truncate the compexity, we introduce a simplified
      // version that prohibits the number of flows less than D. As
      // explained in the paper, its effect is quite limited as the number of
      // such counters are only a few.

      // 15k, 15k, 7k
      if (beta_degree >= 4 and sum < 10000) {
        use_reduction = true;
        flow_num_limit = 3;
        beta_degree = 3;
      } else if (beta_degree >= 4 and sum >= 10000) {
        use_reduction = true;
        flow_num_limit = 2;
        beta_degree = 2;
      } else if (beta_degree == 3 and sum > 5000) {
        use_reduction = true;
        flow_num_limit = 2;
        beta_degree = 2;
      }
    }

    bool get_new_comb() {
      // input: now, always flow num >= 2..
      for (int j = now_flow_num - 2; j >= 0; --j) {
        int t = ++now_result[j];
        for (int k = j + 1; k < now_flow_num - 1; ++k) {
          now_result[k] = t;
        }
        int partial_sum = 0;
        for (int k = 0; k < now_flow_num - 1; ++k) {
          partial_sum += now_result[k];
        }
        int remain = sum - partial_sum;
        if (remain >= now_result[now_flow_num - 2]) {
          now_result[now_flow_num - 1] = remain;
          return true;
        }
      }
      return false;
    }

    bool get_next() {
      // Need to test it is available combination or not, based on newsk_thres
      // (bayesian)
      while (now_flow_num <= flow_num_limit) {
        switch (now_flow_num) {
        case 0:
          now_flow_num = 1;
          now_result.resize(1);
          now_result[0] = sum;
          /*****************************/
          if (beta_degree == 0) // will never be occured
          {
            std::cout << "[ERROR] Beta degree is 0" << std::endl;
            return true;
          }
          /*****************************/
        case 1:
          now_flow_num = 2;
          now_result[0] = 0;
        default: // fall through
          now_result.resize(now_flow_num);
          if (get_new_comb()) {
            // Validate the combinations
            // There need to more or equal numbers to the degree of VC
            if (now_flow_num < beta_degree) {
              continue;
            }
            if (!use_reduction) {
              return false;
              if (condition_check_fcm()) {
                return true;
              }
              // Some flows are very larges and/or have
            } else {
              if (condition_check_fcm_reduction()) {
                return true;
              }
            }
          } else { // no more combination -> go to next flow number
            now_flow_num++;
            for (int i = 0; i < now_flow_num - 2; ++i) {
              now_result[i] = 1;
            }
            now_result[now_flow_num - 2] = 0;
          }
        }
      }
      // std::cout << "Correct combinations " << correct_combinations <<
      // std::endl;
      return false;
    }

    bool condition_check_fcm() {
      std::cout << " x ";
      if (beta_degree == now_flow_num) {
        for (auto &t : thres) {
          uint32_t colls = t[0];
          uint32_t min_val = t[1];
          uint32_t passes = 0;
          for (uint32_t i = 0; i < now_flow_num; ++i) {
            if (now_result[i] >= min_val) {
              passes++;
            }
          }
          // Combination has not large enough values to meet all conditions
          // E.g. it needs have 2 values large than the L2 threshold +
          // predecessor (min_value)
          if (passes < colls) {
            return false;
          }
        }
        // More flows than degrees, so the flows are split up and thus we need
        // to sum some of the values
      } else {
        for (auto &t : thres) {
          uint32_t colls = t[0];
          uint32_t min_val = t[1];
          uint32_t passes = 0;
          bool found_permutation = false;

          uint32_t delta = now_flow_num - beta_degree + 1;
          if (now_result[now_flow_num - 1] >= min_val) {

            vector<uint32_t> perms(now_result.size() - 1);
            for (size_t i = 0; i < perms.size(); i++) {
              perms[i] = now_result[i];
            }

            do {
              uint32_t local_passes = 1;
              uint32_t val =
                  std::accumulate(perms.begin(), perms.begin() + delta - 1, 0);
              if (val >= min_val) {
                local_passes++;
              }
              for (uint32_t i = delta; i < now_flow_num - 1; ++i) {
                if (perms[i] >= min_val) {
                  local_passes++;
                }
              }
              // Found a good permutation
              if (local_passes >= colls) {
                found_permutation = true;
                break;
              }
            } while (std::next_permutation(perms.begin(), perms.end()));
          } else {
            for (size_t delta_i = 2; delta_i <= delta; delta_i++) {
              // If the first deltas and remainder do not exceed the threshold
              // the generated combination is always wrong
              uint32_t val = std::accumulate(
                  now_result.begin(), now_result.begin() + delta_i - 1, 0);
              if (val + now_result[now_flow_num - 1] < min_val) {
                continue;
              }
              vector<uint32_t> perms(now_result.size() - delta_i);
              for (size_t i = delta_i; i < perms.size(); i++) {
                perms[i] = now_result[i];
              }

              bool found_permutation = false;
              do {
                uint32_t local_passes = 1;
                uint32_t val = std::accumulate(perms.begin(), perms.end(), 0);
                if (val >= min_val) {
                  local_passes++;
                }
                for (uint32_t i = delta_i; i < now_flow_num - 1; ++i) {
                  if (perms[i] >= min_val) {
                    local_passes++;
                  }
                }
                // Found a good permutation
                if (local_passes >= colls) {
                  found_permutation = true;
                  break;
                }
              } while (std::next_permutation(perms.begin(), perms.end()));
            }
          }
          // No possible permutation that holds for this threshold so
          // invalid combination
          if (!found_permutation) {
            return false;
          }
        }
      }
      std::cout << "y" << std::endl;
      return true; // if no violation, it is a good permutation!
    }

    bool condition_check_fcm_reduction() {
      // check number of flows should be more or equal than degree...
      if (now_flow_num < beta_degree)
        return false; // exit for next comb.

      // layer 1 check
      for (int i = 0; i < now_flow_num; ++i) {
        if (now_result[i] <= this->counter_overflows[0])
          return false;
      }
      // Run over threshold until more than 2 collisions are found and start
      // from there
      for (auto &t : thres) {
        uint32_t colls = t[0];
        colls = std::min((int)colls, beta_degree);
        uint32_t min_val = t[1];
        uint32_t passes = 0;
        for (uint32_t i = 0; i < now_flow_num; ++i) {
          if (now_result[i] >= min_val) {
            passes++;
          }
        }
        // Combination has not large enough values to meet all conditions
        // E.g. it needs have 2 values large than the L2 threshold +
        // predecessor (min_value)
        if (passes < colls) {
          // std::cout << "Didn't have enough passes, " << passes << " < "
          //           << colls << std::endl;
          return false;
        }
        // Exit early
        if (colls > 1) {
          break;
        }
      }
      return true;
    }
  };

  int factorial(int n) {
    if (n == 0 || n == 1)
      return 1;
    return factorial(n - 1) * n;
  }

  double get_p_from_beta(BetaGenerator &bt, double lambda,
                         vector<double> now_dist, double now_n) {
    std::unordered_map<uint32_t, uint32_t> mp;
    for (int i = 0; i < bt.now_flow_num; ++i) {
      mp[bt.now_result[i]]++;
    }

    double ret = std::exp(-lambda);
    for (auto &kv : mp) {
      uint32_t fi = kv.second;
      uint32_t si = kv.first;
      double lambda_i = now_n * (now_dist[si]) / w;
      ret *= (std::pow(lambda_i, fi)) / factorial(fi);
    }

    return ret;
  }

  double get_p_from_beta(BetaGenerator_HD &bt, double lambda,
                         vector<double> now_dist, double now_n) {
    std::unordered_map<uint32_t, uint32_t> mp;
    for (int i = 0; i < bt.now_flow_num; ++i) {
      mp[bt.now_result[i]]++;
    }

    double ret = std::exp(-lambda);
    for (auto &kv : mp) {
      uint32_t fi = kv.second;
      uint32_t si = kv.first;
      double lambda_i = now_n * (now_dist[si]) / w;
      ret *= (std::pow(lambda_i, fi)) / factorial(fi);
    }

    return ret;
  }

  void calculate_single_degree(vector<double> &nt, int d) {
    nt.resize(this->max_counter_value + 1);
    std::fill(nt.begin(), nt.end(), 0.0);

    printf("[EM_FCM] ******** Running for degree %2d with a size of %12zu\t\t"
           "**********\n",
           d, counter_dist[d].size());

    double lambda = n_old * d / double(w);
    for (uint32_t i = 0; i < counter_dist[d].size(); i++) {
      // enum how to form val:i
      if (counter_dist[d][i] == 0) {
        continue;
      }
      std::cout << this->counter_dist[d][i] << std::endl;
      BetaGenerator alpha(i), beta(i);
      double sum_p = 0;
      vector<double> imm_p;
      vector<vector<double>> imm_now_results;
      while (alpha.get_next()) {
        double p = get_p_from_beta(alpha, lambda, dist_old, n_old);
        sum_p += p;
        imm_p.push_back(p);
        vector<double> p_results(std::begin(alpha.now_result),
                                 std::end(alpha.now_result));
        imm_now_results.push_back(p_results);
      }
      std::cout << "p: ";
      for (auto &p : imm_p) {
        std::cout << p << " ";
      }
      std::cout << std::endl;
      std::cout << sum_p << std::endl;
      if (sum_p == 0.0) {
        imm_p.pop_back();
        imm_now_results.pop_back();
        continue;
      } else {
        for (size_t j = 0; j < imm_p.size(); j++) {
          double p = imm_p[j];
          for (size_t x = 0; x < imm_now_results[j].size(); x++) {
            nt[imm_now_results[j][x]] += counter_dist[d][i] * p / sum_p;
            std::cout << nt[imm_now_results[j][x]] << "+=" << counter_dist[d][i]
                      << "*" << p << "/" << sum_p << std::endl;
          }
        }
      }
    }

    double accum = std::accumulate(nt.begin(), nt.end(), 0.0);
    if (accum != accum) {
      std::cout << "Accum is Nan" << std::endl;
      for (auto &x : nt) {
        std::cout << x << " ";
      }
      std::cout << std::endl;
    }
    if (counter_dist[d].size() != 0)
      printf("[EM_FCM] ******** degree %2d is "
             "finished...(accum:%10.1f #val:%8d)\t**********\n",
             d, accum, (int)this->counters[d].size());
  }

  void calculate_higher_degree(vector<double> &nt, int d) {
    nt.resize(this->max_counter_value + 1);
    std::fill(nt.begin(), nt.end(), 0.0);

    printf("[EM_FCM] ******** Running for degree %2d with a size of %12zu\t\t"
           "**********\n",
           d, counter_dist[d].size());

    double lambda = n_old * d / double(w);
    for (uint32_t i = 0; i < counter_dist[d].size(); i++) {
      // enum how to form val:i
      if (counter_dist[d][i] == 0) {
        continue;
      }
      BetaGenerator_HD alpha(i, this->thresholds[d][i], this->stage_sz, d),
          beta(i, this->thresholds[d][i], this->stage_sz, d);
      double sum_p = 0;
      uint32_t iter = 0;
      while (alpha.get_next()) {
        double p = get_p_from_beta(alpha, lambda, dist_old, n_old);
        sum_p += p;
        iter++;
      }
      if (sum_p == 0) {
        if (iter > 0) {
          uint32_t temp_val = this->counters[d][i];
          if (temp_val == 0) {
            nt[temp_val]++;
            continue;
          }
          vector<vector<uint32_t>> temp_thresh = this->thresholds[d][i];
          for (auto &t : temp_thresh) {
            if (temp_val < t[1]) {
              break;
            }
            temp_val -= t[1];
          }
          nt[temp_val] += 1;
        }
      } else {
        while (beta.get_next()) {
          double p = get_p_from_beta(beta, lambda, dist_old, n_old);
          for (int j = 0; j < beta.now_flow_num; ++j) {
            nt[beta.now_result[j]] += counter_dist[d][i] * p / sum_p;
            std::cout << nt[beta.now_result[j]] << std::endl;
          }
        }
      }
    }
    double accum = std::accumulate(nt.begin(), nt.end(), 0.0);
    if (counter_dist[d].size() != 0)
      printf("[EM_FCM] ******** degree %2d is "
             "finished...(accum:%10.1f #val:%8zu)\t**********\n",
             d, accum, this->counters[d].size());
  }

public:
  void next_epoch() {
    auto start = std::chrono::high_resolution_clock::now();
    double lambda = n_old / double(w);
    dist_old = dist_new;
    n_old = n_new;

    vector<vector<double>> nt;
    nt.resize(max_degree + 1);
    std::fill(ns.begin(), ns.end(), 0);

    // Simple Multi thread
    // std::thread threads[max_degree + 1];
    // for (size_t t = 0; t <= max_degree; t++) {
    //   std::cout << "Starting degree " << t << std::endl;
    //   if (t == 0) {
    //     threads[t] = std::thread(&EMFSD::calculate_single_degree, *this,
    //                              std::ref(nt[t]), t);
    //   } else {
    //     threads[t] = std::thread(&EMFSD::calculate_higher_degree, *this,
    //                              std::ref(nt[t]), t);
    //   }
    // }
    // for (size_t t = 0; t <= max_degree; t++) {
    //   threads[t].join();
    // }

    // Single threaded
    for (size_t d = 0; d <= max_degree; d++) {
      if (d == 0) {
        this->calculate_single_degree(nt[d], d);
      } else {
        this->calculate_higher_degree(nt[d], d);
      }
    }

    n_new = 0.0;
    for (size_t d = 0; d <= max_degree; d++) {
      for (uint32_t i = 0; i < max_counter_value; i++) {
        ns[i] += nt[d][i];
        n_new += nt[d][i];
      }
    }
    for (uint32_t i = 0; i < max_counter_value; i++) {
      dist_new[i] = ns[i] / n_new;
    }
    auto stop = std::chrono::high_resolution_clock::now();
    auto time = duration_cast<std::chrono::milliseconds>(stop - start);

    printf("[EM_FCM - iter %2d] Compute time : %li\n", iter, time.count());
    printf("[EM_FCM - iter %2d] Intermediate cardianlity : %9.1f\n\n", iter,
           n_new);
    iter++;
  }
};
#endif
