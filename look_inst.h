#ifndef CYCAMORE_SRC_LOOK_INST_H_
#define CYCAMORE_SRC_LOOK_INST_H_

#include <utility>
#include <set>
#include <map>

#include "cyclus.h"
#include "rwc_version.h"

namespace cycamore {

struct Short {
  int start_period;
  double shortfall;
};

/// The LookInst automatically deploys facilities in an attempt to efficiently
/// supply a user-specified demand curve.  Although demand can be set to
/// anything by the user, electrical power/energy production is a more common
/// usage.  The user specifies one or more prototypes that can be deployed in
/// order to satisfy demand in a priority order.  The LookInst makes deployment
/// decisions by running mini-simulations initialized by cloning the current
/// simulation state and running the mini-simulation ahead some lookahead
/// duration.  The TimeSeries table specified by the user is checked to
/// determine any production shortfall compared to the user-specified demand
/// curve.  Deployments are then shifted to lower priority prototypes as
/// necessary in order to reduce shortfall as much as possible.
///
/// Because the LookInst runs many internal simulations, it is slower than
/// running a simulation using similar deployments without the LookInst.  The
/// size of this performance penalty varies depending on simulation details,
/// but a rough rule of thumb is ``10 * lookahead / deploy_period`` as a
/// multiplier on the simulation time when run with static deployments (e.g.
/// using the DeployInst).  If a growth simulation runs in 20 seconds with the
/// DeployInst, then a 24 month lookahead with annual deployments will result
/// in an approximate 400 second simulation run time.  
///
/// Due to limitations of how the LookInst operates, facilities that have
/// intermittent offline periods (e.g. reactors refueling) can cause
/// inefficient or undesired deployment decisions to occur. For example, if
/// reactors have a non-zero refueling period where they are not producing
/// electricity, then if their cycles are not staggered well, then periods with
/// more reactors offline refueling will cause the LookInst to shift many
/// deployments to lower priority prototypes unfruitfully in an attempt to meet
/// demand with little to no effect.  This will result in undesired deployment
/// decisions in addition to a significant performance impact (i.e.  slower
/// simulations).  Ways to avoid this problem include:
///    
/// - making sure online/offline cycles and the deploy period have a large
///   least common multiple for better staggering on average.  For example,
///   make a reactor cycle (including refueling time) be something like 19 if
///   the deploy period is 12 months - if reactors are deployed annually, you
///   get 19 separate deployments before reactor cycles start to overlap.
///
/// - eliminate offline periods (with no production of e.g. power) effectively
///   increasing capacity factor to 100% and compensate by reducting production
///   capacity time unit accordingly.  For example, reduce reactor power capacity
///   to 900 MWe from 1000 MWe and reduce refueling time in the 20 month cycle
///   from 2 months to 0 months.
class LookInst : public cyclus::Institution {
#pragma cyclus note { \
  "doc": \
    "The LookInst automatically deploys facilities in an attempt to efficiently\n" \
    " supply a user-specified demand curve.  Although demand can be set to\n" \
    " anything by the user, electrical power/energy production is a more common\n" \
    " usage.  The user specifies one or more prototypes that can be deployed in\n" \
    " order to satisfy demand in a priority order.  The LookInst makes deployment\n" \
    " decisions by running mini-simulations initialized by cloning the current\n" \
    " simulation state and running the mini-simulation ahead some lookahead\n" \
    " duration.  The TimeSeries table specified by the user is checked to\n" \
    " determine any production shortfall compared to the user-specified demand\n" \
    " curve.  Deployments are then shifted to lower priority prototypes as\n" \
    " necessary in order to reduce shortfall as much as possible.\n" \
    "\n\n" \
    "Because the LookInst runs many internal simulations, it is slower than\n" \
    " running a simulation using similar deployments without the LookInst.  The\n" \
    " size of this performance penalty varies depending on simulation details,\n" \
    " but a rough rule of thumb is ``10 * lookahead / deploy_period`` as a\n" \
    " multiplier on the simulation time when run with static deployments (e.g.\n" \
    " using the DeployInst).  If a growth simulation runs in 20 seconds with the\n" \
    " DeployInst, then a 24 month lookahead with annual deployments will result\n" \
    " in an approximate 400 second simulation run time.  \n" \
    "\n\n" \
    "Due to limitations of how the LookInst operates, facilities that have\n" \
    " intermittent offline periods (e.g. reactors refueling) can cause\n" \
    " inefficient or undesired deployment decisions to occur. For example, if\n" \
    " reactors have a non-zero refueling period where they are not producing\n" \
    " electricity, then if their cycles are not staggered well, then periods with\n" \
    " more reactors offline refueling will cause the LookInst to shift many\n" \
    " deployments to lower priority prototypes unfruitfully in an attempt to meet\n" \
    " demand with little to no effect.  This will result in undesired deployment\n" \
    " decisions in addition to a significant performance impact (i.e.  slower\n" \
    " simulations).  Ways to avoid this problem include:\n" \
    "\n\n" \
    "- making sure online/offline cycles and the deploy period have a large\n" \
    "  least common multiple for better staggering on average.  For example,\n" \
    "  make a reactor cycle (including refueling time) be something like 19 if\n" \
    "  the deploy period is 12 months - if reactors are deployed annually, you\n" \
    "  get 19 separate deployments before reactor cycles start to overlap.\n" \
    "\n\n" \
    "- eliminate offline periods (with no production of e.g. power) effectively\n" \
    "  increasing capacity factor to 100% and compensate by reducting production\n" \
    "  capacity time unit accordingly.  For example, reduce reactor power capacity\n" \
    "  to 900 MWe from 1000 MWe and reduce refueling time in the 20 month cycle\n" \
    "  from 2 months to 0 months.\n" \
    "", \
}

 public:
  LookInst(cyclus::Context* ctx);
  virtual ~LookInst();
  virtual std::string version() {return RWC_VERSION;};

  #pragma cyclus

  virtual void EnterNotify();

  virtual void Tock();

 private:
  // this is set to true when a sub-simulation is being run so that the
  // sub simulation's LookInst doesn't waste CPU cycles computing deployments
  // and also potentially causing an infinite recursion.
  static bool am_ghost_;

  // Runs a simulation from the current state through the lookahead window and
  // returns the total capacity initially deployed at the start of the
  // copied/cloned simulation moment (i.e. deploy_t-1) and each other deploy
  // period time in the minisim.  The map key is the simulation time.  The
  // returned value allows the calculation of how much additional capacity
  // needs to be deployed.
  std::map<int, double> RunSim(cyclus::SqliteBack* b, int deploy_t);

  // lookto returns the end time of the current lookahead duration accounting
  // for end-of-simulation where the lookahead window must be shrunk to not
  // extend past the simulation's last time step.
  int lookto(int deploy_t) {
    return std::min(lookahead, context()->sim_info().duration - deploy_t);
  }


  bool UpdateNbuild(int deploy_t, Short fall);

  // Computes the integrated shortfall of capacity-months by running a sub
  // simulation forward from the given deployment time.
  Short CalcShortfall(int deploy_t);

  // Computes the number of new builds required to compensate for retirements
  // and to satisfy demand growth for every deploy period within the lookahead
  // window starting at the given deployment time.  The result is stored
  // inside the member variable nbuilds.
  void CalcReqBuilds(int deploy_t);

  int TimeOf(int period);

  int PeriodOf(int t);

  double WantCap(int t);

  // Returns true if t is a deployment period time step.
  bool OnDeploy(int t);

  // Returns the capacity associated with the given set of builds - nbuild is
  // an ordered list of build numbers of each prototype defined in
  // proto_priority.
  double CapOf(std::vector<int> nbuild);

  // Returns the quantity of the demanded item that was produced at time t in
  // the simulation data stored in db.
  double CapAt(cyclus::SqliteDb& db, int t);

  // NewCapBy returns the total capacity that was operating at time t in the
  // simulation data stored in db.  Since the sub simulations do not include
  // entries from agents that were existing in the initial context clone, this
  // only returns new capacity deployed following the start of the sub
  // simulation in db and prior to t. 
  double NewCapBy(cyclus::SqliteDb& db, int t);

  #pragma cyclus var { \
    "doc": \
      "A list of desired production levels at each deploy period that available prototypes will be deployed to meet." \
      " The first deploy period starts on the time step *after* the LookInst is built." \
      " The number of entries in the curve must be equal to or greater than" \
      " the number of deploy periods in the simuation: ``floor((sim_dur - 2) / deploy_period) + 1``." \
      "", \
  }
  std::vector<double> curve;

  #pragma cyclus var { \
    "doc": \
      "The duration of time in between deployments made to meet the defined" \
      " production curve." \
      "", \
    "units": "time steps", \
  }
  int deploy_period;

  #pragma cyclus var { \
    "doc": \
      "The duration of time ahead of current time step for which under-capacity" \
      " production is checked." \
      " This should usually be near or larger than the deploy_period." \
      "", \
    "units": "time steps", \
  }
  int lookahead;

  // name of timeseries table to get caps from (e.g. Power, etc.)
  #pragma cyclus var { \
    "default": "Power", \
  }
  std::string captable;

  #pragma cyclus var { \
    "doc": \
      "This is an ordered list of prototypes that can/will be deployed to meet" \
      " the defined production curve." \
      " Prototypes earlier in this list are built preferentially before" \
      " prototypes later in this list." \
      " All prototypes that contribute capacity (i.e. add entries to the" \
      " captable time series) must receive an entry in proto_priority - even if" \
      " they are not available for building.  Prototypes must be in order of" \
      " availability - latest available to soonest available." \
      "", \
  }
  std::vector<std::string> proto_priority;
  #pragma cyclus var { \
    "doc": \
      "This is an ordered list of the capacity contribution of each prototype" \
      " defined in proto_priority (same order as proto_priority)." \
      " The capacity should generally include any capacity factor associated" \
      " with the prototype's actual capacity.  For example, if a reactor" \
      " produces 1000 MWe when operating, but refuels for 2 months out of every" \
      " 18, then the capacity value entered here should be ``1000 MWe / 9``." \
      "", \
  }
  std::vector<double> proto_cap;
  #pragma cyclus var { \
    "default": [], \
    "doc": \
      "This is an ordered list of the time step on which each prototype becomes" \
      " available for building (same order as proto_priority)." \
      " If omitted, all prototypes are always available for building." \
      " If a prototype is not ever available for building, use a time step" \
      " after the end of the simulation. Note that because prototypes should" \
      " be in order of availability - this list should be from highest time" \
      " step to lowest." \
      "", \
  }
  std::vector<int> proto_avail;

  #pragma cyclus var { \
    "default": [], \
    "internal": True, \
  }
  std::vector<std::vector<int> > nbuilds;

  // growths maps the deploy period to the amount of new capacity the lookinst
  // must deploy in order for simulation total deployed capacity to match the
  // capacity demand curve. This accounts for replacement of retiring capacity as
  // well as capacity required to satisfy growth.  This does NOT account for
  // lower than 100% capacity factors on operating facilities.
  #pragma cyclus var { \
    "default": {}, \
    "internal": True, \
  }
  std::map<int, double> growths;

  // this is the recorder used in all of the sub simulations
  cyclus::Recorder rec_;
};

}  // namespace cycamore

#endif  // CYCAMORE_SRC_LOOK_INST_H_
