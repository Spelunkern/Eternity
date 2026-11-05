# Progreso de Eternity

Generado el 2026-11-05 con `python3 tools/coverage.py` sobre `coordinator/foundation`. No editar a mano.

**El servidor no es jugable todavía.** Estas cifras dicen de qué funciones originales hay código que las cita, no que el comportamiento sea idéntico. El objetivo se cumple recién cuando un cliente EP4 original conecta y juega.

## Cómo leer los estados

| Estado | Significa |
|---|---|
| En código enlazado al ejecutable | Citada en archivos que `src/main.cpp` incluye. No implica que se ejecute durante el juego: hoy seleccionar un personaje todavía no entra al mundo |
| Código con prueba u oráculo | Citada en `src`/`include` y en pruebas o herramientas |
| Código sin prueba | Citada solo en `src`/`include` |
| Solo un tramo interno | El código cita direcciones dentro de la función, no la función entera |
| Solo mencionada | Aparece en pruebas, herramientas o como pendiente, sin código |
| Sin rastro | No hay ninguna cita en código, pruebas ni herramientas |

## Binarios originales

Solo código propio del proyecto: sin CRT ni bibliotecas comunes (`pscommon`, `snet`). Los bytes de una función citada cuentan enteros aunque esté incompleta, así que el porcentaje es un techo.

| Binario | Funciones | Con código | % funciones | Bytes | Con código | % bytes | Avance por bytes |
|---|---:|---:|---:|---:|---:|---:|---|
| `ps_game` | 2559 | 675 | 26.4 % | 646 KB | 341 KB | 52.8 % | `██████░░░░░░` |
| `ps_dbAgent` | 644 | 43 | 6.7 % | 129 KB | 34 KB | 26.7 % | `███░░░░░░░░░` |
| `ps_login` | 218 | 6 | 2.8 % | 18 KB | 3 KB | 13.8 % | `██░░░░░░░░░░` |
| `ps_session` | 299 | 1 | 0.3 % | 34 KB | 1 KB | 3.4 % | `░░░░░░░░░░░░` |
| `ps_gameLog` | 143 | 0 | 0.0 % | 21 KB | 0 KB | 0.0 % | `░░░░░░░░░░░░` |
| `ps_userLog` | 132 | 0 | 0.0 % | 7 KB | 0 KB | 0.0 % | `░░░░░░░░░░░░` |

## `ps_game` por estado

| Estado | Funciones | Bytes | % bytes |
|---|---:|---:|---:|
| En código enlazado al ejecutable | 265 | 144 KB | 22.3 % |
| Código con prueba u oráculo | 275 | 152 KB | 23.5 % |
| Código sin prueba | 106 | 29 KB | 4.5 % |
| Solo un tramo interno | 29 | 17 KB | 2.6 % |
| Solo mencionada | 112 | 26 KB | 4.0 % |
| Sin rastro | 1772 | 279 KB | 43.2 % |

## `ps_game` por rama

Una función cuenta si la rama tiene código que la cite, entera o en parte.

| Rama | Funciones con código | % bytes |
|---|---:|---:|
| `main` | 507 | 41.4 % |
| `coordinator/foundation` | 675 | 52.8 % |
| `overnight/01-entry` | 546 | 43.5 % |
| `overnight/02-world` | 585 | 45.1 % |
| `overnight/03-movement` | 548 | 43.6 % |
| `overnight/04-mobs` | 567 | 45.7 % |
| `overnight/05-combat` | 568 | 45.1 % |
| `overnight/06-inventory` | 565 | 44.8 % |
| `overnight/07-quests` | 557 | 44.4 % |
| `overnight/08-social` | 570 | 45.9 % |
| `overnight/09-network` | 546 | 43.7 % |
| `overnight/snapshot-20261105` | 543 | 43.5 % |
| `todas las ramas comparadas` | 681 | 53.1 % |

## `ps_game` por archivo original

| Archivo | Funciones | Con código | Bytes | % bytes | Avance |
|---|---:|---:|---:|---:|---|
| `GameData.obj` | 50 | 19 | 40 KB | 64.7 % | `████████░░░░` |
| `GuildMgr.obj` | 172 | 37 | 35 KB | 40.5 % | `█████░░░░░░░` |
| `Mob.obj` | 102 | 25 | 30 KB | 75.4 % | `█████████░░░` |
| `MarketMgr.obj` | 109 | 1 | 24 KB | 0.1 % | `░░░░░░░░░░░░` |
| `UserItemRepairGem.obj` | 25 | 1 | 22 KB | 1.4 % | `░░░░░░░░░░░░` |
| `UserPacket.obj` | 31 | 13 | 20 KB | 92.4 % | `███████████░` |
| `UserItem.obj` | 37 | 13 | 17 KB | 51.1 % | `██████░░░░░░` |
| `Map.obj` | 39 | 7 | 15 KB | 75.4 % | `█████████░░░` |
| `UserPacketExtAdminCmd.obj` | 13 | 0 | 15 KB | 0.0 % | `░░░░░░░░░░░░` |
| `World.obj` | 114 | 23 | 13 KB | 42.4 % | `█████░░░░░░░` |
| `Zone.obj` | 92 | 31 | 12 KB | 54.0 % | `██████░░░░░░` |
| `UserDeath.obj` | 26 | 14 | 12 KB | 70.8 % | `████████░░░░` |
| `MobLuaFSM.obj` | 35 | 13 | 11 KB | 51.8 % | `██████░░░░░░` |
| `PartyMgr.obj` | 65 | 34 | 11 KB | 78.1 % | `█████████░░░` |
| `UserPacketDBAgent.obj` | 24 | 8 | 10 KB | 74.6 % | `█████████░░░` |
| `MobLuaDispatcher.obj` | 41 | 7 | 10 KB | 8.5 % | `█░░░░░░░░░░░` |
| `MobAttack.obj` | 17 | 9 | 10 KB | 97.8 % | `████████████` |
| `AIFactory.obj` | 41 | 5 | 10 KB | 23.2 % | `███░░░░░░░░░` |
| `UserAttackState.obj` | 24 | 12 | 10 KB | 88.2 % | `███████████░` |
| `User.obj` | 60 | 16 | 10 KB | 80.1 % | `██████████░░` |
| `UserAttackSkill.obj` | 15 | 7 | 9 KB | 70.4 % | `████████░░░░` |
| `UserState.obj` | 25 | 14 | 9 KB | 73.5 % | `█████████░░░` |
| `ServerApp.obj` | 52 | 5 | 9 KB | 20.6 % | `██░░░░░░░░░░` |
| `MobAttackSkill.obj` | 17 | 3 | 8 KB | 5.6 % | `█░░░░░░░░░░░` |
| `ZoneUpdate.obj` | 32 | 17 | 7 KB | 85.5 % | `██████████░░` |
| `Cell.obj` | 68 | 10 | 7 KB | 45.4 % | `█████░░░░░░░` |
| `UserSkillApply.obj` | 11 | 10 | 7 KB | 99.4 % | `████████████` |
| `UserAttackSkillUser.obj` | 18 | 5 | 7 KB | 53.0 % | `██████░░░░░░` |
| `UserUpdate.obj` | 15 | 6 | 7 KB | 43.7 % | `█████░░░░░░░` |
| `UserItemUse.obj` | 6 | 1 | 7 KB | 9.1 % | `█░░░░░░░░░░░` |
| `UserSkillApplyRem.obj` | 13 | 5 | 7 KB | 34.2 % | `████░░░░░░░░` |
| `ZoneMobGen.obj` | 28 | 5 | 7 KB | 13.8 % | `██░░░░░░░░░░` |
| `UserSkillApplyChk.obj` | 9 | 6 | 7 KB | 54.4 % | `███████░░░░░` |
| `UserPacketExtAdmin.obj` | 9 | 0 | 6 KB | 0.0 % | `░░░░░░░░░░░░` |
| `UserPvP.obj` | 13 | 8 | 6 KB | 71.5 % | `█████████░░░` |
| `UserSend.obj` | 27 | 20 | 6 KB | 80.5 % | `██████████░░` |
| `Server.obj` | 52 | 3 | 6 KB | 12.4 % | `█░░░░░░░░░░░` |
| `UserPvPPacket.obj` | 6 | 2 | 6 KB | 7.3 % | `█░░░░░░░░░░░` |
| `DefineNpc.obj` | 25 | 3 | 6 KB | 63.4 % | `████████░░░░` |
| `Npc.obj` | 20 | 6 | 5 KB | 81.7 % | `██████████░░` |
| `WorldThread.obj` | 52 | 11 | 5 KB | 74.1 % | `█████████░░░` |
| `ObjectMgr.obj` | 84 | 14 | 5 KB | 23.2 % | `███░░░░░░░░░` |
| `UserPacketGuild.obj` | 6 | 4 | 5 KB | 97.8 % | `████████████` |
| `UserPacketExchange.obj` | 9 | 7 | 5 KB | 99.7 % | `████████████` |
| `AStar.obj` | 61 | 2 | 5 KB | 38.0 % | `█████░░░░░░░` |
| `UserPacketMyShop.obj` | 17 | 3 | 5 KB | 7.7 % | `█░░░░░░░░░░░` |
| `UserAttackMob.obj` | 8 | 5 | 5 KB | 96.8 % | `████████████` |
| `UserAttackUser.obj` | 6 | 2 | 5 KB | 39.1 % | `█████░░░░░░░` |
| `MobPath.obj` | 15 | 3 | 4 KB | 84.4 % | `██████████░░` |
| `UserQuest.obj` | 16 | 11 | 4 KB | 80.7 % | `██████████░░` |
| `MapObelisk.obj` | 12 | 7 | 4 KB | 94.5 % | `███████████░` |
| `MobAction.obj` | 17 | 7 | 4 KB | 78.0 % | `█████████░░░` |
| `ZoneSendView.obj` | 18 | 11 | 4 KB | 70.2 % | `████████░░░░` |
| `ZoneSend.obj` | 7 | 6 | 4 KB | 99.1 % | `████████████` |
| `WorldUpdate.obj` | 26 | 10 | 4 KB | 68.6 % | `████████░░░░` |
| `ZoneMapInit.obj` | 18 | 4 | 4 KB | 86.7 % | `██████████░░` |
| `UserSendCharacter.obj` | 16 | 11 | 4 KB | 69.5 % | `████████░░░░` |
| `UserAttackSkillMob.obj` | 9 | 2 | 4 KB | 15.0 % | `██░░░░░░░░░░` |
| `WorldZone.obj` | 24 | 2 | 4 KB | 47.3 % | `██████░░░░░░` |
| `Item.obj` | 19 | 8 | 4 KB | 56.9 % | `███████░░░░░` |
| `ZoneMapBothMob.obj` | 9 | 7 | 4 KB | 98.1 % | `████████████` |
| `MiniGame.obj` | 20 | 0 | 4 KB | 0.0 % | `░░░░░░░░░░░░` |
| `MobApply.obj` | 9 | 9 | 4 KB | 100.0 % | `████████████` |
| `UserQuestChk.obj` | 14 | 8 | 3 KB | 80.7 % | `██████████░░` |
| `UserPacketPoint.obj` | 4 | 0 | 3 KB | 0.0 % | `░░░░░░░░░░░░` |
| `MobItemDrop.obj` | 7 | 4 | 3 KB | 97.2 % | `████████████` |
| `ClientToDBAgent.obj` | 27 | 4 | 3 KB | 80.6 % | `██████████░░` |
| `UserQuestionMgr.obj` | 31 | 1 | 3 KB | 0.1 % | `░░░░░░░░░░░░` |
| `UserPacketFriend.obj` | 3 | 2 | 3 KB | 93.5 % | `███████████░` |
| `UserSelect.obj` | 2 | 1 | 3 KB | 98.3 % | `████████████` |
| `UserPacketMarket.obj` | 2 | 0 | 3 KB | 0.0 % | `░░░░░░░░░░░░` |
| `Damage.obj` | 39 | 4 | 2 KB | 28.4 % | `███░░░░░░░░░` |
| `MobNativeFunction.obj` | 7 | 7 | 2 KB | 100.0 % | `████████████` |
| `ClientToMgr.obj` | 7 | 1 | 2 KB | 85.1 % | `██████████░░` |
| `UserAttack.obj` | 18 | 8 | 2 KB | 61.3 % | `███████░░░░░` |
| `Friend.obj` | 13 | 1 | 2 KB | 10.4 % | `█░░░░░░░░░░░` |
| `MobUpdate.obj` | 15 | 4 | 2 KB | 68.4 % | `████████░░░░` |
| `UserResetStatus.obj` | 2 | 0 | 2 KB | 0.0 % | `░░░░░░░░░░░░` |
| `MobPAISFunction.obj` | 10 | 0 | 2 KB | 0.0 % | `░░░░░░░░░░░░` |
| `UserAttackSkillUserParty.obj` | 4 | 0 | 2 KB | 0.0 % | `░░░░░░░░░░░░` |
| `MapInit.obj` | 8 | 3 | 2 KB | 91.1 % | `███████████░` |
| `DefineQuest.obj` | 1 | 1 | 2 KB | 100.0 % | `████████████` |
| `UserAttackSkillBuffHeal.obj` | 3 | 2 | 2 KB | 69.8 % | `████████░░░░` |
| `UserAttackItemOption.obj` | 7 | 4 | 2 KB | 63.9 % | `████████░░░░` |
| `UserQuestKill.obj` | 6 | 3 | 2 KB | 94.9 % | `███████████░` |
| `LuaMob.obj` | 26 | 0 | 2 KB | 0.0 % | `░░░░░░░░░░░░` |
| `GuildMgrPvP.obj` | 6 | 4 | 1 KB | 68.6 % | `████████░░░░` |
| `UserSkill.obj` | 3 | 1 | 1 KB | 56.8 % | `███████░░░░░` |
| `UserStatusKillResult.obj` | 17 | 0 | 1 KB | 0.0 % | `░░░░░░░░░░░░` |
| `MapSend.obj` | 11 | 6 | 1 KB | 85.2 % | `██████████░░` |
| `GameDataNpcQuest.obj` | 28 | 1 | 1 KB | 20.8 % | `██░░░░░░░░░░` |
| `Global.obj` | 45 | 0 | 1 KB | 0.0 % | `░░░░░░░░░░░░` |
| `ServerWorldThread.obj` | 6 | 2 | 1 KB | 78.8 % | `█████████░░░` |
| `UserSkillPassive.obj` | 5 | 3 | 1 KB | 54.4 % | `███████░░░░░` |
| `MapPath.obj` | 3 | 2 | 1 KB | 95.6 % | `███████████░` |
| `UserBilling.obj` | 9 | 2 | 1 KB | 6.7 % | `█░░░░░░░░░░░` |
| `ZoneMapUpdate.obj` | 5 | 3 | 1 KB | 82.7 % | `██████████░░` |
| `UserPacketChar.obj` | 3 | 3 | 1 KB | 100.0 % | `████████████` |
| `MobFSMAttack.obj` | 4 | 1 | 1 KB | 11.8 % | `█░░░░░░░░░░░` |
| `Object.obj` | 21 | 0 | 1 KB | 0.0 % | `░░░░░░░░░░░░` |
| `ZoneSendFromWorld.obj` | 5 | 2 | 1 KB | 86.0 % | `██████████░░` |
| `ObjectConnection.obj` | 12 | 4 | 1 KB | 50.6 % | `██████░░░░░░` |
| `UserItemSkill.obj` | 3 | 0 | 1 KB | 0.0 % | `░░░░░░░░░░░░` |
| `UserCrypto.obj` | 13 | 4 | 1 KB | 57.7 % | `███████░░░░░` |
| `UserSkillApplyInit.obj` | 2 | 2 | 0 KB | 100.0 % | `████████████` |
| `NProtect.obj` | 11 | 0 | 0 KB | 0.0 % | `░░░░░░░░░░░░` |
| `SRandom.obj` | 4 | 3 | 0 KB | 99.2 % | `████████████` |
| `UserPacketMiniGame.obj` | 1 | 0 | 0 KB | 0.0 % | `░░░░░░░░░░░░` |
| `ClientToGameLog.obj` | 9 | 0 | 0 KB | 0.0 % | `░░░░░░░░░░░░` |
| `Door.obj` | 4 | 1 | 0 KB | 47.4 % | `██████░░░░░░` |
| `ObjectMoveable.obj` | 2 | 0 | 0 KB | 0.0 % | `░░░░░░░░░░░░` |
| `UserGameLog.obj` | 1 | 1 | 0 KB | 100.0 % | `████████████` |
| `MapDefine.obj` | 1 | 1 | 0 KB | 100.0 % | `████████████` |
| `Main.obj` | 3 | 0 | 0 KB | 0.0 % | `░░░░░░░░░░░░` |
| `AdminNotice.obj` | 1 | 0 | 0 KB | 0.0 % | `░░░░░░░░░░░░` |
| `Dead.obj` | 2 | 1 | 0 KB | 95.5 % | `███████████░` |
| `MapRegion.obj` | 3 | 0 | 0 KB | 0.0 % | `░░░░░░░░░░░░` |
| `Trab.obj` | 2 | 0 | 0 KB | 0.0 % | `░░░░░░░░░░░░` |
| `Quest.obj` | 2 | 0 | 0 KB | 0.0 % | `░░░░░░░░░░░░` |

## Handlers de paquetes de `CUser`

| Handler | Dirección | Tamaño | Estado |
|---|---|---:|---|
| `CUser::PacketGuild` | `004764a0` | 4768 | En código enlazado al ejecutable |
| `CUser::PacketUserDBSetInfo` | `0046d780` | 4068 | En código enlazado al ejecutable |
| `CUser::PacketComm` | `00475870` | 2620 | En código enlazado al ejecutable |
| `CUser::PacketUserDBChar` | `0046cf60` | 2080 | En código enlazado al ejecutable |
| `CUser::PacketProcessing` | `00466da0` | 1898 | En código enlazado al ejecutable |
| `CUser::PacketCharacter` | `0046bf20` | 810 | En código enlazado al ejecutable |
| `CUser::PacketMaininterface` | `00468420` | 6348 | Código con prueba u oráculo |
| `CUser::PacketShop` | `0046af10` | 3609 | Código con prueba u oráculo |
| `CUser::PacketParty` | `004676c0` | 3424 | Código con prueba u oráculo |
| `CUser::PacketPC` | `0046a230` | 3285 | Código con prueba u oráculo |
| `CUser::PacketExchange` | `0046ec90` | 1664 | Código con prueba u oráculo |
| `CUser::PacketQuest` | `0046be10` | 156 | Código con prueba u oráculo |
| `CUser::PacketNpc` | `00467510` | 67 | Código con prueba u oráculo |
| `CUser::PacketUserDBPoint` | `0046e880` | 972 | Solo mencionada |
| `CUser::PacketBilling` | `00457b40` | 786 | Solo mencionada |
| `CUser::PacketUserDBFriend` | `0046e770` | 145 | Solo mencionada |
| `CUser::PacketUserDBSavePoint` | `0046e810` | 101 | Solo mencionada |
| `CUser::PacketAdminCmdB` | `00471f40` | 4572 | Sin rastro |
| `CUser::PacketAdminCmdD` | `00473970` | 3788 | Sin rastro |
| `CUser::PacketAdminCmdE` | `00474840` | 3708 | Sin rastro |
| `CUser::PacketPoint` | `004797c0` | 3228 | Sin rastro |
| `CUser::PacketMyShop` | `00478bd0` | 2976 | Sin rastro |
| `CUser::PacketMarket` | `00477740` | 2720 | Sin rastro |
| `CUser::PacketAdminChat` | `00470e90` | 2484 | Sin rastro |
| `CUser::PacketChatE` | `00470490` | 2482 | Sin rastro |
| `CUser::PacketPVP` | `0047d0f0` | 2420 | Sin rastro |
| `CUser::PacketAdminCmdC` | `00473240` | 1840 | Sin rastro |
| `CUser::PacketPVPGuild` | `0047da70` | 1426 | Sin rastro |
| `CUser::PacketGetInfo` | `00469cf0` | 948 | Sin rastro |
| `CUser::PacketAdminCmdA` | `00471d00` | 576 | Sin rastro |
| `CUser::PacketMiniGame` | `004781f0` | 368 | Sin rastro |
| `CUser::PacketPriest` | `00467560` | 253 | Sin rastro |
| `CUser::PacketAdminUQ` | `00471850` | 248 | Sin rastro |
| `CUser::PacketGem` | `0046bd40` | 204 | Sin rastro |
| `CUser::PacketUQ` | `00470e50` | 64 | Sin rastro |

## Funciones pendientes más grandes de `ps_game`

| Función | Dirección | Tamaño | Archivo |
|---|---|---:|---|
| `CUser::ItemUse` | `004655e0` | 5684 | `UserItemUse.obj` |
| `CUser::ItemGemRemoveAll` | `00461fc0` | 5062 | `UserItemRepairGem.obj` |
| `CGameData::ReLoadConstSkill` | `004c3ed0` | 4582 | `GameData.obj` |
| `CUser::PacketAdminCmdB` | `00471f40` | 4572 | `UserPacketExtAdminCmd.obj` |
| `CUser::ItemGemRemovePos` | `004633a0` | 3887 | `UserItemRepairGem.obj` |
| `CUser::PacketAdminCmdD` | `00473970` | 3788 | `UserPacketExtAdminCmd.obj` |
| `CUser::PacketAdminCmdE` | `00474840` | 3708 | `UserPacketExtAdminCmd.obj` |
| `CGameData::ReLoadConstItem` | `004c31a0` | 3373 | `GameData.obj` |
| `CServerApp::OnConsoleCommand` | `00402ce0` | 3276 | `ServerApp.obj` |
| `CUser::PacketPoint` | `004797c0` | 3228 | `UserPacketPoint.obj` |
| `CUser::ItemGemAdd` | `00461310` | 3218 | `UserItemRepairGem.obj` |
| `CUser::UpdateResetPosition` | `0048d470` | 2991 | `UserUpdate.obj` |
| `CUser::PacketMyShop` | `00478bd0` | 2976 | `UserPacketMyShop.obj` |
| `CMarketMgr::RetrievalItem` | `0043d210` | 2878 | `MarketMgr.obj` |
| `CAIFactory::LoadStateMachineText` | `004a9d90` | 2776 | `AIFactory.obj` |
| `CUser::PacketMarket` | `00477740` | 2720 | `UserPacketMarket.obj` |
| `CUser::AttackDamage` | `00457030` | 2592 | `UserAttackUser.obj` |
| `CUser::ItemLapisianAdd` | `0045fb20` | 2590 | `UserItemRepairGem.obj` |
| `CUser::PacketAdminChat` | `00470e90` | 2484 | `UserPacketExtAdmin.obj` |
| `CUser::PacketChatE` | `00470490` | 2482 | `UserPacketExtAdmin.obj` |
| `CUser::PacketPVP` | `0047d0f0` | 2420 | `UserPvPPacket.obj` |
| `CMob::AttackUser` | `004a3fe0` | 2292 | `MobAttackSkill.obj` |
| `CMarketMgr::CheckMarketItemEndDate` | `00439240` | 2290 | `MarketMgr.obj` |
| `CMob::ChaseExp` | `004b6be0` | 2212 | `MobLuaFSM.obj` |
| `CUser::ItemBagToBag` | `0045b690` | 2205 | `UserItem.obj` |
| `CGuildMgr::UpdateGuildPointRank` | `00432b70` | 1952 | `GuildMgr.obj` |
| `CGuild::ItemGuildBankToBag` | `0042fea0` | 1951 | `GuildMgr.obj` |
| `CUser::ItemRepair` | `00464340` | 1932 | `UserItemRepairGem.obj` |
| `CUser::ItemRemake` | `004608b0` | 1918 | `UserItemRepairGem.obj` |
| `CGuild::ItemBagToGuildBank` | `0042f390` | 1889 | `GuildMgr.obj` |
| `CMarketMgr::DirectBuy` | `0043bd70` | 1848 | `MarketMgr.obj` |
| `CUser::PacketAdminCmdC` | `00473240` | 1840 | `UserPacketExtAdminCmd.obj` |
| `CMarketMgr::Tender` | `0043c4c0` | 1827 | `MarketMgr.obj` |
| `CUser::LevelChange` | `0048b0e0` | 1688 | `UserState.obj` |
| `CMob::DoFunction` | `004917a0` | 1547 | `Mob.obj` |
| `CUser::ChkProtect` | `00486830` | 1478 | `UserSkillApplyChk.obj` |
| `CUser::ExchangePvP` | `0047caf0` | 1448 | `UserPvPPacket.obj` |
| `CUser::ItemBankToBag` | `0045bf30` | 1443 | `UserItem.obj` |
| `CUser::SkillAttackRangeSelect` | `00450f70` | 1432 | `UserAttackSkillMob.obj` |
| `CUser::PacketPVPGuild` | `0047da70` | 1426 | `UserPvPPacket.obj` |

El detalle función por función está en `research/coverage/<binario>.tsv`.
