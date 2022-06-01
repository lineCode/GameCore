# UEHelperLibraries
Unreal Engine C++ plugin containing static helper functions

## BFL_CollisionQueries.h
This provides general purpose collision queries. Currently it provides 2 main functionalities
- Provides scene casts that return exit hits of geometry.
- Gives ability to do scene casts that can penetrate blocking hits.

All collision queries are generic to both line traces and shape sweeps.

Exit hits are useful in cases where you need the other side of the geometry that was hit. We achieve this by performing a second scene cast in the opposite direction. This can get expensive as your query length increases since it is basically doubling your query area. This is why we built an optimization to minimize the backwards query length down to its shortest guarenteed working length. 

Penetration through blocking hits is useful for when your querying with a trace channel but do not want to be stopped by the trace channel's blocking hits. This is a good way to observe what types of hit responses are within your query's area. It's also useful for cases where an extra ECollisionResponse type is needed . Scenarios where you need to know 

## BFL_StrengthCollisionQueries.h
