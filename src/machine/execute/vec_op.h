#ifndef VEC_OP_H
#define VEC_OP_H

#include <QMetaType>

namespace machine {

enum class VecOp : uint8_t {
    VADDVV = 0b000,
	VADDVI = 0b001,
	VMULVV = 0b010,
	VREDSUM = 0b011,
};

}

Q_DECLARE_METATYPE(machine::VecOp)

#endif // VEC_OP_H
