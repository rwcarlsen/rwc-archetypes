#include "look_inst.h"
#include "sim_init.h"

using cyclus::SimInit;
using cyclus::SqliteDb;
using cyclus::SqlStatement;
using cyclus::SqliteBack;

namespace cycamore {

LookInst::LookInst(cyclus::Context* ctx) : cyclus::Institution(ctx), rec_((unsigned int)2) {
  if (!am_ghost_) {
    ctx->CloneSim(); // guarantee we get clone for time zero
  }
}

LookInst::~LookInst() { }

bool LookInst::am_ghost_ = false;

void LookInst::Tock() {
  cyclus::Institution::Tock();
  if (am_ghost_) {
    return;
  }

  int t = context()->time();
  int deploy_t = t + 1;
  int period = PeriodOf(deploy_t);
  if (OnDeploy(t + 2)) {
    context()->CloneSim();
    return;
  } else if (!OnDeploy(t + 1)) {
    return;
  }

  CalcReqBuilds(deploy_t);

  // deciding deploy ratios not necessary for only one facility type
  bool done = proto_priority.size() == 1;
  int iter = 0;
  while (!done) {
    iter++;
    Short fall = CalcShortfall(deploy_t);
    if (fall.shortfall > 1e-3 ) {
      done = UpdateNbuild(deploy_t, fall);
    } else {
      done = true;
    }
  }

  // only schedule new/computed builds for the closest coming build period -
  // the others may still change more.
  for (int i = 0; i < nbuilds[period].size(); i++) {
    for (int k = 0; k < nbuilds[period][i]; k++) {
      context()->SchedBuild(this, proto_priority[i], deploy_t);
    }
  }

  context()->NewDatum("LookInstIters")
    ->AddVal("Time", context()->time())
    ->AddVal("AgentId", id())
    ->AddVal("NSims", iter)
    ->Record();
}

// calculate integrated shortfall over the entire lookahead window
Short LookInst::CalcShortfall(int deploy_t) {
  SqliteBack memback(":memory:");
  RunSim(&memback, deploy_t);

  Short fall = {0, 0};

  for (int look = 0; look < lookto(deploy_t); look++) {
    int t_check = deploy_t + look;
    double power = CapAt(memback.db(), t_check);
    double newshort = std::max(0.0, WantCap(t_check) - power);
    if (fall.shortfall == 0 && newshort > 1e-6) {
      fall.start_period = PeriodOf(t_check);
    }
    fall.shortfall += newshort;
  }
  return fall;
}

// calculate min req new capacity for growth and to replace retiring reactors
void LookInst::CalcReqBuilds(int deploy_t) {
  SqliteBack memback(":memory:");
  std::map<int, double> initcaps = RunSim(&memback, deploy_t);

  // newcap tracks how much we've added in total capacity compared to the
  // simulation stored in memback. 
  double newcap = 0;

  for (int p = PeriodOf(deploy_t); p <= PeriodOf(deploy_t + lookahead); p++) {
    int t = TimeOf(p);

    double slow_cap = context()->n_prototypes("slow_reactor") * 900;
    double fast_cap = context()->n_prototypes("fast_reactor") * 360;
    // we assume all new capacity we add in this for loop will be operating
    // for the duration of the lookahead window.  So we need to subtract off
    // the already just added newcap from the shortfall here in order to
    // calculate the correct new capacity to add for this deploy period.
    double growth_cap = WantCap(t) - NewCapBy(memback.db(), t)-initcaps[t] - newcap;
    growth_cap = std::max(0.0, growth_cap);

    // a running tally of new growth capacity being deployed by this agent is
    // necessary in order to know how many facilities of lower priority type
    // to deploy to replace 1 facility of higher facility type.  This matters
    // because of rounding issues that may result from mismatched capacity
    // production between the different facility types.  growth_cap only
    // includes new capacity that needs to be added this tock - it doesn't
    // include computed capacity needed to be added from previous tocks (i.e.
    // if the lookahead period is longer than the deploy period).  So we must
    // accumulate and add current calculated growth_cap to growths.
    growths[p] += growth_cap;

    std::vector<int> nbuild(proto_priority.size(), 0);
    if (p < nbuilds.size()) {
      nbuild = nbuilds[p];
    }
    for (int i = 0; i < proto_avail.size(); i++) {
      if (proto_avail[i] <= t) {
        int nadd = static_cast<int>(ceil(growth_cap / proto_cap[i]));
        nbuild[i] += nadd;
        newcap += nadd * proto_cap[i];
        break;
      }
    }

    if (p < nbuilds.size()) {
      nbuilds[p] = nbuild;
    } else {
      nbuilds.push_back(nbuild);
    }
  }
}

std::map<int, double> LookInst::RunSim(SqliteBack* b, int deploy_t) {
  am_ghost_ = true;
  if (rec_.dump_count() < 500) {
    rec_.set_dump_count(500);
  }
  cyclus::LogLevel lev = cyclus::Logger::ReportLevel();
  cyclus::Logger::ReportLevel() = cyclus::LEV_ERROR;

  double look = lookto(deploy_t);
  int until_t = deploy_t + look;

  SimInit si;
  si.Init(&rec_, context()->GetClone(), until_t);
  rec_.RegisterBackend(b);
  for (int period = PeriodOf(deploy_t); period < nbuilds.size(); period++) {
    std::vector<int> nbuild = nbuilds[period];
    int t = TimeOf(period);
    for (int i = 0; i < nbuild.size(); i++) {
      for (int k = 0; k < nbuild[i]; k++) {
        // if we use 'this' as parent, then this will be owned and deallocated
        // by multiple conterxts - BAD
        // TODO: figure out a way to not have to use NULL for parent here.
        si.context()->SchedBuild(NULL, proto_priority[i], t);
      }
    }
  }

  std::map<std::string, double> protocap;
  for (int i = 0; i < proto_cap.size(); i++) {
    protocap[proto_priority[i]] = proto_cap[i];
  }
  std::vector<Agent*> ags = si.agents();
  int nperiods = ceil((double)look/(double)deploy_period);
  std::map<int, double> initcaps;
  int start_period = PeriodOf(deploy_t);
  for (int period = start_period; period < start_period + nperiods; period++) {
    int t = TimeOf(period);
    // compute initially deployed capacity
    double totcap = 0;
    for (int i = 0; i < ags.size(); i++) {
      Agent* a = ags[i];
      int exit = a->exit_time();
      if ((exit == -1 || exit >= t) && protocap.count(a->prototype()) > 0) {
        totcap += protocap[ags[i]->prototype()];
      }
    }
    initcaps[t] = totcap;
  }

  // run sim and clean up
  si.timer()->RunSim();
  rec_.Flush();
  rec_.Close();

  cyclus::Logger::ReportLevel() = lev;
  am_ghost_ = false;

  return initcaps;
}

bool LookInst::UpdateNbuild(int deploy_t, Short fall) {
  int period = PeriodOf(deploy_t);
  bool changed = false;
  double look = lookto(deploy_t);
  for (int p = period; p <= PeriodOf(deploy_t + look); p++) {
    std::vector<int> nbuild = nbuilds[p];
    double growth_cap = growths[p];

    for (int i = 0; i < nbuild.size() - 1; i++) {
      if (nbuild[i] == 0) {
        continue;
      }

      // adjust toward a lower priority facility type
      changed = true;
      nbuild[i] -= 1;
      int nadd = static_cast<int>(proto_cap[i] / proto_cap[i + 1]);
      nbuild[i + 1] += nadd;
      if (CapOf(nbuild) < growth_cap) {
        nadd++;
        nbuild[i + 1] += 1;
      }
      break;
    }

    if (changed) {
      nbuilds[p] = nbuild;
      break;
    }
  }
  return !changed;
}

int LookInst::TimeOf(int period) {
  return enter_time() + 1 + period * deploy_period;
};

int LookInst::PeriodOf(int t) {
  return (t - enter_time() - 1) / deploy_period;
};

double LookInst::WantCap(int t) {
  return curve[PeriodOf(t)];
}

bool LookInst::OnDeploy(int t) {
  return (t - enter_time() - 1) % deploy_period == 0;
};

double LookInst::CapOf(std::vector<int> nbuild) {
  double totcap = 0;
  for (int i = 0; i < nbuild.size(); i++) {
    totcap += nbuild[i] * proto_cap[i];
  }
  return totcap;
}

double LookInst::CapAt(SqliteDb& db, int t) {
  double val = 0;
  try {
    SqlStatement::Ptr stmt = db.Prepare("SELECT SUM(Value) FROM TimeSeries" + captable + " WHERE Time = ?");
    stmt->BindInt(1, t);
    stmt->Step();
    val = stmt->GetDouble(0);
  } catch(cyclus::IOError err) { }
  return val;
}

double LookInst::NewCapBy(SqliteDb& db, int t) {
  double totcap = 0;
  for (int i = 0; i < proto_priority.size(); i++) {
    std::string proto = proto_priority[i];
    double cap = proto_cap[i];

    try {
      SqlStatement::Ptr stmt = db.Prepare("SELECT count(*) FROM AgentEntry WHERE Prototype = ? AND EnterTime <= ? AND (? < EnterTime+Lifetime OR Lifetime=-1)");
      stmt->BindText(1, proto.c_str());
      stmt->BindInt(2, t);
      stmt->BindInt(3, t);
      stmt->Step();
      totcap += stmt->GetDouble(0) * cap;
    } catch(cyclus::IOError err) { }
  }
  return totcap;
}

void LookInst::EnterNotify() {
  cyclus::Institution::EnterNotify();
  int dur = context()->sim_info().duration;
  int nperiods = (dur - 2) / deploy_period + 1;
  if (curve.size() < nperiods) {
    std::stringstream ss;
    ss << "prototype '" << prototype() << "' has " << curve.size()
       << " curve capacity vals, expected >= " << nperiods;
    throw cyclus::ValidationError(ss.str());
  } else if (proto_priority.size() != proto_cap.size()) {
    std::stringstream ss;
    ss << "prototype '" << prototype() << "' has " << proto_cap.size()
       << " proto_cap vals, expected " << proto_priority.size();
    throw cyclus::ValidationError(ss.str());
  } else if (proto_avail.size() > 0 && proto_priority.size() != proto_avail.size()) {
    std::stringstream ss;
    ss << "prototype '" << prototype() << "' has " << proto_avail.size()
       << " proto_avail vals, expected " << proto_priority.size();
    throw cyclus::ValidationError(ss.str());
  }

  if (proto_avail.size() == 0) {
    for (int i = 0; i < proto_priority.size(); i++) {
      proto_avail.push_back(0);
    }
  }
}

extern "C" cyclus::Agent* ConstructLookInst(cyclus::Context* ctx) {
  return new LookInst(ctx);
}

}  // namespace cycamore
