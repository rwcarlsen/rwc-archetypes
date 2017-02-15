#include "pattern_sink.h"

#include <algorithm>
#include <sstream>
#include <boost/lexical_cast.hpp>

namespace rwc {

PatternSink::PatternSink(cyclus::Context* ctx)
    : cyclus::Facility(ctx),
      capacity(std::numeric_limits<double>::max()) {
    inventory.capacity(std::numeric_limits<double>::max());
}

PatternSink::~PatternSink() {}

std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr>
PatternSink::GetMatlRequests() {
  using cyclus::Material;
  using cyclus::RequestPortfolio;
  using cyclus::Request;
  using cyclus::Composition;

  std::set<RequestPortfolio<Material>::Ptr> ports;
  if (inactive()) {
    return ports;
  }

  RequestPortfolio<Material>::Ptr port(new RequestPortfolio<Material>());
  double amt = std::min(capacity, std::max(0.0, inventory.space()));
  Material::Ptr mat;

  if (recipe_name.empty()) {
    mat = cyclus::NewBlankMaterial(amt);
  } else {
    Composition::Ptr rec = this->context()->GetRecipe(recipe_name);
    mat = cyclus::Material::CreateUntracked(amt, rec); 
  } 

  if (amt > cyclus::eps()) {
    std::vector<std::string>::const_iterator it;
    std::vector<Request<Material>*> mutuals;
    for (it = in_commods.begin(); it != in_commods.end(); ++it) {
      mutuals.push_back(port->AddRequest(mat, this, *it, pref));
    }
    port->AddMutualReqs(mutuals);
    ports.insert(port);
  }  // if amt > eps

  return ports;
}

void PatternSink::AcceptMatlTrades(
    const std::vector< std::pair<cyclus::Trade<cyclus::Material>,
                                 cyclus::Material::Ptr> >& responses) {
  std::vector< std::pair<cyclus::Trade<cyclus::Material>,
                         cyclus::Material::Ptr> >::const_iterator it;
  for (it = responses.begin(); it != responses.end(); ++it) {
    inventory.Push(it->second);
  }
}

extern "C" cyclus::Agent* ConstructPatternSink(cyclus::Context* ctx) {
  return new PatternSink(ctx);
}

}  // namespace rwc
