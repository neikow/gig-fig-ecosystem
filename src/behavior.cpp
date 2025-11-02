#include "behavior.h"

#include <iostream>
#include <memory>
#include <ostream>
#include <random>

#include <vector>
#include "entity.h"
#include "simulation.h"
#include "species.h"
#include "utils.h"
#include "glm/glm.hpp"

int dX[] = {1, 1, 0, -1, -1, -1, 0, 1};
int dY[] = {0, 1, 1, 1, 0, -1, -1, -1};

int DIRS = 8;

class PlantBehavior final : public IBehavior {
    using IBehavior::IBehavior;

    void handleGrowth(
        const EntityData &entity,
        const SpeciesTraits &traits,
        const std::vector<GridCell> &grid,
        std::vector<SpawnRequest> &spawnRequests
    ) const {
        if (entity.age % traits.reproductionCooldown == 0) {
            for (int dir = 0; dir < DIRS; dir++) {
                const int dx = dX[dir];
                const int dy = dY[dir];

                const int newX = entity.x + dx;
                const int newY = entity.y + dy;

                if (!insideBounds(newX, newY)) continue;

                float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

                if (grid[newY * gridWidth + newX].plantIndex == -1 && r < traits.reproductionChance) {
                    SpawnRequest request;
                    request.type = EntityType::Plant;
                    request.speciesId = entity.speciesId;
                    request.x = newX;
                    request.y = newY;
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

        for (int dy = -traits.visionRange; dy <= traits.visionRange; dy++) {
            for (int dx = -traits.visionRange; dx <= traits.visionRange; dx++) {
                if (isSelf(dx, dy) || !inRange(dx, dy, traits.visionRange)) continue;

                const int newX = entity.x + dx;
                const int newY = entity.y + dy;

                if (!insideBounds(newX, newY)) continue;

                const GridCell &targetCell = grid[newY * gridWidth + newX];
                if (targetCell.plantIndex != -1) {
                    const float distance = dx * dx + dy + dy;
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
            const int rnd = rand();
            bestDirection = glm::ivec2((rnd % 3) - 1, (rnd / 3 % 3) - 1);
            isRandomMove = true;
        }

        const int newX = entity.x + bestDirection.x;
        const int newY = entity.y + bestDirection.y;

        if (!insideBounds(newX, newY)) {
        } else {
            MoveRequest req;
            req.sourceIdx = cell.animalIndex;
            req.fromX = entity.x;
            req.fromY = entity.y;
            req.toX = newX;
            req.toY = newY;
            req.priority = isRandomMove ? MovePriority::RANDOM_MOVE : MovePriority::SEEK_FOOD;
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

        entity.energy = std::ranges::clamp(entity.energy + entities[cell.plantIndex].energy, 0.0f, traits.maxEnergy);
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

        const int randOffset = rand() % DIRS;
        int i = 0;

        const int reproductionR = rand() / static_cast<float>(RAND_MAX);

        if (reproductionR > traits.reproductionChance) return false;

        for (; i < DIRS; i++) {
            const int dir = (i + randOffset) % DIRS;
            const int dx = dX[dir];
            const int dy = dY[dir];

            const int newX = entity.x + dx;
            const int newY = entity.y + dy;

            if (!insideBounds(newX, newY)) continue;

            const int animalIndex = grid[newY * gridWidth + newX].animalIndex;
            if (animalIndex == -1) continue;
            EntityData neighboringAnimal = entities[animalIndex];
            if (neighboringAnimal.speciesId != entity.speciesId) continue;
            if (neighboringAnimal.reproductionCooldown > 0) continue;

            const int randomOffset = rand() % DIRS;
            for (int j = 0; j < DIRS; j++) {
                const int birthDir = (j + randomOffset) % DIRS;
                const int birthDX = dX[birthDir];
                const int birthDY = dY[birthDir];

                const int birthX = entity.x + birthDX;
                const int birthY = entity.y + birthDY;

                if (!insideBounds(birthX, birthY)) continue;

                if (grid[birthY * gridWidth + birthX].animalIndex == -1) {
                    SpawnRequest request;
                    request.type = EntityType::Herbivore;
                    request.speciesId = entity.speciesId;
                    request.x = birthX;
                    request.y = birthY;
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

                        MoveRequest req;
                        req.sourceIdx = cell.animalIndex;
                        req.fromX = entity.x;
                        req.fromY = entity.y;
                        req.toX = fleeX;
                        req.toY = fleeY;
                        req.priority = MovePriority::FLEE;
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
                    const float distance = dx * dx + dy + dy;
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
            const int rnd = rand();
            bestDirection = glm::ivec2((rnd % 3) - 1, (rnd / 3 % 3) - 1);
            isRandomMove = true;
        }

        const int newX = entity.x + bestDirection.x;
        const int newY = entity.y + bestDirection.y;

        std::cout << "New Movement Proposal: (" << newX << ", " << newY << ") from (" << entity.x << ", " << entity.y <<
                ")" << std::endl;

        MoveRequest req;
        req.sourceIdx = cell.animalIndex;
        req.fromX = entity.x;
        req.fromY = entity.y;
        req.toX = newX;
        req.toY = newY;
        req.priority = isRandomMove ? MovePriority::RANDOM_MOVE : MovePriority::SEEK_FOOD;
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
                entity.energy = std::ranges::clamp(
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
