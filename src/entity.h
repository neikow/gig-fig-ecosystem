#ifndef ECOSYSTEM_ENTITY_H
#define ECOSYSTEM_ENTITY_H

enum class EntityType : int {
    None = 0,
    Plant = 1,
    Herbivore = 2,
    Carnivore = 3,
};

enum class Gender : int {
    None = 0,
    Female = 1,
    Male = 2,
};

struct EntityData {
    int x, y;
    EntityType type;
    int speciesId;
    float energy;
    int age;
    int reproductionCooldown;
    Gender gender;
};

#endif //ECOSYSTEM_ENTITY_H
