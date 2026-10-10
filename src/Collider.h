// Collider.h -- what the driven rover can bump into.
//
// Every obstacle is a circle on the ground plane (x, z, radius). A circle is a good enough shape for a tree
// trunk, a rock or a post, and a few circles in a row cover a hut. The rover itself is a circle too, so a
// collision test is one distance comparison. There is no physics engine: Controller.h resolves the overlap and
// turns the impact speed into a bounce.
#pragma once

enum ColliderKind {
    KindBurntTree = 0,
    KindForestTree,
    KindHut,
    KindWindmill,
    KindRock,
    KindHelicopter,
    KindVolcano,
    KindLake,
    KindWorldEdge,
    KindCount
};

struct Collider {
    float x = 0.0f, z = 0.0f, r = 1.0f;
    int kind = KindRock;
};

inline const char* colliderName(int kind) {
    static const char* kNames[KindCount] = {
        "BURNT TREE", "TREE", "HUT", "WINDMILL", "ROCK", "HELICOPTER", "VOLCANO", "LAKE", "WORLD EDGE"
    };
    return (kind >= 0 && kind < KindCount) ? kNames[kind] : "OBSTACLE";
}
