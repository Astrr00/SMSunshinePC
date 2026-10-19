#include <Enemy/Graph.hpp>
#include <Map/Map.hpp>
#include <cstdio>
#include <cstdlib>

// A non-spline graph must never enter the spline-only magnitude dependency.
extern "C" f32 VECMag(Vec*) { std::abort(); }

// Unused graph operations fail loudly if this fixture accidentally invokes them.
JGeometry::TVec3<f32>* gpMarioPos = nullptr;
TMap* gpMap = nullptr;
JGeometry::TVec3<f32> MsGetRotFromZaxis(const JGeometry::TVec3<f32>&) { std::abort(); }
void MsVECNormalize(Vec*, Vec*) { std::abort(); }
f32 MsVECMag2(Vec*) { std::abort(); }
s16 matan(f32, f32) { std::abort(); }
BOOL MsIsInSight(const JGeometry::TVec3<f32>&, f32, const JGeometry::TVec3<f32>&, f32, f32, f32) { std::abort(); }
f32 TMap::checkGround(const JGeometry::TVec3<f32>&, const TBGCheckData**) const { std::abort(); }

namespace {
int checks = 0;
int failures = 0;
void check(bool ok, const char* name) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", name); }
}
}

int main() {
    TRailNode rails[3] = {};
    rails[0].mPosition.x = 0;
    rails[1].mPosition.x = 100;
    rails[2].mPosition.x = 200;
    TGraphWeb graph(rails, "replay-test", 3);
    const JGeometry::TVec3<f32> pos(190.0f, 0.0f, 0.0f);
    const int whole = graph.findNearestNodeIndex(pos, 0xffffffff);
    const int replay = graph.findNearestNodeIndex(pos, 0xffffffff, 2);
    check(whole == 2 && whole >= 2, "whole-graph search reproduces a node outside the two replay rows");
    check(replay == 1, "normal replay search chooses the nearest authored replay row");
    check(graph.getNodeNum() == 3 && graph.findNearestNodeIndex(pos, 0xffffffff) == 2,
          "bounded lookup preserves the extra gate-event node for other callers");
    check(graph.findNearestNodeIndex(pos, 0xffffffff, 0) == -1, "empty range has no candidate");
    check(graph.findNearestNodeIndex(pos, 0xffffffff, 99) == 2, "range cannot exceed the graph");
    rails[1].mFlags = 4;
    check(graph.findNearestNodeIndex(pos, 4, 2) == 0, "range retains original excluded-flag semantics");
    delete[] graph.unk0; // The original empty graph destructor does not own this fixture's allocation.
    std::printf("%d replay-node checks, %d failed\n", checks, failures);
    return failures;
}
