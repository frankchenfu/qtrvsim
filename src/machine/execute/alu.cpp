#include "alu.h"

#include "common/polyfills/mulh64.h"

namespace machine {

// RegisterValue alu_combined_operate(
RegisterValueUnion alu_combined_operate(
    AluCombinedOp op,
    AluComponent component,
    bool w_operation,
    bool modified,
    // RegisterValue a,
    // RegisterValue b) {
    RegisterValueUnion a,
    RegisterValueUnion b,
    uint8_t vl) {
    switch (component) {
    case AluComponent::ALU:
        return RegisterValue((w_operation) ? alu32_operate(op.alu_op, modified, a, b)
                             : alu64_operate(op.alu_op, modified, a, b));
    case AluComponent::MUL:
        return RegisterValue((w_operation) ? mul32_operate(op.mul_op, a, b)
                             : mul64_operate(op.mul_op, a, b));
    case AluComponent::VEC:
        return vec32_operate(op.vec_op, a, b, vl);
    case AluComponent::PASS:
        return a;
    default: qDebug("ERROR, unknown alu component: %hhx", uint8_t(component)); return 0;
    }
}

/**
 * Shift operations are limited to shift by 31 bits.
 * Other bits of the operand may be used as flags and need to be masked out
 * before any ALU operation is performed.
 */
constexpr uint64_t SHIFT_MASK32 = 0b011111; // == 31
constexpr uint64_t SHIFT_MASK64 = 0b111111; // == 63

// int64_t alu64_operate(AluOp op, bool modified, RegisterValue a, RegisterValue b) {
//     uint64_t _a = a.as_u64();
//     uint64_t _b = b.as_u64();
int64_t alu64_operate(AluOp op, bool modified, RegisterValueUnion a_raw, RegisterValueUnion b_raw) {
    RegisterValue a = a_raw.i;
    RegisterValue b = b_raw.i;
    uint64_t _a = a.as_u64();
    uint64_t _b = b.as_u64();

    switch (op) {
    case AluOp::ADD: return _a + ((modified) ? -_b : _b);
    case AluOp::SLL: return _a << (_b & SHIFT_MASK64);
    case AluOp::SLT: return a.as_i64() < b.as_i64();
    case AluOp::SLTU: return _a < _b;
    case AluOp::XOR:
        return _a ^ _b;
        // Most compilers should calculate SRA correctly, but it is UB.
    case AluOp::SR:
        return (modified) ? (a.as_i64() >> (_b & SHIFT_MASK64)) : (_a >> (_b & SHIFT_MASK64));
    case AluOp::OR: return _a | _b;
    case AluOp::AND:
        return ((modified) ? ~_a : _a) & _b; // Modified: clear bits of b using mask
                                             // in a
    default: qDebug("ERROR, unknown alu operation: %hhx", uint8_t(op)); return 0;
    }
}

// int32_t alu32_operate(AluOp op, bool modified, RegisterValue a, RegisterValue b) {
//     uint32_t _a = a.as_u32();
//     uint32_t _b = b.as_u32();
int32_t alu32_operate(AluOp op, bool modified, RegisterValueUnion a_raw, RegisterValueUnion b_raw) {
    RegisterValue a = a_raw.i;
    RegisterValue b = b_raw.i;
    uint32_t _a = a.as_u32();
    uint32_t _b = b.as_u32();

    switch (op) {
    case AluOp::ADD: return _a + ((modified) ? -_b : _b);
    case AluOp::SLL: return _a << (_b & SHIFT_MASK32);
    case AluOp::SLT: return a.as_i32() < b.as_i32();
    case AluOp::SLTU: return _a < _b;
    case AluOp::XOR:
        return _a ^ _b;
        // Most compilers should calculate SRA correctly, but it is UB.
    case AluOp::SR:
        return (modified) ? (a.as_i32() >> (_b & SHIFT_MASK32)) : (_a >> (_b & SHIFT_MASK32));
    case AluOp::OR: return _a | _b;
    case AluOp::AND:
        return ((modified) ? ~_a : _a) & _b; // Modified: clear bits of b using mask in a
    default: qDebug("ERROR, unknown alu operation: %hhx", uint8_t(op)); return 0;
    }
}

// int64_t mul64_operate(MulOp op, RegisterValue a, RegisterValue b) {
int64_t mul64_operate(MulOp op, RegisterValueUnion a_raw, RegisterValueUnion b_raw) {
    RegisterValue a = a_raw.i;
    RegisterValue b = b_raw.i;

    switch (op) {
    case MulOp::MUL: return a.as_u64() * b.as_u64();
    case MulOp::MULH: return mulh64(a.as_i64(), b.as_i64());
    case MulOp::MULHSU: return mulhsu64(a.as_i64(), b.as_u64());
    case MulOp::MULHU: return mulhu64(a.as_u64(), b.as_u64());
    case MulOp::DIV:
        if (b.as_i64() == 0) {
            return -1; // Division by zero is defined.
        } else if (a.as_i64() == INT64_MIN && b.as_i64() == -1) {
            return INT64_MIN; // Overflow.
        } else {
            return a.as_i64() / b.as_i64();
        }
    case MulOp::DIVU:
        return (b.as_u64() == 0) ? UINT64_MAX // Division by zero is defined.
                                 : a.as_u64() / b.as_u64();
    case MulOp::REM:
        if (b.as_i64() == 0) {
            return a.as_i64(); // Division by zero is defined.
        } else if (a.as_i64() == INT64_MIN && b.as_i64() == -1) {
            return 0; // Overflow.
        } else {
            return a.as_i64() % b.as_i64();
        }
    case MulOp::REMU:
        return (b.as_u64() == 0) ? a.as_u64() // Division by zero reminder
                                              // is defined.
                                 : a.as_u64() % b.as_u64();
    default: qDebug("ERROR, unknown multiplication operation: %hhx", uint8_t(op)); return 0;
    }
}

// int32_t mul32_operate(MulOp op, RegisterValue a, RegisterValue b) {
int32_t mul32_operate(MulOp op, RegisterValueUnion a_raw, RegisterValueUnion b_raw) {
    RegisterValue a = a_raw.i;
    RegisterValue b = b_raw.i;

    switch (op) {
    case MulOp::MUL: return a.as_u32() * b.as_u32();
    case MulOp::MULH: return ((uint64_t)a.as_i32() * (uint64_t)b.as_i32()) >> 32;
    case MulOp::MULHSU: return ((uint64_t)a.as_i32() * (uint64_t)b.as_u32()) >> 32;
    case MulOp::MULHU: return ((uint64_t)a.as_u32() * (uint64_t)b.as_u32()) >> 32;
    case MulOp::DIV:
        if (b.as_i32() == 0) {
            return -1; // Division by zero is defined.
        } else if (a.as_i32() == INT32_MIN && b.as_i32() == -1) {
            return INT32_MIN; // Overflow.
        } else {
            return a.as_i32() / b.as_i32();
        }
    case MulOp::DIVU:
        return (b.as_u32() == 0) ? UINT32_MAX // Division by zero is defined.
                                 : a.as_u32() / b.as_u32();
    case MulOp::REM:
        if (b.as_i32() == 0) {
            return a.as_i32(); // Division by zero is defined.
        } else if (a.as_i32() == INT32_MIN && b.as_i32() == -1) {
            return 0; // Overflow.
        } else {
            return a.as_i32() % b.as_i32();
        }
    case MulOp::REMU:
        return (b.as_u32() == 0) ? a.as_u32() // Division by zero reminder
                                              // is defined.
                                 : a.as_u32() % b.as_u32();
    default: qDebug("ERROR, unknown multiplication operation: %hhx", uint8_t(op)); return 0;
    }
}

RegisterValueUnion vec32_operate(VecOp op, RegisterValueUnion a, RegisterValueUnion b, uint8_t vl) {
    switch (op) {
    case VecOp::VADDVV: {
        vector_register_storage_t result;
        for (size_t i = 0; i < vl; i++) {
            result[i] = a.v[i] + b.v[i];
        }
        printf("perform vector addition:\nInputs: [");
        for (size_t i = 0; i < vl; i++) {
            printf("%d ", a.v[i]);
        }
        printf("] + [");
        for (size_t i = 0; i < vl; i++) {
            printf("%d ", b.v[i]);
        }
        printf("] = [");
        for (size_t i = 0; i < vl; i++) {
            printf("%d ", result[i]);
        }
        printf("]\n");
        return VectorRegisterValue(result);
    }
    case VecOp::VADDVI: {
        vector_register_storage_t result;
        for (size_t i = 0; i < vl; i++) {
            result[i] = a.v[i] + b.i.as_u32();
        }
        // printf("perform vector addition:\nInputs: [");
        // for (size_t i = 0; i < vl; i++) {
        //     printf("%d ", a.v[i]);
        // }
        // printf("] + %d = [", b.i.as_u32());
        // for (size_t i = 0; i < vl; i++) {
        //     printf("%d ", result[i]);
        // }
        // printf("]\n");
        return VectorRegisterValue(result);
    }
    case VecOp::VMULVV: {
        vector_register_storage_t result;
        for (size_t i = 0; i < vl; i++) {
            result[i] = a.v[i] * b.v[i];
        }
        printf("perform vector multiplication:\nInputs: [");
        for (size_t i = 0; i < vl; i++) {
            printf("%d ", a.v[i]);
        }
        printf("] + [");
        for (size_t i = 0; i < vl; i++) {
            printf("%d ", b.v[i]);
        }
        printf("] = [");
        for (size_t i = 0; i < vl; i++) {
            printf("%d ", result[i]);
        }
        printf("]\n");
        return VectorRegisterValue(result);
    }
    case VecOp::VREDSUM: {
        uint32_t result = a.i.as_u32();
        for (size_t i = 0; i < vl; i++) {
            result += b.v[i];
        }
        return RegisterValue(result);
    }
    default: qDebug("ERROR, unknown vector operation: %hhx", uint8_t(op)); return 0;
    }
}

} // namespace machine
