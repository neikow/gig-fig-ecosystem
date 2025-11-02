#include <unordered_map>
#include <vector>

#include "simulation.h"

#include <iostream>
#include <random>

#include "species.h"
#include "behavior.h"

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

    void spawnEntity(const EntityType type, const int speciesId, const int x, const int y) {
        EntityData entity;
        entity.x = x;
        entity.y = y;
        entity.type = type;
        entity.speciesId = speciesId;
        entity.energy = speciesTraits[speciesId].maxEnergy;
        entity.age = 0;
        entity.reproductionCooldown = speciesTraits[speciesId].reproductionCooldown;

        entities.push_back(entity);

        const int cellIdx = y * gridWidth + x;
        if (type == EntityType::Plant) {
            grid[cellIdx].plantIndex = entities.size() - 1;
        } else {
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

        static std::mt19937 rng(std::random_device{}());
        std::ranges::shuffle(moveRequests, rng);

        std::ranges::stable_sort(
            moveRequests,
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
        for (const auto &[type, speciesId, x, y]: spawnRequests) {
            spawnEntity(type, speciesId, x, y);
        }
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

            if (updatedEntity.energy > 0.0f && updatedEntity.age < traits.maxAge) {
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

std::unique_ptr<ISimulation> createEcosystemSimulation(const int gridWidth, const int gridHeight) {
    auto sim = std::make_unique<EcosystemSimulation>(gridWidth, gridHeight);

    sim->registerBehavior(EntityType::Plant, makePlantBehavior(gridWidth, gridHeight));
    sim->registerBehavior(EntityType::Herbivore, makeHerbivoreBehavior(gridWidth, gridHeight));
    sim->registerBehavior(EntityType::Carnivore, makeCarnivoreBehavior(gridWidth, gridHeight));

    sim->registerSpecies(
        SPECIES_GRASS,
        SpeciesTraits{
            .maxEnergy = 30.0f,
            .reproductionCooldown = 10,
            .reproductionChance = 0.1f,
            .hungerDamage = 0.0f,
            .maxAge = 500,
        });

    sim->spawnEntity(EntityType::Plant, SPECIES_GRASS, gridWidth / 2, gridHeight / 2);
    sim->spawnEntity(EntityType::Plant, SPECIES_GRASS, gridWidth / 2 + 2, gridHeight / 2 + 1);
    sim->spawnEntity(EntityType::Plant, SPECIES_GRASS, gridWidth / 2 + 4, gridHeight / 2 - 1);
    sim->spawnEntity(EntityType::Plant, SPECIES_GRASS, gridWidth / 2 + 5, gridHeight / 2 - 4);

    sim->registerSpecies(
        SPECIES_RABBIT,
        SpeciesTraits{
            .maxEnergy = 200.0f,
            .movementEnergyCost = 5.0f,
            .reproductionThreshold = 100.0f,
            .reproductionCooldown = 50,
            .reproductionChance = 0.3f,
            .reproductionEnergyCost = 100.0f,
            .visionRange = 16.0f,
            .fleeingRange = 4.0f,
            .hungerDamage = 1.0f,
            .feedingThreshold = 150.0f,
            .maxAge = 500,
        });

    sim->spawnEntity(EntityType::Herbivore, SPECIES_RABBIT, gridWidth / 2, gridHeight / 4 + 1);
    sim->spawnEntity(EntityType::Herbivore, SPECIES_RABBIT, gridWidth / 2, gridHeight / 4 + 2);
    sim->spawnEntity(EntityType::Herbivore, SPECIES_RABBIT, gridWidth / 2, gridHeight / 4 + 3);

    sim->registerSpecies(
        SPECIES_WOLF,
        SpeciesTraits{
            .maxEnergy = 300.0f,
            .movementEnergyCost = 8.0f,
            .reproductionThreshold = 150.0f,
            .reproductionCooldown = 80,
            .reproductionChance = 0.2f,
            .reproductionEnergyCost = 150.0f,
            .visionRange = 8.0f,
            .hungerDamage = 2.0f,
            .feedingThreshold = 250.0f,
            .maxAge = 700,
        });

    sim->spawnEntity(EntityType::Carnivore, SPECIES_WOLF, 2 * gridWidth / 3, 2 * gridHeight / 3);

    return sim;
}
