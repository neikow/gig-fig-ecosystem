#ifndef ECOSYSTEM_ENTITY_H
#define ECOSYSTEM_ENTITY_H

enum class EntityType : int {
    None = 0,
    Plant = 1,
    Herbivore = 2,
    Carnivore = 3,
};

struct EntityData {
    int x, y;
    EntityType type;
    int speciesId;
    float energy;
    int age;
    int reproductionCooldown;
};

#endif //ECOSYSTEM_ENTITY_H
