#ifndef RWC_PATTERN_SINK_H_
#define RWC_PATTERN_SINK_H_

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "cyclus.h"
#include "rwc_version.h"

namespace rwc {

class Context;

class PatternSink : public cyclus::Facility  {
 public:

  PatternSink(cyclus::Context* ctx);
  virtual ~PatternSink();
  virtual std::string version() {return RWC_VERSION;};

  #pragma cyclus note { \
    "doc": "" \
    }

  #pragma cyclus

  virtual void Tick() {};
  virtual void Tock() {};

  virtual std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr>
      GetMatlRequests();

  virtual void AcceptMatlTrades(
      const std::vector< std::pair<cyclus::Trade<cyclus::Material>,
      cyclus::Material::Ptr> >& responses);

 private:

  bool inactive() { return (context()->time() - enter_time()) % every_n != 0; }


  #pragma cyclus var {"tooltip": "input commodities", \
                      "doc": "commodities that the sink facility accepts", \
                      "uilabel": "List of Input Commodities", \
                      "uitype": ["oneormore", "incommodity"]}
  std::vector<std::string> in_commods;

  #pragma cyclus var {"default": "", "tooltip": "requested composition", \
                      "doc": "name of recipe to use for material requests, " \
                             "where the default (empty string) is to accept " \
                             "everything", \
                       "uilabel": "Input Recipe", \
                      "uitype": "recipe"}
  std::string recipe_name;

  #pragma cyclus var {"default": 1e299, \
                      "tooltip": "sink maximum inventory size", \
                      "uilabel": "Maximum Inventory", \
                      "doc": "total maximum inventory size of sink facility"}
  double max_inv_size;

  /// monthly acceptance capacity
  #pragma cyclus var {"default": 1e299, "tooltip": "sink capacity", \
                      "uilabel": "Maximum Throughput", \
                      "doc": "capacity the sink facility can " \
                             "accept at each time step"}
  double capacity;

  #pragma cyclus var { \
    "default": 1.0, \
    "doc": "request preference", \
  }
  double pref;

  #pragma cyclus var { \
    "doc": "only request ever nth timestep.", \
    "default": 1, \
  }
  int every_n;

  /// this facility holds material in storage.
  #pragma cyclus var {'capacity': 'max_inv_size'}
  cyclus::toolkit::ResBuf<cyclus::Resource> inventory;
};

}  // namespace rwc

#endif  // RWC_PATTERN_SINK_H_
