
Supplemental Cyclus Archetypes
===============================

Run ``make install`` to build and install these archetypes into
``.local/lib/cyclus`` which will need to be in your CYCLUS_PATH environment
variable.  The archetypes are built into a single library named ``rwc``.
Included archetypes are:

* **LookInst**: This archetype allows the user to specify an arbitrary power
  capacity time series that the institution will automatically deploy power
  generating facilities (of multiple types) to match.  It uses a look-ahead
  mechanism to see if its chosen deployments were not "good" and adjusts its
  deployment decisions accordingly. This archetype currently requires
  ``rwcarlsen/cyclus#restart``
  branch of Cyclus to work.

* **FleetReactor**: This reactor models an entire reactor fleet as a single,
  homogenous unit.  Like the Cycamore reactor, it just uses static,
  user-specified compositions for fresh and spent fuel. Refueling is
  incremental - occuring every time step.  It is approximately a continuous
  flow fleet model - much like the way system-dynamics simulators represent
  facilities.  It "pretends" to be many reactors to the Cyclus kernel -
  allowing other agents to deploy/decommission single-reactor units, but these
  single-reactors just adjust the size/capacity of the homogenous fleet.

* **Storage**: This facility is very similar to the Cycamore storage facility,
  and was originally created before Cycamore's existed.  It has slightly more
  careful handling of discrete material objects (e.g. not ever splitting them)
  and is a bit more sophisticated with respect to resource exchange for
  offering/bidding its inventory.  It would be good to use implementation
  details from this archetype to improve the Cycamore storage archetype.

* **PatternSink**: Pattern sink is identical to the Cycamore sink facility
  except it has an additional parameter that allows the user to control the
  frequency with which the facility requests material - i.e. the user can tell
  the facility to only request material every Nth time step.

