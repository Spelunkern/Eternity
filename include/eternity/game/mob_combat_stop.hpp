#pragma once
namespace eternity::game {
class Actor;
// ps_game.exe CUser::StatusAttackStop 00489ed0. Reuse the canonical actor's
// recovered implementation. This operation has no transport/world callback.
// Actor, its scene and caller-held combat projections retain their lifetimes
// through the following Death call. Clear target TYPE only; retained target ID
// is inactive data and must not trigger target release or lookup here.
void mob_combat_stop(Actor&);
}
