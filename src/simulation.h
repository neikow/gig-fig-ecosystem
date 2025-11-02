#ifndef ECOSYSTEM_SIMULATION_H
#define ECOSYSTEM_SIMULATION_H
#include "entity.h"
#include "species.h"

constexpr int SPECIES_GRASS = 0;
constexpr int SPECIES_RABBIT = 1;
constexpr int SPECIES_WOLF = 2;

struct GridCell {
    int plantIndex = -1;
    int animalIndex = -1;
};

struct SpawnRequest {
    EntityType type;
    int speciesId;
    int x, y;
};

enum class MovePriority {
    FLEE = 100,
    SEEK_FOOD = 1,
    RANDOM_MOVE = 0,
};

struct MoveRequest {
    int sourceIdx;
    int fromX, fromY;
    int toX, toY;
    MovePriority priority;
};

struct SimulationSettings {
    int gridWidth, gridHeight;

    int grassCount;
    SpeciesTraits grassTraits;
    int rabbitCount;
    SpeciesTraits rabbitTraits;
    int wolfCount;
    SpeciesTraits wolfTraits;
};

class ISimulation {
protected:
    int gridWidth, gridHeight;

public:
    virtual ~ISimulation() = default;

    ISimulation(const int gridWidth, const int gridHeight) : gridWidth(gridWidth), gridHeight(gridHeight) {
    }

    int iteration = 0;

    virtual void step() = 0;

    virtual std::pair<std::vector<GridCell>, std::vector<EntityData> > getGridData() = 0;

    virtual std::unordered_map<int, int> getEntityCounts() = 0;
};

std::unique_ptr<ISimulation> createEcosystemSimulation(SimulationSettings config);

#endif //ECOSYSTEM_SIMULATION_H
