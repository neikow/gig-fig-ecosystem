#include "behavior.h"

#include <iostream>
#include <memory>
#include <ostream>
#include <random>
#include <algorithm>

#include <vector>
#include "entity.h"
#include "simulation.h"
#include "species.h"
#include "utils.h"
#include "glm/glm.hpp"

int dX[] = {1, 1, 0, -1, -1, -1, 0, 1};
int dY[] = {0, 1, 1, 1, 0, -1, -1, -1};

int DIRS = 8;

static int randomDirOffset() {
    return std::uniform_int_distribution(0, DIRS - 1)(getRng());
}

static float probability() {
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(getRng());
}

static int randomDirection() {
    return std::uniform_int_distribution<int>(-1, 1)(getRng());
}

static int randomRangeInt(const int minVal, const int maxVal) {
    return std::uniform_int_distribution(minVal, maxVal)(getRng());
}

class PlantBehavior final : public IBehavior {
    using IBehavior::IBehavior;

    void handleGrowth(
        const EntityData &entity,
        const SpeciesTraits &traits,
        const std::vector<GridCell> &grid,
        std::vector<SpawnRequest> &spawnRequests
    ) const {
        if (probability() < traits.spontaneousReproductionChance) {
            const SpawnRequest request{
                .type = EntityType::Plant,
                .speciesId = entity.speciesId,
                .x = randomRangeInt(0, gridWidth - 1),
                .y = randomRangeInt(0, gridHeight - 1),
            };
            spawnRequests.push_back(request);
        }

        if (entity.age % traits.reproductionCooldown == 0) {
            for (int dir = 0; dir < DIRS; dir++) {
                const int dx = dX[dir];
                const int dy = dY[dir];

                const int newX = entity.x + dx;
                const int newY = entity.y + dy;

                if (!insideBounds(newX, newY)) continue;

                const float p = probability();

                if (grid[newY * gridWidth + newX].plantIndex == -1 && p < traits.reproductionChance) {
                    SpawnRequest request{
                        .type = EntityType::Plant,
                        .speciesId = entity.speciesId,
                        .x = newX,
                        .y = newY,
                    };
                    spawnRequests.push_back(request);
                }
            }
        }
    }

    const void execute(
        EntityData &entity,
        const SpeciesTraits &traits,
        GridCell &cell,
        std::vector<GridCell> &grid,
        std::vector<EntityData> &entities,
        std::vector<SpawnRequest> &spawnRequests,
        std::vector<MoveRequest> &moveRequests
    ) override {
        handleGrowth(entity, traits, grid, spawnRequests);
    }
};

std::unique_ptr<IBehavior> makePlantBehavior(int gridWidth, int gridHeight) {
    return std::make_unique<PlantBehavior>(gridWidth, gridHeight);
}

class HerbivoreBehavior final : public IBehavior {
    using IBehavior::IBehavior;

    bool proposeMovement(
        const EntityData &entity,
        const SpeciesTraits &traits,
        const GridCell &cell,
        const std::vector<GridCell> &grid,
        std::vector<MoveRequest> &moveRequests
    ) const {
        if (entity.energy <= traits.movementEnergyCost) return false;

        glm::ivec2 bestDirection(0, 0);
        float bestScore = -1.0f;

        if (entity.energy < traits.feedingThreshold) {
            for (int dy = -traits.visionRange; dy <= traits.visionRange; dy++) {
                for (int dx = -traits.visionRange; dx <= traits.visionRange; dx++) {
                    if (isSelf(dx, dy) || !inRange(dx, dy, traits.visionRange)) continue;

                    const int newX = entity.x + dx;
                    const int newY = entity.y + dy;

                    if (!insideBounds(newX, newY)) continue;

                    const GridCell &targetCell = grid[newY * gridWidth + newX];
                    if (targetCell.plantIndex != -1) {
                        const float distance = dx * dx + dy * dy;
                        const float score = 1.0f / (distance + 0.1f);

                        if (score > bestScore) {
                            bestScore = score;
                            bestDirection = glm::ivec2((dx > 0) - (dx < 0), (dy > 0) - (dy < 0));
                        }
                    }
                }
            }
        }

        bool isRandomMove = false;

        if (bestScore < 0.0f) {
            bestDirection = glm::ivec2(randomDirection(), randomDirection());
            isRandomMove = true;
        }

        const int newX = entity.x + bestDirection.x;
        const int newY = entity.y + bestDirection.y;

        if (!insideBounds(newX, newY)) {
        } else {
            const MoveRequest req{
                .sourceIdx = cell.animalIndex,
                .fromX = entity.x,
                .fromY = entity.y,
                .toX = newX,
                .toY = newY,
                .priority = isRandomMove ? MovePriority::RANDOM_MOVE : MovePriority::SEEK_FOOD,
            };
            moveRequests.push_back(req);
            return true;
        }
        return false;
    }

    static bool handleEating(
        EntityData &entity,
        const SpeciesTraits &traits,
        const GridCell &cell,
        std::vector<EntityData> &entities
    ) {
        if (cell.plantIndex < 0) return false;
        if (entity.energy <= 0.0f || entity.energy >= traits.feedingThreshold) return false;
        if (entities[cell.plantIndex].energy <= 0.0f) return false;

        entity.energy = std::clamp(entity.energy + entities[cell.plantIndex].energy, 0.0f, traits.maxEnergy);
        entities[cell.plantIndex].energy = 0.0f;

        return true;
    }

    bool handleReproduction(
        EntityData &entity,
        const SpeciesTraits &traits,
        const std::vector<GridCell> &grid,
        const std::vector<EntityData> &entities,
        std::vector<SpawnRequest> &spawnRequests
    ) const {
        if (entity.reproductionCooldown > 0) return false;
        if (entity.energy <= traits.reproductionThreshold) return false;

        const int offset = randomDirOffset();

        const float reproductionP = probability();

        if (reproductionP > traits.reproductionChance) return false;

        for (int i = 0; i < DIRS; i++) {
            const int dir = (i + offset) % DIRS;
            const int dx = dX[dir];
            const int dy = dY[dir];

            const int newX = entity.x + dx;
            const int newY = entity.y + dy;

            if (!insideBounds(newX, newY)) continue;

            const int animalIndex = grid[newY * gridWidth + newX].animalIndex;
            if (animalIndex == -1) continue;
            EntityData neighboringAnimal = entities[animalIndex];
            if (neighboringAnimal.speciesId != entity.speciesId) continue;
            if (neighboringAnimal.gender == entity.gender) continue;
            if (neighboringAnimal.reproductionCooldown > 0) continue;

            const int randomOffset = randomDirOffset();
            for (int j = 0; j < DIRS; j++) {
                const int birthDir = (j + randomOffset) % DIRS;
                const int birthDX = dX[birthDir];
                const int birthDY = dY[birthDir];

                const int birthX = entity.x + birthDX;
                const int birthY = entity.y + birthDY;

                if (!insideBounds(birthX, birthY)) continue;

                if (grid[birthY * gridWidth + birthX].animalIndex == -1) {
                    const SpawnRequest request{
                        .type = EntityType::Herbivore,
                        .speciesId = entity.speciesId,
                        .x = birthX,
                        .y = birthY
                    };
                    spawnRequests.push_back(request);


                    entity.reproductionCooldown = traits.reproductionCooldown;
                    entity.energy -= traits.reproductionEnergyCost;
                    neighboringAnimal.reproductionCooldown = traits.reproductionCooldown;
                    neighboringAnimal.energy -= traits.reproductionEnergyCost;

                    return true;
                }
            }
        }

        return false;
    }

    bool proposeFleeing(
        const EntityData &entity,
        const SpeciesTraits &traits,
        const GridCell &cell,
        const std::vector<GridCell> &grid,
        const std::vector<EntityData> &entities,
        std::vector<MoveRequest> &moveRequests
    ) const {
        // Passing down genes is more important than fleeing
        if (entity.energy <= traits.reproductionThreshold) return false;

        for (int dy = -traits.fleeingRange; dy <= traits.fleeingRange; dy++) {
            for (int dx = -traits.fleeingRange; dx <= traits.fleeingRange; dx++) {
                if (isSelf(dx, dy) || !inRange(dx, dy, traits.fleeingRange)) continue;

                const int newX = entity.x + dx;
                const int newY = entity.y + dy;

                if (!insideBounds(newX, newY)) continue;

                const GridCell &targetCell = grid[newY * gridWidth + newX];
                if (targetCell.animalIndex != -1 && entities[targetCell.animalIndex].type == EntityType::Carnivore) {
                    const glm::ivec2 fleeDirection = glm::ivec2((dx > 0) - (dx < 0), (dy > 0) - (dy < 0)) * -1;

                    const int fleeX = entity.x + fleeDirection.x;
                    const int fleeY = entity.y + fleeDirection.y;

                    if (!insideBounds(fleeX, fleeY)) {
                    } else {
                        if (grid[fleeY * gridWidth + fleeX].animalIndex != -1) return false;

                        const MoveRequest req{
                            .sourceIdx = cell.animalIndex,
                            .fromX = entity.x,
                            .fromY = entity.y,
                            .toX = fleeX,
                            .toY = fleeY,
                            .priority = MovePriority::FLEE,
                        };
                        moveRequests.push_back(req);
                    }
                    return true;
                }
            }
        }
        return false;
    }

public:
    const void execute(
        EntityData &entity,
        const SpeciesTraits &traits,
        GridCell &cell,
        std::vector<GridCell> &grid,
        std::vector<EntityData> &entities,
        std::vector<SpawnRequest> &spawnRequests,
        std::vector<MoveRequest> &moveRequests
    ) override {
        // an animal can either reproduce, eat or move in a single step
        if (
            proposeFleeing(
                entity,
                traits,
                cell,
                grid,
                entities,
                moveRequests
            )
        ) {
            return;
        }

        if (
            handleReproduction(
                entity,
                traits,
                grid,
                entities,
                spawnRequests
            )
        ) {
            return;
        }

        if (
            handleEating(
                entity,
                traits,
                cell,
                entities
            )
        ) {
            return;
        }

        proposeMovement(
            entity,
            traits,
            cell,
            grid,
            moveRequests
        );
    }
};

std::unique_ptr<IBehavior> makeHerbivoreBehavior(int gridWidth, int gridHeight) {
    return std::make_unique<HerbivoreBehavior>(gridWidth, gridHeight);
}

class CarnivoreBehavior final : public IBehavior {
    using IBehavior::IBehavior;

    bool proposeMovement(
        const EntityData &entity,
        const SpeciesTraits &traits,
        const GridCell &cell,
        const std::vector<GridCell> &grid,
        const std::vector<EntityData> &entities,
        std::vector<MoveRequest> &moveRequests
    ) const {
        if (entity.energy <= traits.movementEnergyCost) return false;

        glm::ivec2 bestDirection(0, 0);
        float bestScore = -1.0f;

        for (int dy = -traits.visionRange; dy <= traits.visionRange; dy++) {
            for (int dx = -traits.visionRange; dx <= traits.visionRange; dx++) {
                if (isSelf(dx, dy) || !inRange(dx, dy, traits.visionRange)) continue;

                const int newX = entity.x + dx;
                const int newY = entity.y + dy;

                if (!insideBounds(newX, newY)) continue;

                const GridCell &targetCell = grid[newY * gridWidth + newX];
                if (targetCell.animalIndex != -1 && entities[targetCell.animalIndex].speciesId != entity.speciesId) {
                    const float distance = dx * dx + dy * dy;
                    const float score = 1.0f / (distance + 0.1f);

                    if (score > bestScore) {
                        bestScore = score;
                        bestDirection = glm::ivec2((dx > 0) - (dx < 0), (dy > 0) - (dy < 0));
                    }
                }
            }
        }

        bool isRandomMove = false;

        if (bestScore < 0.0f) {
            bestDirection = glm::ivec2(randomDirection(), randomDirection());
            isRandomMove = true;
        }

        const int newX = entity.x + bestDirection.x;
        const int newY = entity.y + bestDirection.y;

        const MoveRequest req{
            .sourceIdx = cell.animalIndex,
            .fromX = entity.x,
            .fromY = entity.y,
            .toX = newX,
            .toY = newY,
            .priority = isRandomMove ? MovePriority::RANDOM_MOVE : MovePriority::SEEK_FOOD,
        };
        moveRequests.push_back(req);
        return true;
    }

    bool handleEating(
        EntityData &entity,
        const SpeciesTraits &traits,
        GridCell &cell,
        const std::vector<GridCell> &grid,
        std::vector<EntityData> &entities
    ) const {
        if (entity.energy >= traits.feedingThreshold) return false;

        const int randomOffset = rand() % DIRS;
        for (int i = 0; i < DIRS; i++) {
            const int dir = (i + randomOffset) % DIRS;
            const int dx = dX[dir];
            const int dy = dY[dir];

            const int newX = entity.x + dx;
            const int newY = entity.y + dy;

            if (!insideBounds(newX, newY)) continue;

            const GridCell &targetCell = grid[newY * gridWidth + newX];
            if (
                targetCell.animalIndex > -1
                // For the time being, allow to eat any other animal species;
                && entities[targetCell.animalIndex].speciesId != entity.speciesId
            ) {
                entity.energy = std::clamp(
                    entity.energy + entities[targetCell.animalIndex].energy,
                    0.0f,
                    traits.maxEnergy
                );
                entities[targetCell.animalIndex].energy = 0.0f;
                return true;
            }
        }

        return false;
    }

    bool handleReproduction(
        EntityData &entity,
        const SpeciesTraits &traits,
        const std::vector<GridCell> &grid,
        const std::vector<EntityData> &entities,
        std::vector<SpawnRequest> &spawnRequests
    ) const {
        if (entity.reproductionCooldown > 0) return false;
        if (entity.energy <= traits.reproductionThreshold) return false;

        const int offset = randomDirOffset();

        const float reproductionP = probability();

        if (reproductionP > traits.reproductionChance) return false;

        for (int i = 0; i < DIRS; i++) {
            const int dir = (i + offset) % DIRS;
            const int dx = dX[dir];
            const int dy = dY[dir];

            const int newX = entity.x + dx;
            const int newY = entity.y + dy;

            if (!insideBounds(newX, newY)) continue;

            const int animalIndex = grid[newY * gridWidth + newX].animalIndex;
            if (animalIndex == -1) continue;
            EntityData neighboringAnimal = entities[animalIndex];
            if (neighboringAnimal.speciesId != entity.speciesId) continue;
            if (neighboringAnimal.gender == entity.gender) continue;
            if (neighboringAnimal.reproductionCooldown > 0) continue;

            const int randomOffset = randomDirOffset();
            for (int j = 0; j < DIRS; j++) {
                const int birthDir = (j + randomOffset) % DIRS;
                const int birthDX = dX[birthDir];
                const int birthDY = dY[birthDir];

                const int birthX = entity.x + birthDX;
                const int birthY = entity.y + birthDY;

                if (!insideBounds(birthX, birthY)) continue;

                if (grid[birthY * gridWidth + birthX].animalIndex == -1) {
                    const SpawnRequest request{
                        .type = EntityType::Carnivore,
                        .speciesId = entity.speciesId,
                        .x = birthX,
                        .y = birthY
                    };
                    spawnRequests.push_back(request);


                    entity.reproductionCooldown = traits.reproductionCooldown;
                    entity.energy -= traits.reproductionEnergyCost;
                    neighboringAnimal.reproductionCooldown = traits.reproductionCooldown;
                    neighboringAnimal.energy -= traits.reproductionEnergyCost;

                    return true;
                }
            }
        }

        return false;
    }

    const void execute(
        EntityData &entity,
        const SpeciesTraits &traits,
        GridCell &cell,
        std::vector<GridCell> &grid,
        std::vector<EntityData> &entities,
        std::vector<SpawnRequest> &spawnRequests,
        std::vector<MoveRequest> &moveRequests
    ) override {
        if (
            handleReproduction(
                entity,
                traits,
                grid,
                entities,
                spawnRequests
            )
        ) {
            return;
        }

        if (
            handleEating(
                entity,
                traits,
                cell,
                grid,
                entities
            )
        ) {
            return;
        }

        proposeMovement(
            entity,
            traits,
            cell,
            grid,
            entities,
            moveRequests
        );
    }
};

std::unique_ptr<IBehavior> makeCarnivoreBehavior(int gridWidth, int gridHeight) {
    return std::make_unique<CarnivoreBehavior>(gridWidth, gridHeight);
}
