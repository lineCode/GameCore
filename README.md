# UEHelperLibraries
Unreal Engine C++ plugin containing static helper functions


## BFL_CollisionQueries.h
All collision queries are generic to both line traces and shape sweeps.

### Exit hit scene casting
Exit hits are useful in cases where you need the other side of the geometry that was hit. We achieve this by performing a second scene cast in the opposite direction. This can get expensive as your query length is effectively doubling. To mitigate this, a built in optimization is provided to minimize the backwards query length down to its shortest guarenteed working length.

### Penetration scene casting
Penetration through blocking hits is useful for when your querying with a trace channel but do not want to be stopped by the trace channel\'s blocking hits. This is a good way to observe what types of hit responses are within your query\'s area.
Given a situation where you need a scene cast that goes through walls (e.g. a bullet), you may make a trace channel that overlaps walls. However, if you want to keep the distinction between physical walls and trigger boxes, for example, getting overlap responses for both won\'t help. Instead, you want an overlap response for the trigger box and a blocking response for the wall. This penetration feature makes this distinction possible while penetrating through the blocking hits.

## BFL_StrengthCollisionQueries.h

