#include "operators/boolOperators.h"

BoolView getBoolView(OpAnd& op) { return BoolView(op.violation, op.triggers); }
