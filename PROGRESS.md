# Eternity progress

Generated on 2026-11-05 by `python3 tools/coverage.py` from `main`. Do not edit by hand.

**The server is not playable yet.** These figures show which original functions are cited by code, not that behavior is identical, and a function's bytes count in full even when incomplete, so the percentages are ceilings. The goal is met only when an original EP4 client connects and plays.

## Reading the statuses

| Status | Meaning |
|---|---|
| In code linked into the executable | Cited by files `src/main.cpp` includes. This does not mean it runs during play: selecting a character does not enter the world yet |
| Code with a test or oracle | Cited in `src`/`include` and in tests or tools |
| Code without a test | Cited only in `src`/`include` |
| Internal range only | Code cites addresses inside the function, not the function itself |
| Required port only | Declared as a service other code requires; not reconstructed. Not counted as code |
| Mentioned only | Appears in tests, tools or as pending, with no code |
| No trace | No citation in code, tests or tools |

## Original binaries

Project code only: excludes the CRT and shared libraries (`pscommon`, `snet`).

| Binary | Functions | With code | % functions | Bytes | With code | % bytes | Progress by bytes |
|---|---:|---:|---:|---:|---:|---:|---|
| `ps_game` | 2559 | 668 | 26.1% | 646 KB | 335 KB | 52.0% | `██████░░░░░░` |
| `ps_dbAgent` | 644 | 44 | 6.8% | 129 KB | 35 KB | 26.8% | `███░░░░░░░░░` |
| `ps_login` | 218 | 6 | 2.8% | 18 KB | 3 KB | 13.8% | `██░░░░░░░░░░` |
| `ps_session` | 299 | 1 | 0.3% | 34 KB | 1 KB | 3.4% | `░░░░░░░░░░░░` |
| `ps_gameLog` | 143 | 0 | 0.0% | 21 KB | 0 KB | 0.0% | `░░░░░░░░░░░░` |
| `ps_userLog` | 132 | 0 | 0.0% | 7 KB | 0 KB | 0.0% | `░░░░░░░░░░░░` |

## `ps_game` by status

| Status | Functions | Bytes | % bytes |
|---|---:|---:|---:|
| In code linked into the executable | 265 | 144 KB | 22.3% |
| Code with a test or oracle | 277 | 154 KB | 23.8% |
| Code without a test | 98 | 23 KB | 3.6% |
| Internal range only | 28 | 15 KB | 2.3% |
| Required port only | 12 | 7 KB | 1.1% |
| Mentioned only | 169 | 42 KB | 6.6% |
| No trace | 1710 | 261 KB | 40.4% |

## `ps_game` by original source file

| File | Functions | With code | Bytes | % bytes | Progress |
|---|---:|---:|---:|---:|---|
| `GameData.obj` | 50 | 19 | 40 KB | 64.7% | `████████░░░░` |
| `GuildMgr.obj` | 172 | 38 | 35 KB | 42.1% | `█████░░░░░░░` |
| `Mob.obj` | 102 | 26 | 30 KB | 76.1% | `█████████░░░` |
| `MarketMgr.obj` | 109 | 1 | 24 KB | 0.1% | `░░░░░░░░░░░░` |
| `UserItemRepairGem.obj` | 25 | 1 | 22 KB | 1.4% | `░░░░░░░░░░░░` |
| `UserPacket.obj` | 31 | 12 | 20 KB | 92.2% | `███████████░` |
| `UserItem.obj` | 37 | 13 | 17 KB | 51.1% | `██████░░░░░░` |
| `Map.obj` | 39 | 7 | 15 KB | 75.4% | `█████████░░░` |
| `UserPacketExtAdminCmd.obj` | 13 | 0 | 15 KB | 0.0% | `░░░░░░░░░░░░` |
| `World.obj` | 114 | 23 | 13 KB | 42.4% | `█████░░░░░░░` |
| `Zone.obj` | 92 | 30 | 12 KB | 53.4% | `██████░░░░░░` |
| `UserDeath.obj` | 26 | 11 | 12 KB | 53.8% | `██████░░░░░░` |
| `MobLuaFSM.obj` | 35 | 13 | 11 KB | 51.8% | `██████░░░░░░` |
| `PartyMgr.obj` | 65 | 34 | 11 KB | 78.1% | `█████████░░░` |
| `UserPacketDBAgent.obj` | 24 | 8 | 10 KB | 74.6% | `█████████░░░` |
| `MobLuaDispatcher.obj` | 41 | 7 | 10 KB | 8.5% | `█░░░░░░░░░░░` |
| `MobAttack.obj` | 17 | 9 | 10 KB | 97.8% | `████████████` |
| `AIFactory.obj` | 41 | 5 | 10 KB | 23.2% | `███░░░░░░░░░` |
| `UserAttackState.obj` | 24 | 12 | 10 KB | 88.2% | `███████████░` |
| `User.obj` | 60 | 16 | 10 KB | 80.1% | `██████████░░` |
| `UserAttackSkill.obj` | 15 | 7 | 9 KB | 70.4% | `████████░░░░` |
| `UserState.obj` | 25 | 14 | 9 KB | 73.5% | `█████████░░░` |
| `ServerApp.obj` | 52 | 4 | 9 KB | 19.6% | `██░░░░░░░░░░` |
| `MobAttackSkill.obj` | 17 | 6 | 8 KB | 15.4% | `██░░░░░░░░░░` |
| `ZoneUpdate.obj` | 32 | 17 | 7 KB | 85.5% | `██████████░░` |
| `Cell.obj` | 68 | 10 | 7 KB | 45.4% | `█████░░░░░░░` |
| `UserSkillApply.obj` | 11 | 10 | 7 KB | 99.4% | `████████████` |
| `UserAttackSkillUser.obj` | 18 | 3 | 7 KB | 10.4% | `█░░░░░░░░░░░` |
| `UserUpdate.obj` | 15 | 6 | 7 KB | 43.7% | `█████░░░░░░░` |
| `UserItemUse.obj` | 6 | 1 | 7 KB | 9.1% | `█░░░░░░░░░░░` |
| `UserSkillApplyRem.obj` | 13 | 5 | 7 KB | 34.2% | `████░░░░░░░░` |
| `ZoneMobGen.obj` | 28 | 5 | 7 KB | 13.8% | `██░░░░░░░░░░` |
| `UserSkillApplyChk.obj` | 9 | 6 | 7 KB | 54.4% | `███████░░░░░` |
| `UserPacketExtAdmin.obj` | 9 | 0 | 6 KB | 0.0% | `░░░░░░░░░░░░` |
| `UserPvP.obj` | 13 | 7 | 6 KB | 55.9% | `███████░░░░░` |
| `UserSend.obj` | 27 | 20 | 6 KB | 80.5% | `██████████░░` |
| `Server.obj` | 52 | 3 | 6 KB | 12.4% | `█░░░░░░░░░░░` |
| `UserPvPPacket.obj` | 6 | 2 | 6 KB | 7.3% | `█░░░░░░░░░░░` |
| `DefineNpc.obj` | 25 | 3 | 6 KB | 63.4% | `████████░░░░` |
| `Npc.obj` | 20 | 6 | 5 KB | 81.7% | `██████████░░` |
| `WorldThread.obj` | 52 | 11 | 5 KB | 74.1% | `█████████░░░` |
| `ObjectMgr.obj` | 84 | 14 | 5 KB | 23.2% | `███░░░░░░░░░` |
| `UserPacketGuild.obj` | 6 | 4 | 5 KB | 97.8% | `████████████` |
| `UserPacketExchange.obj` | 9 | 7 | 5 KB | 99.7% | `████████████` |
| `AStar.obj` | 61 | 2 | 5 KB | 38.0% | `█████░░░░░░░` |
| `UserPacketMyShop.obj` | 17 | 3 | 5 KB | 7.7% | `█░░░░░░░░░░░` |
| `UserAttackMob.obj` | 8 | 5 | 5 KB | 96.8% | `████████████` |
| `UserAttackUser.obj` | 6 | 1 | 5 KB | 25.2% | `███░░░░░░░░░` |
| `MobPath.obj` | 15 | 3 | 4 KB | 84.4% | `██████████░░` |
| `UserQuest.obj` | 16 | 11 | 4 KB | 80.7% | `██████████░░` |
| `MapObelisk.obj` | 12 | 7 | 4 KB | 94.5% | `███████████░` |
| `MobAction.obj` | 17 | 7 | 4 KB | 78.0% | `█████████░░░` |
| `ZoneSendView.obj` | 18 | 11 | 4 KB | 70.2% | `████████░░░░` |
| `ZoneSend.obj` | 7 | 6 | 4 KB | 99.1% | `████████████` |
| `WorldUpdate.obj` | 26 | 10 | 4 KB | 68.6% | `████████░░░░` |
| `ZoneMapInit.obj` | 18 | 4 | 4 KB | 86.7% | `██████████░░` |
| `UserSendCharacter.obj` | 16 | 11 | 4 KB | 69.5% | `████████░░░░` |
| `UserAttackSkillMob.obj` | 9 | 2 | 4 KB | 15.0% | `██░░░░░░░░░░` |
| `WorldZone.obj` | 24 | 2 | 4 KB | 47.3% | `██████░░░░░░` |
| `Item.obj` | 19 | 8 | 4 KB | 56.9% | `███████░░░░░` |
| `ZoneMapBothMob.obj` | 9 | 7 | 4 KB | 98.1% | `████████████` |
| `MiniGame.obj` | 20 | 0 | 4 KB | 0.0% | `░░░░░░░░░░░░` |
| `MobApply.obj` | 9 | 9 | 4 KB | 100.0% | `████████████` |
| `UserQuestChk.obj` | 14 | 8 | 3 KB | 80.7% | `██████████░░` |
| `UserPacketPoint.obj` | 4 | 0 | 3 KB | 0.0% | `░░░░░░░░░░░░` |
| `MobItemDrop.obj` | 7 | 4 | 3 KB | 97.2% | `████████████` |
| `ClientToDBAgent.obj` | 27 | 4 | 3 KB | 80.6% | `██████████░░` |
| `UserQuestionMgr.obj` | 31 | 1 | 3 KB | 0.1% | `░░░░░░░░░░░░` |
| `UserPacketFriend.obj` | 3 | 2 | 3 KB | 93.5% | `███████████░` |
| `UserSelect.obj` | 2 | 1 | 3 KB | 98.3% | `████████████` |
| `UserPacketMarket.obj` | 2 | 0 | 3 KB | 0.0% | `░░░░░░░░░░░░` |
| `Damage.obj` | 39 | 4 | 2 KB | 28.4% | `███░░░░░░░░░` |
| `MobNativeFunction.obj` | 7 | 7 | 2 KB | 100.0% | `████████████` |
| `ClientToMgr.obj` | 7 | 1 | 2 KB | 85.1% | `██████████░░` |
| `UserAttack.obj` | 18 | 8 | 2 KB | 61.3% | `███████░░░░░` |
| `Friend.obj` | 13 | 1 | 2 KB | 10.4% | `█░░░░░░░░░░░` |
| `MobUpdate.obj` | 15 | 4 | 2 KB | 68.4% | `████████░░░░` |
| `UserResetStatus.obj` | 2 | 0 | 2 KB | 0.0% | `░░░░░░░░░░░░` |
| `MobPAISFunction.obj` | 10 | 0 | 2 KB | 0.0% | `░░░░░░░░░░░░` |
| `UserAttackSkillUserParty.obj` | 4 | 0 | 2 KB | 0.0% | `░░░░░░░░░░░░` |
| `MapInit.obj` | 8 | 3 | 2 KB | 91.1% | `███████████░` |
| `DefineQuest.obj` | 1 | 1 | 2 KB | 100.0% | `████████████` |
| `UserAttackSkillBuffHeal.obj` | 3 | 2 | 2 KB | 69.8% | `████████░░░░` |
| `UserAttackItemOption.obj` | 7 | 3 | 2 KB | 36.8% | `████░░░░░░░░` |
| `UserQuestKill.obj` | 6 | 3 | 2 KB | 94.9% | `███████████░` |
| `LuaMob.obj` | 26 | 0 | 2 KB | 0.0% | `░░░░░░░░░░░░` |
| `GuildMgrPvP.obj` | 6 | 4 | 1 KB | 68.6% | `████████░░░░` |
| `UserSkill.obj` | 3 | 1 | 1 KB | 56.8% | `███████░░░░░` |
| `UserStatusKillResult.obj` | 17 | 0 | 1 KB | 0.0% | `░░░░░░░░░░░░` |
| `MapSend.obj` | 11 | 6 | 1 KB | 85.2% | `██████████░░` |
| `GameDataNpcQuest.obj` | 28 | 1 | 1 KB | 20.8% | `██░░░░░░░░░░` |
| `Global.obj` | 45 | 0 | 1 KB | 0.0% | `░░░░░░░░░░░░` |
| `ServerWorldThread.obj` | 6 | 2 | 1 KB | 78.8% | `█████████░░░` |
| `UserSkillPassive.obj` | 5 | 3 | 1 KB | 54.4% | `███████░░░░░` |
| `MapPath.obj` | 3 | 2 | 1 KB | 95.6% | `███████████░` |
| `UserBilling.obj` | 9 | 1 | 1 KB | 4.0% | `░░░░░░░░░░░░` |
| `ZoneMapUpdate.obj` | 5 | 3 | 1 KB | 82.7% | `██████████░░` |
| `UserPacketChar.obj` | 3 | 3 | 1 KB | 100.0% | `████████████` |
| `MobFSMAttack.obj` | 4 | 1 | 1 KB | 11.8% | `█░░░░░░░░░░░` |
| `Object.obj` | 21 | 0 | 1 KB | 0.0% | `░░░░░░░░░░░░` |
| `ZoneSendFromWorld.obj` | 5 | 2 | 1 KB | 86.0% | `██████████░░` |
| `ObjectConnection.obj` | 12 | 4 | 1 KB | 50.6% | `██████░░░░░░` |
| `UserItemSkill.obj` | 3 | 0 | 1 KB | 0.0% | `░░░░░░░░░░░░` |
| `UserCrypto.obj` | 13 | 4 | 1 KB | 57.7% | `███████░░░░░` |
| `UserSkillApplyInit.obj` | 2 | 2 | 0 KB | 100.0% | `████████████` |
| `NProtect.obj` | 11 | 0 | 0 KB | 0.0% | `░░░░░░░░░░░░` |
| `SRandom.obj` | 4 | 3 | 0 KB | 99.2% | `████████████` |
| `UserPacketMiniGame.obj` | 1 | 0 | 0 KB | 0.0% | `░░░░░░░░░░░░` |
| `ClientToGameLog.obj` | 9 | 0 | 0 KB | 0.0% | `░░░░░░░░░░░░` |
| `Door.obj` | 4 | 1 | 0 KB | 47.4% | `██████░░░░░░` |
| `ObjectMoveable.obj` | 2 | 0 | 0 KB | 0.0% | `░░░░░░░░░░░░` |
| `UserGameLog.obj` | 1 | 1 | 0 KB | 100.0% | `████████████` |
| `MapDefine.obj` | 1 | 1 | 0 KB | 100.0% | `████████████` |
| `Main.obj` | 3 | 0 | 0 KB | 0.0% | `░░░░░░░░░░░░` |
| `AdminNotice.obj` | 1 | 0 | 0 KB | 0.0% | `░░░░░░░░░░░░` |
| `Dead.obj` | 2 | 1 | 0 KB | 95.5% | `███████████░` |
| `MapRegion.obj` | 3 | 0 | 0 KB | 0.0% | `░░░░░░░░░░░░` |
| `Trab.obj` | 2 | 0 | 0 KB | 0.0% | `░░░░░░░░░░░░` |
| `Quest.obj` | 2 | 0 | 0 KB | 0.0% | `░░░░░░░░░░░░` |

## `CUser` packet handlers

| Handler | Address | Size | Status |
|---|---|---:|---|
| `CUser::PacketGuild` | `004764a0` | 4768 | In code linked into the executable |
| `CUser::PacketUserDBSetInfo` | `0046d780` | 4068 | In code linked into the executable |
| `CUser::PacketComm` | `00475870` | 2620 | In code linked into the executable |
| `CUser::PacketUserDBChar` | `0046cf60` | 2080 | In code linked into the executable |
| `CUser::PacketProcessing` | `00466da0` | 1898 | In code linked into the executable |
| `CUser::PacketCharacter` | `0046bf20` | 810 | In code linked into the executable |
| `CUser::PacketMaininterface` | `00468420` | 6348 | Code with a test or oracle |
| `CUser::PacketShop` | `0046af10` | 3609 | Code with a test or oracle |
| `CUser::PacketParty` | `004676c0` | 3424 | Code with a test or oracle |
| `CUser::PacketPC` | `0046a230` | 3285 | Code with a test or oracle |
| `CUser::PacketExchange` | `0046ec90` | 1664 | Code with a test or oracle |
| `CUser::PacketQuest` | `0046be10` | 156 | Code with a test or oracle |
| `CUser::PacketNpc` | `00467510` | 67 | Code with a test or oracle |
| `CUser::PacketUserDBPoint` | `0046e880` | 972 | Mentioned only |
| `CUser::PacketBilling` | `00457b40` | 786 | Mentioned only |
| `CUser::PacketUserDBFriend` | `0046e770` | 145 | Mentioned only |
| `CUser::PacketUserDBSavePoint` | `0046e810` | 101 | Mentioned only |
| `CUser::PacketAdminCmdB` | `00471f40` | 4572 | No trace |
| `CUser::PacketAdminCmdD` | `00473970` | 3788 | No trace |
| `CUser::PacketAdminCmdE` | `00474840` | 3708 | No trace |
| `CUser::PacketPoint` | `004797c0` | 3228 | No trace |
| `CUser::PacketMyShop` | `00478bd0` | 2976 | No trace |
| `CUser::PacketMarket` | `00477740` | 2720 | No trace |
| `CUser::PacketAdminChat` | `00470e90` | 2484 | No trace |
| `CUser::PacketChatE` | `00470490` | 2482 | No trace |
| `CUser::PacketPVP` | `0047d0f0` | 2420 | No trace |
| `CUser::PacketAdminCmdC` | `00473240` | 1840 | No trace |
| `CUser::PacketPVPGuild` | `0047da70` | 1426 | No trace |
| `CUser::PacketGetInfo` | `00469cf0` | 948 | No trace |
| `CUser::PacketAdminCmdA` | `00471d00` | 576 | No trace |
| `CUser::PacketMiniGame` | `004781f0` | 368 | No trace |
| `CUser::PacketPriest` | `00467560` | 253 | No trace |
| `CUser::PacketAdminUQ` | `00471850` | 248 | No trace |
| `CUser::PacketGem` | `0046bd40` | 204 | No trace |
| `CUser::PacketUQ` | `00470e50` | 64 | No trace |

## Largest pending `ps_game` functions

| Function | Address | Size | File |
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
| `CUser::SkillAttackRangeSelect` | `00452560` | 1516 | `UserAttackSkillUser.obj` |
| `CUser::SkillAttackRange` | `00451f80` | 1496 | `UserAttackSkillUser.obj` |
| `CUser::ChkProtect` | `00486830` | 1478 | `UserSkillApplyChk.obj` |
| `CUser::ExchangePvP` | `0047caf0` | 1448 | `UserPvPPacket.obj` |
| `CUser::ItemBankToBag` | `0045bf30` | 1443 | `UserItem.obj` |

Function-by-function detail is in `research/coverage/<binary>.tsv`.
