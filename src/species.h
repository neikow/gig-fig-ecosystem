//
// Created by Vitaly Lysen on 01/11/2025.
//

#ifndef ECOSYSTEM_SPECIES_H
#define ECOSYSTEM_SPECIES_H

struct SpeciesTraits {
    float maxEnergy;

    float movementEnergyCost;

    float reproductionThreshold;
    int reproductionCooldown;
    float reproductionChance;
    float reproductionEnergyCost;

    float spontaneousReproductionChance;

    float visionRange;
    float fleeingRange;

    float hungerDamage;
    float feedingThreshold;

    int maxAge;
};

#endif //ECOSYSTEM_SPECIES_H
