#ifndef CYCAMORE_SRC_STORAGE_H_
#define CYCAMORE_SRC_STORAGE_H_

#include "cyclus.h"
#include "rwc_version.h"

namespace cycamore {

class Storage : public cyclus::Facility {
 public:
  Storage(cyclus::Context* ctx);
  virtual ~Storage(){};
  virtual std::string version() {return RWC_VERSION;};

  virtual void Tick();

  virtual void Tock() {}

  virtual void AcceptMatlTrades(const std::vector<std::pair<
      cyclus::Trade<cyclus::Material>, cyclus::Material::Ptr> >& responses);

  virtual std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr>
  GetMatlRequests();

  virtual std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr> GetMatlBids(
      cyclus::CommodMap<cyclus::Material>::type& commod_requests);

  virtual void GetMatlTrades(
      const std::vector<cyclus::Trade<cyclus::Material> >& trades,
      std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                            cyclus::Material::Ptr> >& responses);

  #pragma cyclus clone
  #pragma cyclus initfromcopy
  #pragma cyclus infiletodb
  #pragma cyclus initfromdb
  #pragma cyclus schema
  #pragma cyclus annotations
  #pragma cyclus snapshot
  virtual cyclus::Inventories SnapshotInv();
  virtual void InitInv(cyclus::Inventories& inv);

 private:
  void ReadyPush(std::vector<cyclus::Material::Ptr> mats);
  cyclus::Material::Ptr ReadyPop(int obj_id);
  cyclus::Material::Ptr ReadyPopQty(double qty);

  #pragma cyclus var { \
    "doc": "Name for recipe to be used in requests." \
           " Empty string results in use of an empty dummy recipe.", \
    "uitype": "recipe", \
    "default": "", \
  }
  std::string inrecipe;

  #pragma cyclus var { \
  }
  std::string incommod;

  #pragma cyclus var { \
  }
  std::string outcommod;
  
  #pragma cyclus var { \
    "internal": True, \
    "default": {}, \
  }
  std::map<int, int> time_npop;

  #pragma cyclus var { \
  }
  int wait_time;

  #pragma cyclus var { \
    "default" : 1e299, \
  }
  double invsize;

  #pragma cyclus var { \
    "default" : True, \
  }
  bool discrete;

  #pragma cyclus var { \
    "internal": True, \
    "default" : 1e299, \
  }
  double waitingbuf_size;

  double readyqty_;
  std::map<int, cyclus::Material::Ptr> ready_;

  #pragma cyclus var { \
    "capacity" : "waitingbuf_size", \
  }
  cyclus::toolkit::ResBuf<cyclus::Material> waiting;
};

}  // namespace cycamore

#endif  // CYCAMORE_SRC_STORAGE_H_
