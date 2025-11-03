#include <iostream>

#include "app.h"
#include "simulation.h"
#include "glm/vec3.hpp"

int main() {
    SimulationSettings config{};
    config.gridWidth = 40;
    config.gridHeight = 40;

    config.grassCount = 300;
    config.grassTraits = SpeciesTraits{
        .maxEnergy = 40.0f,
        .reproductionCooldown = 10,
        .reproductionChance = 0.1f,
        .spontaneousReproductionChance = 0.001f,
        .hungerDamage = 0.0f,
        .maxAge = 500,
    };

    config.rabbitCount = 200;
    config.rabbitTraits = SpeciesTraits{
        .maxEnergy = 200.0f,
        .movementEnergyCost = 1.0f,
        .reproductionThreshold = 100.0f,
        .reproductionCooldown = 50,
        .reproductionChance = 0.1f,
        .reproductionEnergyCost = 100.0f,
        .visionRange = 16.0f,
        .fleeingRange = 4.0f,
        .hungerDamage = 1.0f,
        .feedingThreshold = 180.0f,
        .maxAge = 500,
    };

    config.wolfCount = 20;
    config.wolfTraits = SpeciesTraits{
        .maxEnergy = 400.0f,
        .movementEnergyCost = 2.0f,
        .reproductionThreshold = 150.0f,
        .reproductionCooldown = 120,
        .reproductionChance = 0.01f,
        .reproductionEnergyCost = 150.0f,
        .visionRange = 12.0f,
        .hungerDamage = 0.5f,
        .feedingThreshold = 250.0f,
        .maxAge = 700,
    };

    App app(
        config,
        {
            {SPECIES_GRASS, {0, 100, 0}},
            {SPECIES_RABBIT, {200, 200, 200}},
            {SPECIES_WOLF, {100, 100, 100}},
        });

    app.run();

    return 0;
}
