#include <unordered_map>
#include <vector>
#include <iostream>
#include <random>
#include <algorithm>

#include "simulation.h"

#include "species.h"
#include "behavior.h"
#include "glm/vec2.hpp"

static std::mt19937 rng(std::random_device{}());

class EcosystemSimulation final : public ISimulation {
    std::vector<GridCell> grid;
    std::vector<EntityData> entities;
    std::vector<SpawnRequest> spawnRequests;
    std::vector<MoveRequest> moveRequests;

    std::unordered_map<int, SpeciesTraits> speciesTraits;
    std::unordered_map<EntityType, std::unique_ptr<IBehavior> > behaviors;

public:
    EcosystemSimulation(const int gridWidth, const int gridHeight) : ISimulation(gridWidth, gridHeight) {
        grid.resize(gridWidth * gridHeight);
        for (auto &[plantIndex, animalIndex]: grid) {
            animalIndex = -1;
            plantIndex = -1;
        }
    }

    void registerBehavior(const EntityType type, std::unique_ptr<IBehavior> behavior) {
        behaviors[type] = std::move(behavior);
    }

    void registerSpecies(const int speciesId, const SpeciesTraits &traits) {
        speciesTraits[speciesId] = traits;
    }

    std::unordered_map<int, int> getEntityCounts() override {
        std::unordered_map<int, int> counts;
        for (const auto &e: entities) {
            ++counts[e.speciesId];
        }
        return counts;
    }

    void spawnEntity(
        const EntityType type,
        const int speciesId,
        const int x,
        const int y,
        const Gender gender = Gender::None
    ) {
        const EntityData entity{
            .x = x,
            .y = y,
            .type = type,
            .speciesId = speciesId,
            .energy = speciesTraits[speciesId].maxEnergy,
            .age = 0,
            .reproductionCooldown = speciesTraits[speciesId].reproductionCooldown,
            .gender = gender,
        };

        entities.push_back(entity);

        if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) {
            std::cerr << "Warning: spawning entity out of bounds (" << x << ", " << y << ")" << std::endl;
            return;
        }

        const int cellIdx = y * gridWidth + x;

        if (type == EntityType::Plant) {
            if (grid[cellIdx].plantIndex != -1) {
                return;
            }
            grid[cellIdx].plantIndex = entities.size() - 1;
        } else {
            if (grid[cellIdx].animalIndex != -1) {
                return;
            }
            grid[cellIdx].animalIndex = entities.size() - 1;
        }
    }

    void step() override {
        executeBehaviors();
        resolveMoveRequests();
        processSpawnRequests();
        processEntityLifecycle();

        iteration++;
    }

    std::pair<std::vector<GridCell>, std::vector<EntityData> > getGridData() override {
        return {grid, entities};
    }

private:
    void executeBehaviors() {
        spawnRequests.clear();

        for (auto &entity: entities) {
            const int cellIdx = entity.y * gridWidth + entity.x;
            GridCell &cell = grid[cellIdx];

            if (const auto behaviorIt = behaviors.find(entity.type); behaviorIt != behaviors.end()) {
                behaviorIt->second->execute(
                    entity,
                    speciesTraits[entity.speciesId],
                    cell,
                    grid,
                    entities,
                    spawnRequests,
                    moveRequests
                );
            }
        }
    }

    void resolveMoveRequests() {
        if (moveRequests.empty()) return;

        std::shuffle(moveRequests.begin(), moveRequests.end(), rng);

        std::stable_sort(
            moveRequests.begin(),
            moveRequests.end(),
            [](const MoveRequest &a, const MoveRequest &b) {
                return static_cast<int>(a.priority) > static_cast<int>(b.priority);
            }
        );

        const int gridSize = gridWidth * gridHeight;
        std::vector<bool> claimed(gridSize);

        for (const auto &r: moveRequests) {
            if (r.toX < 0 || r.toX >= gridWidth || r.toY < 0 || r.toY >= gridHeight) continue;
            if (r.fromX < 0 || r.fromX >= gridWidth || r.fromY < 0 || r.fromY >= gridHeight) continue;

            const int fromIdx = r.fromY * gridWidth + r.fromX;
            const int toIdx = r.toY * gridWidth + r.toX;

            if (r.sourceIdx < 0 || r.sourceIdx >= static_cast<int>(entities.size())) continue;
            if (grid[fromIdx].animalIndex != r.sourceIdx) continue;

            if (grid[toIdx].animalIndex != -1) continue;
            if (claimed[toIdx]) continue;

            grid[toIdx].animalIndex = r.sourceIdx;
            grid[fromIdx].animalIndex = -1;
            entities[r.sourceIdx].x = r.toX;
            entities[r.sourceIdx].y = r.toY;
            entities[r.sourceIdx].energy -= speciesTraits[entities[r.sourceIdx].speciesId].movementEnergyCost;
            claimed[r.sourceIdx] = true;
        }

        moveRequests.clear();
    }

    void processSpawnRequests() {
        std::uniform_int_distribution<> genderDist(1, 2);
        for (const auto &[type, speciesId, x, y]: spawnRequests) {
            Gender gender;
            if (type == EntityType::Plant || type == EntityType::None) {
                gender = Gender::None;
            } else {
                gender = static_cast<Gender>(genderDist(rng));
            }

            spawnEntity(type, speciesId, x, y, gender);
        }
        spawnRequests.clear();
    }

    void processEntityLifecycle() {
        std::vector<EntityData> survivingEntities;
        survivingEntities.reserve(entities.size());

        for (const auto &entity: entities) {
            EntityData updatedEntity = entity;

            updatedEntity.age++;
            if (updatedEntity.reproductionCooldown > 0) {
                updatedEntity.reproductionCooldown--;
            }

            const SpeciesTraits &traits = speciesTraits[updatedEntity.speciesId];
            updatedEntity.energy -= traits.hungerDamage;

            if (updatedEntity.energy > 0.1f && updatedEntity.age < traits.maxAge) {
                survivingEntities.push_back(updatedEntity);
            }
        }

        for (auto &cell: grid) {
            cell.plantIndex = -1;
            cell.animalIndex = -1;
        }

        for (size_t i = 0; i < survivingEntities.size(); ++i) {
            const auto &e = survivingEntities[i];
            const int cellIdx = e.y * gridWidth + e.x;
            if (e.type == EntityType::Plant) {
                grid[cellIdx].plantIndex = static_cast<int>(i);
            } else {
                grid[cellIdx].animalIndex = static_cast<int>(i);
            }
        }

        entities = std::move(survivingEntities);
    }
};

glm::ivec2 randomPosition(const int gridWidth, const int gridHeight) {
    std::uniform_int_distribution distX(0, gridWidth - 1);
    std::uniform_int_distribution distY(0, gridHeight - 1);
    return {distX(rng), distY(rng)};
}

Gender randomGender() {
    std::uniform_int_distribution dist(1, 2);
    return static_cast<Gender>(dist(rng));
}

std::unique_ptr<ISimulation> createEcosystemSimulation(SimulationSettings config) {
    auto sim = std::make_unique<EcosystemSimulation>(config.gridWidth, config.gridHeight);

    sim->registerBehavior(EntityType::Plant, makePlantBehavior(config.gridWidth, config.gridHeight));
    sim->registerBehavior(EntityType::Herbivore, makeHerbivoreBehavior(config.gridWidth, config.gridHeight));
    sim->registerBehavior(EntityType::Carnivore, makeCarnivoreBehavior(config.gridWidth, config.gridHeight));

    sim->registerSpecies(
        SPECIES_GRASS,
        config.grassTraits
    );

    for (int i = 0; i < config.grassCount; ++i) {
        const auto pos = randomPosition(config.gridWidth, config.gridHeight);
        sim->spawnEntity(EntityType::Plant, SPECIES_GRASS, pos.x, pos.y);
    }

    sim->registerSpecies(
        SPECIES_RABBIT,
        config.rabbitTraits
    );

    for (int i = 0; i < config.rabbitCount; ++i) {
        const auto pos = randomPosition(config.gridWidth, config.gridHeight);
        sim->spawnEntity(EntityType::Herbivore, SPECIES_RABBIT, pos.x, pos.y, randomGender());
    }

    sim->registerSpecies(
        SPECIES_WOLF,
        config.wolfTraits
    );

    for (int i = 0; i < config.wolfCount; ++i) {
        const auto pos = randomPosition(config.gridWidth, config.gridHeight);
        sim->spawnEntity(EntityType::Carnivore, SPECIES_WOLF, pos.x, pos.y, randomGender());
    }

    return sim;
}
