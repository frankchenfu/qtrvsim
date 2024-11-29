#include "memory/frontend_memory.h"

#include "common/endian.h"

namespace machine {

bool FrontendMemory::write_u8(
    Address address,
    uint8_t value,
    AccessEffects type) {
    return write_generic<typeof(value)>(address, value, type);
}

bool FrontendMemory::write_u16(
    Address address,
    uint16_t value,
    AccessEffects type) {
    return write_generic<typeof(value)>(address, value, type);
}

bool FrontendMemory::write_u32(
    Address address,
    uint32_t value,
    AccessEffects type) {
    return write_generic<typeof(value)>(address, value, type);
}

bool FrontendMemory::write_u64(
    Address address,
    uint64_t value,
    AccessEffects type) {
    return write_generic<typeof(value)>(address, value, type);
}

uint8_t FrontendMemory::read_u8(Address address, AccessEffects type) const {
    return read_generic<uint8_t>(address, type);
}

uint16_t FrontendMemory::read_u16(Address address, AccessEffects type) const {
    return read_generic<uint16_t>(address, type);
}

uint32_t FrontendMemory::read_u32(Address address, AccessEffects type) const {
    return read_generic<uint32_t>(address, type);
}

uint64_t FrontendMemory::read_u64(Address address, AccessEffects type) const {
    return read_generic<uint64_t>(address, type);
}

bool FrontendMemory::write_vec_u32(
    Address address,
    vector_register_storage_t value,
    uint8_t vl,
    AccessEffects type) {
    bool flag = true;
    for (size_t i = 0; i < vl; i++) {
        flag &= write_generic<uint32_t>(address + i * sizeof(uint32_t), value[i], type);
    }
    // printf("write_vec_u32: ");
    // for (size_t i = 0; i < vl; i++) {
    //     printf("%d ", value[i]);
    // }
    // printf("\n");
    return flag;
}

vector_register_storage_t FrontendMemory::read_vec_u32(
    Address address,
    uint8_t vl,
    AccessEffects type) const {
    vector_register_storage_t value;
    for (size_t i = 0; i < vl; i++) {
        value[i] = read_generic<uint32_t>(address + i * sizeof(uint32_t), type);
    }
    // printf("read_vec_u32: ");
    // for (size_t i = 0; i < vl; i++) {
    //     printf("%d ", value[i]);
    // }
    // printf("\n");
    return value;
}

void FrontendMemory::write_ctl(
    enum AccessControl ctl,
    Address offset,
    // RegisterValue value) {
    RegisterValueUnion value, uint8_t vl) {
    switch (ctl) {
    case AC_NONE: {
        break;
    }
    case AC_I8:
    case AC_U8: {
        write_u8(offset, value.i.as_u8());
        break;
    }
    case AC_I16:
    case AC_U16: {
        write_u16(offset, value.i.as_u16());
        break;
    }
    case AC_I32:
    case AC_U32: {
        write_u32(offset, value.i.as_u32());
        break;
    }
    case AC_I64:
    case AC_U64: {
        write_u64(offset, value.i.as_u64());
        break;
    }
    case AC_V32: {
        // printf("found AC_V32: ");
        // vector_register_storage_t vec = value.v.as_vec();
        // for (size_t i = 0; i < vl; i++) {
        //     printf("%d ", vec[i]);
        // }
        // printf("\n");
        write_vec_u32(offset, value.v.as_vec(), vl);
        break;
    }
    default: {
        throw SIMULATOR_EXCEPTION(
            UnknownMemoryControl, "Trying to write to memory with unknown ctl",
            QString::number(ctl));
    }
    }
}

RegisterValueUnion
FrontendMemory::read_ctl(enum AccessControl ctl, Address address, uint8_t vl) const {
    switch (ctl) {
    case AC_NONE: return RegisterValue(0);
    case AC_I8: return RegisterValue((int8_t)read_u8(address));
    case AC_U8: return RegisterValue(read_u8(address));
    case AC_I16: return RegisterValue((int16_t)read_u16(address));
    case AC_U16: return RegisterValue(read_u16(address));
    case AC_I32: return RegisterValue((int32_t)read_u32(address));
    case AC_U32: return RegisterValue(read_u32(address));
    case AC_I64: return RegisterValue((int64_t)read_u64(address));
    case AC_U64: return RegisterValue(read_u64(address));
    case AC_V32: return VectorRegisterValue(read_vec_u32(address, vl));
    default: {
        throw SIMULATOR_EXCEPTION(
            UnknownMemoryControl, "Trying to read from memory with unknown ctl",
            QString::number(ctl));
    }
    }
}

void FrontendMemory::sync() {}

LocationStatus FrontendMemory::location_status(Address address) const {
    (void)address;
    return LOCSTAT_NONE;
}

template<typename T>
T FrontendMemory::read_generic(Address address, AccessEffects type) const {
    T value;
    read(&value, address, sizeof(T), { .type = type });
    // When cross-simulating (BIG simulator on LITTLE host machine and vice
    // versa) data needs to be swapped before writing to memory and after
    // reading from memory to achieve correct results of misaligned reads. See
    // bellow.
    //
    // Example (4 byte write and 4 byte read offseted by 2 bytes):
    //
    //  BIG on LITTLE
    //      REGISTER:           12 34 56 78
    //      PRE-SWAP:           78 56 34 12 (still in register)
    //      NATIVE ENDIAN MEM:  12 34 56 78 00 00 (native is LITTLE)
    //      READ IN MEM:              56 78 00 00
    //      REGISTER:                 00 00 78 56
    //      POST-SWAP:                56 78 00 00 (correct)
    //
    //  LITTLE on BIG
    //      REGISTER:          12 34 56 78
    //      PRE-SWAP:          78 56 34 12  (still in register)
    //      NATIVE ENDIAN MEM: 78 56 34 12 00 00 (native is BIG)
    //      READ IN MEM:             34 12 00 00
    //      REGISTER:                34 12 00 00
    //      POST-SWAP:               00 00 12 34 (correct)
    //
    return byteswap_if(value, this->simulated_machine_endian != NATIVE_ENDIAN);
}

template<typename T>
bool FrontendMemory::write_generic(
    Address address,
    const T value,
    AccessEffects type) {
    // See example in read_generic for byteswap explanation.
    const T swapped_value
        = byteswap_if(value, this->simulated_machine_endian != NATIVE_ENDIAN);
    return write(address, &swapped_value, sizeof(T), { .type = type }).changed;
}
FrontendMemory::FrontendMemory(Endian simulated_endian)
    : simulated_machine_endian(simulated_endian) {}
} // namespace machine