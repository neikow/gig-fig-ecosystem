#ifndef ECOSYSTEM_BEHAVIOR_H
#define ECOSYSTEM_BEHAVIOR_H
#include <random>
#include <vector>
#include <chrono>

#include "entity.h"
#include "species.h"
#include "simulation.h"

static inline std::mt19937 &getRng() {
    static thread_local std::mt19937 g(
        static_cast<unsigned>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );
    return g;
}

class IBehavior {
protected:
    int gridWidth, gridHeight;

public:
    virtual ~IBehavior() = default;

    IBehavior(const int gridWidth, const int gridHeight) : gridWidth(gridWidth), gridHeight(gridHeight) {
    }

    virtual const void execute(
        EntityData &entity,
        const SpeciesTraits &traits,
        GridCell &cell,
        std::vector<GridCell> &grid,
        std::vector<EntityData> &entities,
        std::vector<SpawnRequest> &spawnRequests,
        std::vector<MoveRequest> &moveRequests
    ) = 0;

    [[nodiscard]] bool insideBounds(const int x, const int y) const {
        return x >= 0 && y >= 0 && x < gridWidth && y < gridHeight;
    }
};

std::unique_ptr<IBehavior> makePlantBehavior(int gridWidth, int gridHeight);

std::unique_ptr<IBehavior> makeHerbivoreBehavior(int gridWidth, int gridHeight);

std::unique_ptr<IBehavior> makeCarnivoreBehavior(int gridWidth, int gridHeight);

#endif //ECOSYSTEM_BEHAVIOR_H
