#include "utils.h"

bool inRange(const int dx, const int dy, const int range) {
    return (dx * dx + dy * dy) <= (range * range);
}

bool isSelf(const int dx, const int dy) {
    return dx == 0 && dy == 0;
}
