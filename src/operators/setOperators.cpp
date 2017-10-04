#include "operators/setOperators.h"

SetView getSetView(OpSetIntersect& op) {
    return SetView(op.memberHashes, op.cachedHashTotal, op.triggers);
}
