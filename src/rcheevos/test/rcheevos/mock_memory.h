#ifndef MOCK_MEMORY_H
#define MOCK_MEMORY_H

typedef struct {
  uint8_t* ram;
  uint32_t size;
}
memory_t;

static uint32_t peekb(uint32_t address, memory_t* memory) {
  return address < memory->size ? memory->ram[address] : 0;
}

static uint32_t peek(uint32_t address, uint32_t num_bytes, void* ud) {
  memory_t* memory = (memory_t*)ud;

  switch (num_bytes) {
    case 1: return peekb(address, memory);

    case 2: return peekb(address, memory) |
      peekb(address + 1, memory) << 8;

    case 4: return peekb(address, memory) |
      peekb(address + 1, memory) << 8 |
      peekb(address + 2, memory) << 16 |
      peekb(address + 3, memory) << 24;
  }

  return 0;
}

#endif /* MOCK_MEMORY_H */
