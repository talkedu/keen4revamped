// Actor state tables.
//
// The original keeps every statetype in DGROUP, with far pointers to the
// think/contact/react routines. We read those 30-byte records from the
// player's exe and translate each far pointer to the C++ function ported
// from that address. Every ported routine registers its original address
// with CK_FUNC, so a state that points at code we have not ported yet stops
// the game with the address instead of misbehaving.

#include <cstdio>
#include <map>
#include <memory>
#include <string>

#include "exe/keen4exe.h"
#include "game/ck_def.h"

namespace {

using keen4::exe::g_exe;

struct FuncInfo {
  void* fn;
  const char* name;
};

std::map<std::uint32_t, FuncInfo>& Registry() {
  static std::map<std::uint32_t, FuncInfo> registry;
  return registry;
}

std::map<word, std::unique_ptr<statetype>>& States() {
  static std::map<word, std::unique_ptr<statetype>> states;
  return states;
}

void* LookupFunc(word dgofs, int field) {
  word ofs = g_exe.Word(static_cast<std::uint16_t>(dgofs + field));
  word seg = g_exe.Word(static_cast<std::uint16_t>(dgofs + field + 2));
  if (!seg && !ofs) return nullptr;
  auto it = Registry().find((static_cast<std::uint32_t>(seg) << 16) | ofs);
  if (it == Registry().end()) {
    static char message[96];
    std::snprintf(message, sizeof message, "State %04X uses unported routine %04X:%04X", dgofs, seg, ofs);
    Quit(message);
  }
  return it->second.fn;
}

}  // namespace

void CK_RegisterFunc(word seg, word ofs, void* fn, const char* name) {
  Registry()[(static_cast<std::uint32_t>(seg) << 16) | ofs] = {fn, name};
}

statetype* ST(word dgofs) {
  auto& states = States();
  auto it = states.find(dgofs);
  if (it != states.end()) return it->second.get();

  auto owned = std::make_unique<statetype>();
  statetype* state = owned.get();
  states[dgofs] = std::move(owned);

  state->dgofs = dgofs;
  state->leftshapenum = g_exe.Int(dgofs);
  state->rightshapenum = g_exe.Int(static_cast<std::uint16_t>(dgofs + 2));
  state->progress = static_cast<progresstype>(g_exe.Int(static_cast<std::uint16_t>(dgofs + 4)));
  state->skippable = g_exe.Int(static_cast<std::uint16_t>(dgofs + 6));
  state->pushtofloor = g_exe.Int(static_cast<std::uint16_t>(dgofs + 8));
  state->tictime = g_exe.Int(static_cast<std::uint16_t>(dgofs + 0x0A));
  state->xmove = g_exe.Int(static_cast<std::uint16_t>(dgofs + 0x0C));
  state->ymove = g_exe.Int(static_cast<std::uint16_t>(dgofs + 0x0E));
  state->think = reinterpret_cast<thinkfunc>(LookupFunc(dgofs, 0x10));
  state->contact = reinterpret_cast<contactfunc>(LookupFunc(dgofs, 0x14));
  state->react = reinterpret_cast<thinkfunc>(LookupFunc(dgofs, 0x18));
  word next = g_exe.Word(static_cast<std::uint16_t>(dgofs + 0x1C));
  state->nextstate = next ? ST(next) : nullptr;
  return state;
}

void CK_InitStates() { States().clear(); }
