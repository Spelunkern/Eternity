# Overnight coordination

**Historical, finished on 2026-11-05.** All sessions stopped and the reviewed work is
in `main`. Unintegrated branch state is preserved in the `backup/overnight/*` and
`backup/wip/*` tags. This log was written during the work and later entries are in
Spanish; it is kept unedited as a record.

On 2026-11-05, nine separate Codex CLI sessions were started in
`/home/franco/Projects/Eternity-overnight/01-entry` through `09-network`, using
`gpt-6.1-terra`, reasoning `low`, and service tier `default`. Each branch starts
from snapshot commit `b34b5e3`, which records a copy of the then-current dirty
foundation worktree on `overnight/snapshot-20261105`; it does not alter or commit
the coordinator worktree. Workers commit only in their own branches and report
in `reports/overnight/`. The live session status and output are under
`/home/franco/.local/share/eternity-overnight/`.

Each terminal resumes its own thread after a successful milestone, unless its
report explicitly ends in `WAITING_FOR_ASSIGNMENT`; errors stop that runner.
The dashboard monitor reflects terminal status and delivered milestones, which
are not automatically integrated or validated. The coordinator must review
each branch against the current foundation worktree, preserve independent
changes and use shared build/SQL locks. Never merge the snapshot wholesale over
the dirty foundation tree. Completion still requires actual original-client
gameplay and end-to-end validation; worker activity or commits do not prove it.

## Reviewed changes applied to the main worktree

- `02-world` commit `539533e`: applied the world startup shutdown fencing and its tests to the existing main worktree changes. The focused `vertical-world-unit` build and execution are queued behind the shared build lock; validation is pending.
- `06-inventory` commit `df229a6`: applied the pooled inventory admission ownership fix and regression assertion. The focused inventory and pickup builds and tests are queued behind the shared build lock; validation is pending.

These are selected patches, not merges of the worker branches. Other worker changes remain in their independent worktrees pending review, integration and validation. The original client path remains unverified.
- `01-entry` commit `7287eea`: applied the retained asynchronous selection adapter and the corresponding fixture changes. Focused TCP entry and session registry tests are queued with the preceding tests in one main-worktree build, preventing concurrent writes to that build directory. The adapter still needs real runtime wiring and validation.
- `09-network` commit `66688d7`: applied bounded byte accounting for received world frames and its encrypted burst TCP regression. The focused test is queued alongside the other main-worktree checks. The 64 KiB cap is native infrastructure, not a recovered original limit.

## Isolated SQL checks

The disposable SQL integration fixture passed `creation-network-integration` (TCP creation, duplicate names, relogin and pending deletion) and `vertical-persistence-sql-integration` (selection, mutation, exit save and reload) using binaries newer than their test sources. The test catalog was deleted and `eternity-sqlserver` stopped afterward. SQL Server readiness for these checks required waiting until the source catalog had recovered, not merely until `SELECT 1` responded. These checks do not exercise original-client gameplay or prove the new selection adapter's main-worktree build.
- `04-mobs` commit `3b29826`: applied the retained-zone idle, death and respawn frame adapter and its world fixture change. The queued world test will exercise the combined path; the production frame still needs actual runtime wiring.
- The disposable creation fixture now waits for all three source SQL catalogs to finish recovery before reading their schema. A simulated recovering-to-online transition and Python compilation passed; rerunning the full SQL fixture with this readiness change remains pending.
- `01-entry` commit `bd32b26`: applied the rollback of the database fence if posting a selection job throws, with its regression case. Native execution remains queued.
- `03-movement` commit `65b174b`: applied retained-zone visibility delivery and its scene fixture; production actor and channel ownership remains to be connected. Native execution remains queued.
- `04-mobs` commit `ab4cb4a`: applied the ordinary-idle targeting gate and its original-executable oracle data. The oracle output matched all six recovered traces; native execution remains queued.
- `08-social` commits `2d76832` and `913966b`: applied block-add handling and the original executable probe. Seven original cases passed, including the distinct live list and wire IDs; production lookup, routing and native execution remain pending.
- `02-world` commit `06512fb`: applied the recovered 250 ms world-loop wait arithmetic and boundary tests; native execution is queued, and the production loop remains unwired.
- `08-social` commit `ce193dc`: applied friend request handling and original-executable probe. Nine original request cases passed; request acceptance, timers and live owner routing remain pending. Native execution is queued.
- `07-quests` commit `5c37f4b`: applied the timed quest integration regression. Its native test still needs to be built and run.
- `01-entry` commit `018f330`: applied retained selection lifetime during reentrant teardown and the matching session-registry regression. Native execution remains queued.
- The `05-combat` branch `170be71` focused `vertical-combat-damage-unit` passed in its isolated worktree after the worker's full build. It exercises a fixture composition; production mob state and services are still unbound. The patch has not yet been applied to the main worktree.
- The main worktree's integrated C++ changes passed a containerized syntax-only check. Its focused native build has acquired the shared build slot and is running.
- Original-executable probes for `08-social` friend responses (`855d124`, six cases) and `04-mobs` ordinary movement (`092d0c2`, seven traces) passed in their worker trees. These later patches remain pending main-worktree integration until the current build finishes.
- `01-entry` commit `d898070`: applied the disposable SQL handoff fixture that runs the retained selection through the serialized roster adapter and checks the account fence. It passed a syntax check in the worker tree; building and rerunning the SQL integration against a disposable catalog remain necessary. The earlier SQL pass predates this fixture change.
- The network worker's binaries were newer than its changed source and tests. `world-network-unit` and `serialized-world-application-tcp-unit` both passed in that isolated worktree; the main-worktree versions remain in the currently running integrated build/test.

## Integrated build results

The main worktree compiled the selected targets and passed all ten focused tests: `inventory-unit`, `pickup-unit`, `world-network-unit`, `vertical-world-unit`, `vertical-mobs-unit`, `vertical-scene-unit`, `serialized-world-application-tcp-unit`, `vertical-async-entry-tcp-unit`, `vertical-session-registry-unit`, and `vertical-social-replica-unit`.

After that pass, applied `04-mobs` commit `092d0c2` (ordinary idle movement), `08-social` commit `855d124` (friend response), and the `05-combat` commit `170be71` regression. A second incremental build/test is queued for mobs, social, combat, quests, and the updated SQL integration executable. The SQL executable must subsequently be run in a disposable catalog; the ten-test pass above predates these three new changes.
- `07-quests` commit `27b8363`: applied quest packet routing, cancellation ordering and abandonment through the existing retained quest runtime. Real action, shop, transport and event providers remain to be bound; focused native execution is queued.
- `02-world` commit `f5abdb3`: applied phased traversal of retained permanent primary zones. The original also updates dynamic instances and additional phases; a complete production scheduler remains necessary. Focused native execution is queued.

The subsequent main-worktree incremental build completed, including the updated `vertical-persistence-sql-integration` executable. All five focused tests passed: `vertical-combat-damage-unit`, `vertical-world-unit`, `vertical-mobs-unit`, `vertical-quests-unit`, and `vertical-social-replica-unit`. The updated SQL handoff fixture still requires execution against a disposable SQL catalog.

The updated `vertical-persistence-sql-integration` passed with the retained selection owner and serialized roster handoff, using the isolated SQL fixture and actual SQL loader. Its randomly named test catalog was dropped and the SQL container stopped. This also exercised the fixture's source-database recovery wait; it does not establish production world entry.

Applied `09-network` commit `0fe84df` to update admission overflow coverage for the new byte budget; `06-inventory` commit `9eff12b` for drop request dispatch; and `04-mobs` commit `f5c70f4` for live actor targeting. The drop dispatch oracle matched all 1,024 original-executable cases. Focused native builds and tests for these newer changes are queued; live session wiring remains outstanding.

The `08-social` worker's friend invitation timeout/zone-leave probe (`5dfe50f`) matched nine original instruction-block cases, including tick wrap and shared request state. Its patch remains in the worker tree pending integration and native test execution.

The previously waiting `01-entry` and `03-movement` terminals were resumed with their original thread IDs, branches and Terra low/standard settings. Their new assignments explicitly pursue concrete session ownership and live movement composition, including missing real services, rather than stopping at a dependency list. Their earlier terminal windows had exited and were replaced; no active worker was terminated.

The `05-combat` worker's `vertical-combat-skill-select-unit` passed using a binary newer than its source, and the 1,280-case original-executable coordinator oracle regenerated identically (`e128922`). This covers the selected-skill coordinator with intercepted boundaries, not production hit/death services. The patch remains in the worker tree pending main integration and validation.

The main build for the newer network, inventory and mob changes completed its compilation after its coordinator process was interrupted; the initial combined command did not reach its test phase, so the resulting binaries were tested separately. Four of five tests passed: `vertical-protocol-handoff-tcp-unit`, `vertical-async-entry-tcp-unit`, `vertical-mobs-unit`, and `vertical-mobs-actor-unit`. `vertical-inventory-unit` failed on its drop ownership assertion. The test assumed runtime item ID 1; it now captures the retained item's actual pointer and runtime ID and checks that exact allocation, persistent UID and zone removal. Rebuild and rerun are queued, and the failure is not yet considered resolved.
The inventory fixture's assumed ID is demonstrably wrong: `GroundItemPool` defaults its serial to 1 and increments before assigning the first fresh runtime ID, which is therefore 2. The updated assertion follows the retained allocation instead. Its rerun remains queued.

The `06-inventory` worker's exchange offer removal and money handler (`5be7b81`) matched 1,024 original-executable cases on a fresh oracle run. It is partial exchange routing, not settlement or full trading, and remains in the worker branch pending main integration and native tests.

The corrected `vertical-inventory-unit` passed in the main worktree, confirming the same retained allocation and UID reach and leave the zone. A third shared build slot, limited to one CPU, allowed focused main-worktree validation alongside the two separate worker builds.

Subsequently applied `02-world` `6242336` (serialized phase traversal), `04-mobs` `77ff11d` (live targeting refresh), `05-combat` `e128922` (selected mob skill coordinator), `06-inventory` `5be7b81` (partial exchange offers), and `08-social` commits `5dfe50f`, `dac19e5`, `b54132d` (invitation timeout, social dispatch and release notices). The local retained-item regression fix was preserved while applying the exchange test. Focused build/test for this combined state is running; production wiring and full exchange remain incomplete.

The combined incremental main build passed all six focused tests: `vertical-inventory-unit`, `vertical-world-unit`, `vertical-mobs-unit`, `vertical-mobs-actor-unit`, `vertical-social-replica-unit`, and `vertical-combat-skill-select-unit`. The inventory test includes the corrected retained object assertion and the partial exchange oracle. These tests do not yet exercise a production world session or original client.

Applied `07-quests` commit `775bb0d`: the original `PacketQuest` oracle passed all thirteen cases, and the expanded main-worktree `vertical-quests-unit` passed after rebuilding. It covers dispatch, timed failure, NPC quest progression and selectable rewards with fixture services; live user and NPC service wiring remains required.

Applied `01-entry` `cd50d48`, which composes retained selection, session character, admission and disconnect ownership. Its worker branch passed six focused tests, including cancellation during SQL load; the main registry test is building. This owner still requires concrete world services, an admission success-path test, production wiring and original-client validation.

Applied `08-social` `94ff1e0`, which routes social DB mutations to account-specific retained lifecycles. Four focused tests passed in the worker branch; a main-tree social test is still needed after this integration.
The main `vertical-session-registry-unit` passed after integrating `cd50d48`, including the retained session's in-flight cancellation path. The success path and actual world services remain untested and unbound.
The main `vertical-social-replica-unit` also passed with account-routed social DB mutation integration `94ff1e0`.

Applied `09-network` `6c9e3bc`, which provides a socket IO work lease through asynchronous session drain, and `05-combat` `17e318f`, which reconstructs the skill-hit prefix (256 original oracle cases; full damage continuation remains unimplemented). The network worker passed four focused TCP tests; the combat worker passed 13 focused tests.

Connected the main-tree `VerticalWorldSession` to acquire that IO lease before selection and release it only after disconnect services finish and registry/admission ownership is released. The main regression checks that the lease survives an in-flight SQL cancellation and is released after drain. Main-tree network, session and combat tests are running. A world strand on a separate IO context and the DB executor still need independent lifetime management by the production runtime.

The main build and all four focused tests passed after connecting the session IO lease: `vertical-session-registry-unit`, `serialized-world-application-tcp-unit`, `vertical-async-entry-tcp-unit`, and `vertical-combat-skill-hit-prefix-unit`.

After that pass, integrated `01-entry` `7ba718d` while preserving the IO lease changes: the session now retains `WorldUser` and entry state and has a fixture success path through admission publication. Also applied `03-movement` `bace0ba` (retained movement and visibility composition), `07-quests` `376be6c` (mob-death quest credit), and `04-mobs` `c5dc730` (live idle projection). A combined focused build and test is running. These fixtures still require actual world services and original-client verification.

Main integration tests passed 4/4 after merging retained session entry, movement visibility, quest credit and live mob projection: `vertical-session-registry-unit`, `vertical-scene-unit`, `vertical-quests-unit`, `vertical-mobs-actor-unit`.

Subsequently applied `01-entry` `491b47f` (zone-entry/disconnect owner composition), `02-world` `e17a05c` (authority retained across multiple phases), and `05-combat` `c94e857` (skill damage calculation handoff). Their combined native build and tests are running. The entry fixture uses real ownership structures but test-double services and no SQL save; complete runtime services and original-client validation remain outstanding.

The main incremental build passed `vertical-session-registry-unit`, `vertical-world-unit` and `vertical-combat-skill-hit-damage-unit` after the zone entry/disconnect, world authority and skill damage changes. The session test reaches real native zone membership and leave structures with fixture services, not a live SQL save or original client.

Applied `03-movement` `7291bf3` (priority delivery), `04-mobs` `fa36ad2` (live catalog rereads), and `09-network` `e408767` (TCP shutdown with tracked save drain). The mob original-executable time oracle matched; the worker reported focused tests passed. A main-tree build and focused tests are running.

The main build passed `vertical-scene-unit`, `vertical-mobs-actor-unit` and `vertical-async-disconnect-tcp-unit` for priority movement delivery, live mob rereads and tracked save drain during shutdown. The store used by the shutdown test is a fixture.

Applied `07-quests` `f5da3bf`, adding party mob-quest ownership coverage. Its worker branch passed the focused ten-test suite; the updated main quest test is rebuilding.

The main `vertical-quests-unit` passed after `f5da3bf`, including party mob quest ownership.

Applied `08-social` commits `da1e019` and `b19e653`: the original executable probes passed three character-ID load cases and four online/offline presence cases. The original loads character ID at `CUser+0x128` independently of the object ID at `CUser+0xdc`; `ActorLoadPipeline` currently constructs `Actor::id()` from the character ID. Whether original live allocation aliases these IDs remains unresolved and must be determined before binding social and other entity lookups. The updated main social test is building.
The updated main `vertical-social-replica-unit` passed after integrating the social identity and presence coverage.

Applied `01-entry` `0b461ea` (disconnect acknowledgment routing), preserving the main-tree socket IO lease; `02-world` `111b465` and `7daba77` (primary mob phase and reentrant shutdown checks); `03-movement` `3889bc9` and `4d6298e` (retained interactions and combined movement composition); and `04-mobs` `e87b6d9` (mob world adapter). The prior world test's idle/respawn composition was replaced by the newer world-owned equivalent after verifying it was the only local difference. Focused combined build and tests are running. These owners still require live runtime services and client validation.

The combined main-tree tests for the latest session acknowledgment, primary mob phase, movement interactions and actor targeting passed 4/4: `vertical-world-unit`, `vertical-mobs-actor-unit`, `vertical-scene-unit`, `vertical-session-registry-unit`.

After that, applied `09-network` `06c22df` and `878ed21` (bounded posted output and authentication exception boundary), `05-combat` `27a41ab` (primary skill hit result and drain healing), and `06-inventory` `eb4e9d1` (partial exchange item offers), preserving the local retained-item test. The worker's 1,024 item-offer cases matched the original executable. Main focused builds/tests for these additions are running; exchange settlement and full hit continuation remain incomplete.

`08-social` `9be80a4` resolves the successful-entry identity question: original `PacketUserDBChar` 0046d4c1–0046d4d3 copies the selected character ID (+128) to the entity ID (+dc) before world admission and friend initialization. The main-tree lifecycle oracle passed two allocation, four selection/load/entry and two index-key cases. Equality is established for successful normal in-world entry, not allocation or arbitrary failure paths. The updated social assertion still requires main-tree execution.

The main build passed 5/5 focused tests after the output budget, login exception, item offer and primary skill changes: `login-network-unit`, `world-network-unit`, `world-application-unit`, `vertical-inventory-unit`, `vertical-combat-skill-primary-unit`.

Applied `01-entry` `28f5395` (tracked save and acknowledgment drain), `03-movement` `f5f8925` (retained movement in TCP handoff fixture), `04-mobs` `9392d3c` (retained actor mob frame), and `07-quests` `264956d` and `d66ef14` (quest player/item credit and zone timer pass). Combined main tests are running; fixture stores/services are not production SQL/world services.

Fresh original-executable oracle runs matched the worker fixtures for zero-account world preauthentication (`09-network` `eab5238`), exchange invitation responses (`06-inventory` `0991d31`), and secondary skill damage/effect tail (`05-combat` `70b255f`). These later worker changes await main integration after the current combined build. The worker's `02-world` `a9b9817` also introduces an ASIO frame driver, but still requires the real full update phase provider and production wiring.

The main combined build passed 6/6: `vertical-protocol-handoff-tcp-unit`, `vertical-mobs-frame-unit`, `vertical-quests-unit`, `vertical-async-entry-tcp-unit`, `vertical-session-registry-unit`, and `vertical-social-replica-unit`. These include TCP movement and tracked save-drain fixtures but not a live client or SQL save through the complete owner.

Applied `02-world` `a9b9817` (interruptible frame driver), `09-network` `eab5238` (zero-account preauthentication), `05-combat` `70b255f` (secondary skill hit and effect tail), and `06-inventory` `0991d31` (exchange invitation response). Original-executable oracles for the latter three matched their fixtures; a combined main build and tests are running. The frame driver still needs the real complete world update provider, and combat/exchange still require live owners and settlement.

After resolving normal in-world identity aliasing, resumed `08-social` with its original thread and branch to build a concrete retained per-session social owner from the existing dispatcher, replica lifecycle, invitation state and live lookup/transport dependencies. It remains isolated and must not alter the coordinator worktree.

The main build passed all five focused tests following the world cadence, preauthentication, exchange response and combat tail changes: `world-protocol-unit`, `world-application-unit`, `vertical-inventory-unit`, `vertical-world-unit`, `vertical-combat-skill-tail-unit`.

Applied `01-entry` `f856a05` (owned session save binding) and `03-movement` `4e0639f` (priority queue drain to retained channels). Their combined main-tree tests are running. The save binding still needs an actual retained SQL store bundle and live lifecycle callbacks; queue delivery is not a socket-write acknowledgment.
The main-tree `vertical-session-registry-unit` and `vertical-scene-unit` passed after integrating the owned save binding and retained priority channel drain. Their stores and channel service providers are fixtures; actual SQL bundle and production services remain in progress.

Applied `02-world` `33ba2ed` (common steady clock and shared overrun counter), `03-movement` `76b651c` (priority drain through the TCP fixture), and `07-quests` `e45694e` (NPC interaction dispatch). The original NPC packet oracle completed successfully; worker branches reported focused tests passed. Main-tree combined build/tests are running. The real world phase provider, scheduling of priority drains, and live NPC services remain necessary.

Applied `01-entry` `065d9a8`, retaining the two SQL connections, recovery writer, shared mutex and `CharacterSaveStore` for one session, plus the updated disposable SQL fixture. Its worker branch compiled and passed eight non-SQL tests; the previous main SQL pass predates this bundle. The main SQL fixture must be rebuilt and run against a disposable catalog before claiming it passes.
The main-tree build passed all four focused tests after the clock, TCP priority drain and NPC dispatch changes: `vertical-protocol-handoff-tcp-unit`, `vertical-world-unit`, `vertical-quests-unit`, `vertical-async-entry-tcp-unit`. The updated SQL bundle fixture and session test are now rebuilding.

The main build passed `vertical-scene-unit`, `vertical-inventory-unit`, `vertical-combat-skill-hit-damage-unit` and `vertical-combat-target-unit` after movement reentry, exchange invitation and retained combat targeting integration. The invitation oracle also matched the original executable.

The first isolated SQL rerun of `065d9a8` failed while creating its disposable catalog, before the updated executable ran; the SQL container was stopped. A separate catalog-creation diagnostic is running to capture the actual SQL error and verify system database readiness before retrying. The earlier SQL pass does not validate the new bundle.
A subsequent isolated diagnostic reported `model` and `tempdb` ONLINE and successfully created and dropped a disposable catalog. It did not identify the preceding CREATE failure. The updated SQL fixture was retried with error capture and guaranteed container stop; it is currently running.

The failed SQL rerun's captured error was `Incorrect syntax near the keyword 'NULL'`: `DATABASEPROPERTYEX` returned textual `NULL` even though `sys.databases` reported the source ONLINE, and the previous identifier regex accepted it. `tools/creation_integration.py` now waits for a non-NULL valid collation within the existing recovery deadline. A simulated ONLINE/NULL-to-valid transition passed; the updated actual SQL fixture is running again. SQL container and temporary credentials are cleaned up afterward.

Applied `09-network` `f5a7632`: the decoder delivers complete frames incrementally before inspecting a malformed trailing header, matching the original receive boundary. The worker passed eight focused network tests; the main framing/login/world/TCP tests are building. The corrected isolated SQL fixture remains active and waiting for source readiness.

After fixing the transient NULL collation handling, the updated isolated `vertical-persistence-sql-integration` passed with `VerticalWorldSessionSqlSave` owning the real connections, recovery writer, mutex and store, including selection, mutation, exit save, reload and resource lifetime checks. The disposable catalog was dropped and the SQL container confirmed stopped. This fixture invokes the real store but does not exercise the full live session's tracked-save binding or an original client.

The `08-social` worker's `vertical-social-session-unit` passed independently using a binary newer than its test source (`af69b73`). It exercises retained world/session indexes and social lifecycles with fixture DB transports; it is not installed in the live server. The patch and `01-entry` resource opener plus `02-world` startup guard remain pending integration while the main framing rebuild is active.

Applied `01-entry` `20e6347`, which fences asynchronous per-session SQL resource opening through cancellation and drain. Its worker tests passed with a fixture opener; the main registry test must be rebuilt after the ongoing framing build. Actual SQL bundle opening and session readiness/teardown wiring remain to be exercised together.

Fresh worker-tree original-executable probes matched for seven near-home mob return traces (`04-mobs` `2a7b694`) and 512 non-settlement exchange-control cases (`06-inventory` `fa776b4`). Both worker branches report focused native suites passed. These changes remain unintegrated in main during the framing rebuild; distant mob navigation and exchange settlement remain incomplete.

The `09-network` worker's malformed world-frame oracle (`98be2d1`) reproduced original close reason 13 for all eight cases; its branch reports seven focused native tests passed. The change remains pending main integration until the active framing build completes.

Read-only integration checks confirm the pending world startup/shutdown guard (`02-world` `1887cb5`), malformed-frame close reason (`09-network` `98be2d1`) and near-home mob lifecycle (`04-mobs` `2a7b694`) apply cleanly after the active build. The retained social session (`08-social` `af69b73`) needs its CMake target merged manually because the coordinator's CMake file has advanced; its code and test remain isolated until then.

Copied the new retained social session header and focused test from `08-social` `af69b73` into the main tree. Its CMake target remains to be added after the active framing build; the worker's test passed, but this main-tree copy is not yet built or wired into production.

The main framing rebuild completed and passed all six tests: `framing-unit`, `login-network-unit`, `world-network-unit`, `world-application-unit`, `vertical-protocol-handoff-tcp-unit`, and `vertical-async-disconnect-tcp-unit`.

Applied `02-world` `f1dc942`/`1887cb5`, `04-mobs` `0d2f50a`/`2a7b694`, `05-combat` `bcce3df`, `06-inventory` `fa776b4`, `03-movement` `afff04b`, and `09-network` `98be2d1`. Added the previously copied `08-social` `af69b73` test target to the current CMake file manually. The combined eight-target main build/tests are running. These changes preserve explicit unfinished paths such as distant mob return and exchange settlement.

The `03-movement` worker's status-stand probe (`85b0923`) passed 96 original-executable cases for motion, logout, socket, transfer and recovery, with the shop tail intercepted. Its native fixture passed on the worker branch; main integration and live shop/transport binding remain pending the active build.

The `06-inventory` worker's exchange money/capacity oracle (`572fa2f`) matched 2,048 original-executable checks on a fresh run. This is preflight only; settlement and transfers remain incomplete. The patch remains in the worker branch until the active main build finishes.

The combined main-tree build passed all eight selected tests after the latest world, mob, combat, inventory, movement, network and social changes: `world-application-unit`, `vertical-inventory-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-scene-unit`, `vertical-session-registry-unit`, `vertical-combat-sleep-unit`, and `vertical-social-session-unit`.

After that pass, integrated `01-entry` `b635070` (preparation gate, merged while preserving the main socket IO lease), `02-world` `5f8ce80` (tracked world shutdown), `03-movement` `85b0923` (status stand), `07-quests` `a6d7dff`/`68d008f`/`00ec52b` (NPC lifecycle checks and quest runtime restore), `05-combat` `7b69447` (weapon wear), and `06-inventory` `572fa2f` (exchange capacity checks). The weapon-wear CMake target was added manually to the advanced coordinator file. Combined main tests are running; the live services and complete exchange settlement remain outstanding.

The six focused main-tree tests passed after the session preparation gate, world shutdown coordinator, movement stand, quest restore, exchange capacity and weapon wear changes: `vertical-inventory-unit`, `vertical-world-unit`, `vertical-quests-unit`, `vertical-scene-unit`, `vertical-session-registry-unit`, and `vertical-combat-weapon-wear-unit`.

Applied `01-entry` `1f271f5` (concrete asynchronous SQL preparation composition), `03-movement` `2ea9319`/`9e44322` (bound movement operations), and `08-social` `786e5ee` (ordered tracked social DB mutation jobs). Added the social job test target manually to the advanced main CMake file. Combined tests are running. These remain uninstalled in the production world-service root; the SQL preparation still needs full live-session validation.

The `02-world` terminal had stopped at a dependency report. Resumed its existing thread and branch with a concrete assignment to assemble retained world/admission/session/driver owners and implement real phase and instance lifecycle services incrementally, without synthetic success or changes to the coordinator checkout.

The main build passed all four focused tests after SQL preparation, movement operations and tracked social job integration: `vertical-scene-unit`, `vertical-session-registry-unit`, `vertical-social-session-unit`, and `vertical-social-packet-jobs-unit`.

Applied `01-entry` `c9fcdc8` (DB-worker resource disposal), `09-network` `062f7a8` (reply timing), `07-quests` `de7e45f` (quest entry from retained runtime), `04-mobs` `b8536e5`/`90ad627` (direct return movement), `05-combat` `cdfe006` (combat scheduler composition), and `06-inventory` `a3f021d` (ordered settlement preflight failures). The scheduler target was added manually to the current CMake file. Combined main tests are running. Blocked-path mob navigation, actual exchange settlement and production world service wiring remain incomplete.

The main tree passed 4/4 focused tests for the asynchronous SQL preparation composition, bound movement operations and ordered social mutation jobs: `vertical-scene-unit`, `vertical-session-registry-unit`, `vertical-social-session-unit`, `vertical-social-packet-jobs-unit`.

Applied `01-entry` `7bb9fa3` (fenced worker-side save-binding release), `08-social` `28dd1ea` (social mutations before tracked saves), and `06-inventory` `2c2ec4b` (exchange ready protocol with required real settlement owner). Their main tests are running. The actual settlement implementation is still absent and must not be replaced with synthetic success; real save/transport producer fencing remains a production integration requirement.

The main tree passed `vertical-inventory-unit`, `vertical-session-registry-unit`, and `vertical-social-packet-jobs-unit` after the worker-side session release, social/save ordering and exchange ready changes.

Applied `01-entry` `a8f787e` (tracked save drain plus binding teardown) and `03-movement` `e730847`/`a81c1d7` (retained shop shutdown with shared buffers). Their main tests are building; the fixture store, scene and transport do not prove production SQL or client behavior.

The main `vertical-session-registry-unit` and `vertical-scene-unit` passed after the session save teardown and shared shop-delivery changes. Applied `07-quests` `7c99048`, adding real quest-runtime to replica/save-plan coverage; its main test is rebuilding. This validates a generated SQL plan, not an actual SQL execution of the full world session.

The main `vertical-quests-unit` passed after adding the live quest-to-replica/save-plan test `7c99048`. Applied `04-mobs` `4aca4e8` for following an existing A* cursor on return; its five-case original executable oracle matched in the main tree and the focused native test is running. Path construction for blocked routes remains incomplete.

The main `vertical-quests-unit` and `vertical-mobs-frame-unit` passed for quest save-plan projection and retained A* return movement, respectively. Integrated movement delivery commit `d08f5ce`, which maps shared movement buffers through retained session channels; the focused main-tree scene test is running. These remain component-level tests, not proof of production world entry or client gameplay.
The main `vertical-scene-unit` subsequently passed with the shared-buffer channel adapter. This still uses fixture channels and does not demonstrate multi-client socket delivery.

Integrated `06-inventory` `373d652` and `07-quests` `9183da3`; the combined main-tree `vertical-inventory-unit` and `vertical-quests-unit` passed. Exchange expiry still requires the live timer owner, settlement is unfinished, and quest validation does not substitute for production coordinator wiring.

Integration review found a construction cycle in `WorldRoot`: it owns a private session registry but requires admission services at creation, while the concrete `VerticalWorldSessionAdmissionServices` requires a reference to that registry during construction. A registry-aware admission-service factory or another ownership design is required before these two new components can actually be assembled. The focused combined build is running; tests that instantiate the root with an unrelated fixture service do not detect this integration gap.

The combined main-tree build passed eight focused tests for world protocol/application, world root, mob frame, session admission, social save fence, combat scheduler and combat view. Added a registry-aware admission factory to `WorldRoot` so the admission service can be constructed against the root's own registry; the existing direct-service overload remains available. A root lifecycle test now checks registry identity and is rebuilding. Production phases, transport shutdown and original-client play remain unverified.
The revised `vertical-world-unit` passed and confirms that the admission factory receives the identical registry exposed by the root. This resolves the construction cycle; it does not yet instantiate the complete production session and service graph.

Integrated `03-movement` `8c1a9c1`, `06-inventory` `ab857c9` and `07-quests` `dae068b`. The main `vertical-scene-unit`, `vertical-inventory-unit` and `vertical-quests-unit` passed together. These cover retained cell-transition buffers, byte-level exchange money-log builders, and binding quest reward removal to its runtime; actual exchange settlement, live reward services and socket/client validation remain open.
Integrated `08-social` `428489d`; the main `vertical-social-packet-jobs-unit` passed after retained lifetime changes for social/save drain. This remains a component fixture and must be installed into live disconnect coordination.

Integrated `03-movement` `204a6ac` and `05-combat` `b8e0cf6`; main-tree `vertical-scene-unit` and `vertical-combat-scheduler-unit` passed. The tests cover retained duel participants and skill eligibility with equipment owners. Duel transport, actual range/death/effect providers and production scheduling remain unverified.
Integrated `04-mobs` `d5488e0` and `09-network` `9660f4a`; the main mob-frame, world-protocol and world-application tests passed 3/3. The reconstructed mob path includes blocked-line A* search, and network timeout includes the original zero baseline for a first reply. Live mob AI wiring and the original reset path's nonzero baseline writers remain open.

Integrated `01-entry` `c2a3152`, `02-world` `3c6aa2f`, `03-movement` `134acee`, `06-inventory` `7e495d7`, `07-quests` `973ebf8`, and `08-social` `ce53fba`. Resolved integration conflicts while preserving the root's registry-aware admission factory and the transport IO lease through deferred admission-pass teardown. Six main tests passed together: inventory, world, quests, scene, session registry and social packet jobs. The world now has a bound primary mob/trap phase, but its test uses phase fixtures; full production world scheduling, exchange item settlement, real reward service installation, SQL session and original-client validation remain incomplete. Reassigned social session to integrate social save fencing into live disconnect ownership.

Integrated `01-entry` `eb4449c`, `04-mobs` `e7790e7`, `05-combat` `c230f14`, and `09-network` `ec272b6`. Main-tree world-application, mob-frame, session-registry and combat-scheduler tests passed 4/4. The retained packet FIFO still requires installation in the live scheduler, and the mob reset, combat weapon and reply-scan changes remain component-level validation rather than an original-client gameplay test.
Integrated `02-world` `98eb63e`, `06-inventory` `50efd8f`, and `07-quests` `ed3a060`; retained the root's registry-aware admission factory while merging the primary user attack phase. Main world, inventory and quest tests passed 3/3. The world attack phase still needs real providers and production scheduling, item-transfer packets are not settlement, and quest experience is not a full original-client play test.

Integrated `01-entry` `c524b9d` and `02-world` `e5d6091`, preserving the shared-registry factory during the latter merge. The main world and session-registry tests passed 2/2 for queued movement dispatch and the primary ground-item expiry phase. The session movement binding still needs reconciliation with the newer retained-view movement owner; the phase and transport providers are fixtures, and full live world, instance-zone and original-client behavior remain unverified.

Integrated `06-inventory` `8f87121`; main `vertical-inventory-unit` passed for reconstructed database transfer packet bytes. Actual item transfer and SQL execution are still pending. The desktop dashboard now displays only the twelve most recent completed entries while retaining its full history in state, keeping the requested overview lightweight.
Integrated `04-mobs` `36afc4a`, `05-combat` `82235ed`, and `09-network` `30d6d7e`. Main mob-frame, combat-range, world-network and world-protocol tests passed 4/4. Original-executable comparisons support range and duplicate-account behavior; the live frame, mob position/damage/death and account-save fence remain to be installed and original-client play remains unverified.

Integrated `01-entry` `87d3f70`, `03-movement` `e6973cf` and `07-quests` `e6b39ea`; main session-registry, scene and quests tests passed 3/3. The session gameplay factory can now retain a movement owner and the movement branch supplies a composed owner, resolving the earlier incompatible interface at the API level. A production instance still needs to construct both together with real services; these separate fixture tests do not establish a live connection or end-to-end movement.
The main world fixture now explicitly returns success when included as a helper function, avoiding undefined return behavior; `vertical-world-unit` passed. All nine assigned Codex processes were confirmed live while their current blocks continue.

Integrated `01-entry` `52ee868`; main `vertical-session-registry-unit` passed for the per-session packet router, FIFO, lazy gameplay owner and teardown cases. Runtime service installation and a real original-client connection remain unverified. The nine worker process handles were checked live during this round.

Integrated `04-mobs` `f5d7f9b`, `05-combat` `bb33271`, `06-inventory` `0dd1901`, and `07-quests` `414554f`. The main inventory, mob-frame, quests, combat-range and mob-position tests passed 5/5. The changes include retained mob-return state, reconstructed movement position, item-transfer log packets and player-kill quest progress. Production frame/damage services, actual item settlement, live quest event wiring and original-client play remain open.

Integrated `02-world` `0ef332a` and the social save-binding changes from `08-social` `816d7ee`, preserving the main session, packet-router, shared-registry factory, and IO lease changes. Added the missing `owns_social` compatibility check from the worker branch. The main world, session-registry, social-live-session and social-packet-jobs tests passed 4/4. The social live-session fixture exercises a retained session's actual save binding and drain ordering with a test database and store, not SQL Server or production construction. Its copied fixture remains separate to preserve the newer movement/router tests; it should be consolidated when the shared fixture is refactored. Quest timing still uses fixture services and the full world loop remains unwired.

Integrated `03-movement` `43fdcb4`, `04-mobs` `a68ec66`, `05-combat` `33276e8`, `06-inventory` `5c7aa98`, and `08-social` `f923d78` selectively; the SQL save bundle now exposes the social binding and the social fixture compiles that path, without connecting SQL. Resolved the handoff test against the main shared-buffer implementation. The first combined build exposed a missing quest library link in `vertical-mobs-frame-unit` after its included world fixture gained quest timing; fixed the link and reran. Five main tests passed, including two local TCP channels exercising visibility, mob frame, combat hit, inventory and social teardown. The two endpoints are test clients, not the original client; actual item settlement and production world service construction remain open.

Integrated `01-entry` `9544585`, `02-world` `27de28f`, `03-movement` `d38db3d`, `07-quests` `ac273fb`/`2320df0`, and `09-network` `0d42791`, retaining the root registry-aware admission factory. Main world, session, quests, handoff TCP and serialized world application TCP tests passed 5/5. The session service composition still requires all real providers and is not installed in `--serve`; weather and network lookups use fixtures in these tests. Original-client gameplay remains unverified.

Integrated `01-entry` `68d80ee`/`8ed870d`, `02-world` `77c34e3`, `04-mobs` `444c8dc`, `05-combat` `620dea3`, `06-inventory` `585bc15`, `07-quests` `62eac14`, and the social preparation portion of `08-social` `6db6ab8`. The first combined build exposed older cancellation calls in the separate social fixture; updated it to the new failure-aware signature. The rerun passed all eight selected protocol, inventory, world, mob-frame, quests, retained-session, social-session and combat-damage tests. This includes real reconstructed owners with test providers; `--serve` still lacks production assembly, actual settlement and original-client validation.

Integrated `01-entry` `2f0df7a`, `03-movement` `d30962d`, `04-mobs` `d025ef6`, `06-inventory` `2df5e49`, `07-quests` `50b0e2a`, `08-social` `34dd81a`, and `09-network` `64b4800`. Main handoff TCP, protocol, inventory, mob frame, quests, serialized TCP application, session registry and social live-session tests passed 8/8. This includes a transport close-completion callback, shared SQL/social mutex, and closing during a two-channel fixture; production construction and a real SQL social save remain unverified. The exchange split-failure milestone is an oracle/test, not implemented settlement.

Integrated `01-entry` `190a2c6`, `03-movement` `8f6d62a`, `05-combat` `199791f`, `07-quests` `9f5b842`, and `09-network` `8d9a235`. Main protocol, quests, scene, serialized TCP application and combat-death tests passed 5/5. Transport shutdown now reports application close errors and combat death reaches the reconstructed coordinator under fixture conditions; production provider assembly and original-client play remain unverified.

Integrated `04-mobs` `491ca4c`, `05-combat` `3b35499` and `09-network` `16663dc`. Reconciled `08-social` `2127efe` with the independently integrated entry branch: the already-selected catalog is transferred exactly once to the retained social owner and rebound for entry delivery; the SQL-social preparation requires the same retained account mutex. A main retained-session fixture verifies catalog identity and shared mutex through social preparation and drain. Main mob-frame, serialized TCP, session-registry, social-live-session and combat death/effects tests passed 5/5. SQL execution and actual runtime construction remain outstanding.

Integrated `01-entry` `c77ff69`/`4f05417`, `03-movement` `8d88892`, `04-mobs` `54b1ddb`, `05-combat` `bd50916`, `06-inventory` `b261d3f`, `07-quests` `2605148`/`0c3c3f5`, and `09-network` `8e919e8`, retaining the main socket IO lease and bounded-input test. Main login-network, handoff TCP, inventory, mob-frame, quests, session registry and combat death/effects tests passed 7/7. The exchange split step still is not complete settlement; production world/SQL assembly and original-client play remain open.

Integrated `02-world` `bdfaa02`, `03-movement` `cdc2f20`, and `07-quests` `fe48533`. Main world, handoff TCP and quests tests passed 3/3. NPC idle uses reconstructed zone owners under fixture services, the two-channel test retains the observer independently, and quest save composition still needs live installation. `08-social` `a6de736` was reviewed but remains pending reconciliation: it requires a social binding during runtime preparation and changes the packet router, while the entry branch's existing fixture and retained catalog handoff have a different ownership path. Do not replace either branch wholesale or claim the runtime integration is complete.

Integrated `01-entry` `3e3ef02`, `02-world` `bd58901`, `03-movement` `b0dfbc3`, `04-mobs` `6ae1c08`, `05-combat` `76087b2`, `06-inventory` `f134446`, `07-quests` `d214a4e`, and `09-network` `6909ff8`. The main handoff TCP, inventory, world, mob-frame, quests, serialized TCP, session-registry and combat-death tests passed 8/8. These include the ordered primary frame prefix and retained split-item replica delivery under fixture services; complete exchange settlement, remaining world phases, production assembly, real SQL and original-client gameplay remain open. `08-social` `7318a84` still needs reconciliation with the runtime-owned account registry and the distinct catalog-handoff design in main.

Integrated `01-entry` `ffc62d3`, `02-world` `a3c326e`, `05-combat` `45a19aa`, `06-inventory` `2ec4a2d`, `07-quests` `99e3900`, and `09-network` `ecd4285`. Main inventory, world, quests, serialized TCP, session-registry and combat-death tests passed 6/6. The ordered exchange item loop and zone packet phase still require real session installation and complete settlement. `08-social` through `b4ba4e3` remains pending reconciliation with the main runtime-owned account registry and catalog handoff; do not overwrite these owners with the worker's older copies.

Social runtime integration note: the main `VerticalWorldSessionRuntime` now owns `VerticalWorldSocialAccounts`, uses `Experience` and hands its selected catalog off through `take_catalog`/`bind_catalog`. The social worker's `b4ba4e3` runtime assumes an older externally supplied account and shared catalog; its mandatory fenced social preparation must be merged while retaining these main facilities, and the existing runtime lifecycle fixture must be adapted to construct the real social owner instead of removing the requirement. The main `VerticalSocialWorldOwner::create_for_selection` already bridges the chosen handoff. Avoid inventing success/no-op providers.

Integrated `01-entry` `60f728e`, `03-movement` `729bb0f`, `04-mobs` `b102cc1`, `05-combat` `b5e917f`, `06-inventory` `609e3c0`, and `09-network` `966944f`. Main inventory, mob-frame, scene, serialized TCP, session-registry and combat-death tests passed 6/6. The movement gameplay binding uses the retained owner in component fixtures; live world construction and complete exchange settlement remain outstanding. The social runtime integration needs to preserve the main runtime-owned account registry, loaded Experience table and take/bind catalog ownership while adding the worker's mandatory fenced social preparation.

Integrated `01-entry` `f0f995b`, `02-world` `2978ec7`, `04-mobs` `980d856`, `05-combat` `1f6f238`, `06-inventory` `0ec091a`, `07-quests` `f3b6c83`, and `09-network` `b9f513b`, preserving the main socket IO lease. Main inventory, world, mob-frame, quests, serialized TCP, session-registry, combat-damage and combat-death tests passed 8/8. This reconstructs additional effect expiration, guild membership and quest restoration behavior with fixture services; actual production assembly and original-client play remain incomplete. The bag-boundary change is oracle evidence, not full exchange settlement.

Integrated `01-entry` `1900819`, `02-world` `c63064b`, `03-movement` `06cff40`, `07-quests` `4c910d7`, `09-network` `a4c5f0b`, and the standalone party-finder portion of `08-social` `af0dc9d`. Main system TCP, world, quests, scene, session-registry and social-session tests passed 6/6. Party-finder routing into the actual session runtime and the rest of the `08-social` runtime integration remain pending; production world assembly and original-client play are still unverified.

Integrated `01-entry` `e90c9a5`/`13dccde`, `02-world` `ec1fc25`, `04-mobs` `8bde8b8`, `05-combat` `84ced2e`, `06-inventory` `b2e6526`, `07-quests` `0a759d9`, and `09-network` `07ac3fb`. Main login-network, inventory, world, mob-frame, quests, session-registry and combat-retarget tests passed 7/7. Resumed `03-movement` with an expanded assignment to build actual retained per-session resources and lifecycle in its own worktree. The social runtime still requires reconciliation and installation; original-client gameplay remains unverified.

Integrated `02-world` `d19420b`, `03-movement` `190e123`/`49a4ead`, `06-inventory` `f84ad5f`, `07-quests` `c48e7f7`, and `09-network` `43ec432`. Main login-network, handoff TCP, inventory, world and quests tests passed 5/5. These cover retained priority resources, an exchange settlement phase composition and live quest owners with test providers. Full real exchange ownership/SQL persistence, runtime installation and original-client gameplay remain unverified.

Integrated `01-entry` `c98515e`, `04-mobs` `b22c40c`, `05-combat` `29d282f`, and `07-quests` `927964c`. Main mob-frame, quests, session-registry and stacked-aggro tests passed 4/4. Social and guild release are composed under fixture ownership; the social runtime route and full production assembly still require integration, and original-client gameplay remains unverified.

Integrated `03-movement` `9272dc9`, `05-combat` `6077ea9`, `06-inventory` `cb4a73a`, and `09-network` `46cd638`. Main handoff TCP, combat aggro, inventory, world protocol and serialized TCP application tests passed 5/5. The directory retains movement resources by registered session and the exchange phase now composes both directions, but live runtime installation and real SQL settlement remain open. The social runtime branch still needs reconciliation with main's guild, experience and account owners.

Integrated `01-entry` `e42f04b`, `02-world` `ae9d4e1`, `03-movement` `b67a263`, `04-mobs` `89f04fc`, `05-combat` `390bba3`, `06-inventory` `7610110`, `07-quests` `2cac593`, and `09-network` `d452a35`. Main zones, inventory, mob-frame, quests, scene, serialized TCP, session-registry and combat-log tests passed 8/8. The instance retirement and session directory remain to be installed in production. Social runtime reconciliation is still pending: it must preserve the main registry, guild and experience changes and use the catalog handoff rather than older branch copies.

Integrated `01-entry` `51efb24`, `03-movement` `3f873c4`, `05-combat` `8c6abe8`, and `09-network` `d3a66cd`. Main login-network, scene, session-registry and combat-log tests passed 4/4. The social worker is testing reconciliation of its catalog handoff against a newer session snapshot; do not apply its earlier runtime wholesale over main's guild/experience/account ownership. Original-client gameplay remains unverified.

Integrated the compatible social selection/request-state handoff and oversized-memo behavior from `08-social` `0dc78be`/`9f3ae15`. The social owner now aliases the retained session's actual `zone_requests` for both social processing and zone leave. Updated the separate social session fixture to use the session-owned zone entry after `01-entry` changed admission ownership. Main social live-session and replica tests passed 2/2. The complete social packet runtime from this branch still needs integration with the main guild/experience/account runtime; the fixture is not a real SQL or original-client test.

Integrated `01-entry` `d77d7df`, `02-world` `e70457a`, `03-movement` `a898609`, `04-mobs` `00f1e81`, `06-inventory` `20f04d3`, `07-quests` `881a3ff`/`ef91fed`, and `09-network` `e9f4fdf`, preserving the root registry-aware factory. Main zones, login-network, inventory, world, mob-frame, quests, scene and session-registry tests passed 8/8. These prove component behavior with fixture services, not full production world entry, real exchange settlement or original-client gameplay.

Integrated `01-entry` `21736cb`, `04-mobs` `4c23a64`, `05-combat` `f3fcec4`, `06-inventory` `408e826`, `07-quests` `55feaf5`, and `09-network` `8f07d3e`. Preserved the local dropped-item identity fix. Main login, inventory, mob-frame, quests, session-registry and two combat scheduler tests passed 7/7. The SQL selection composition is exercised through fixtures, not a full live SQL session; actual world service construction and original-client play remain open.

Integrated `01-entry` `671142b`, `03-movement` `bfce6b0`, `06-inventory` `f07a180`, and the standalone guild-etin query from `08-social` `8e13658`. Main guild-catalog, handoff TCP, inventory and session-registry tests passed 4/4. The query has not been wired to the main runtime; a failed completion post deliberately retains the account fence but cannot signal progress without a working executor. Full production assembly and original-client play remain unverified.

Integrated `01-entry` `5e3cf34`, `03-movement` `d6869d9`, `04-mobs` `b512c28`, `05-combat` `ce13222`, `07-quests` `a188418`, and `09-network` `e047b2e`, retaining the main IO lease. Adapted the separate social fixture to the per-session scene interface. Main login, inventory, mob-frame, quests, scene, session-registry, social live-session and combat-scheduler tests passed 8/8. Real actor scene providers, social runtime assembly, SQL session and original-client validation remain outstanding.

Integrated `02-world` `ee017c0`, `05-combat` `990a96e`, and `06-inventory` `a43325e`. Main inventory, world and combat-scheduler tests passed 3/3 for dynamic-zone leases, retained equipment polling and inventory delivery to retained replicas. The live zone lifecycle, production owner construction and original-client gameplay remain unverified. Resumed `09-network` with an expanded assignment for authenticated heartbeat events and atomic account/shutdown ownership.

Integrated `01-entry` `bd03a88`, `02-world` `863d349`, `03-movement` `e82027d`, `04-mobs` `2d87d9d`, `05-combat` `e145b59`, `07-quests` `6862c6c`, and the standalone guild remark portion of `08-social` `c285c43`. Preserved the socket IO lease and adapted the separate social fixture to the new actor scene bind/unbind interface. Main guild-catalog, world, mob-frame, quests, scene, session-registry, social live-session and combat-scheduler tests passed 8/8. Actual scene providers, runtime guild/social routing, SQL and original-client gameplay remain unverified.

Integrated `03-movement` `ce4d466`, `04-mobs` `67da829`, `05-combat` `11f77fd`, `06-inventory` `6f9180a`, `07-quests` `2b049ff`, and `09-network` `2828bd3`/`95fd022`. Main login, inventory, mob-frame, quests, scene, asynchronous disconnect TCP and combat-scheduler tests passed 7/7. The login connection now owns decoded heartbeat timing in these tests. Production gameplay assembly, real session SQL and original-client behavior remain unverified.

Integrated `01-entry` `39f3d44`, `02-world` `ac3c138`, `03-movement` `7ffc739`, `05-combat` `e49df0c`, `07-quests` `37ea471`, and the standalone ignored-guild-opcode handler from `08-social` `ac7bc21`. Main guild-catalog, world, quests, scene, session-registry and combat-scheduler tests passed 6/6. Session-linked scene zone resolution and dynamic zone leases use fixture services here; runtime guild/social routing, production construction and original-client gameplay remain incomplete.

Integrated `01-entry` `a4824e0`, `02-world` `920483a`, `04-mobs` `f7e45a6`, `05-combat` `44bee0d`, `06-inventory` `67a24d6`, `07-quests` `c6fac02`, and `09-network` `3ba016d`, preserving the main IO lease and root factory. Main session, inventory, world, mob-frame, quests, serialized TCP, retained-session and combat-scheduler tests passed 8/8. The retained account claim fence is validated in fixtures; installation into the full production world and SQL/original-client testing remain open.

Integrated `01-entry` `eb350bc`, `02-world` `3a9f400`, `03-movement` `1ae2a67` probe, `07-quests` `a04f71e`, and `09-network` `e6ed83b`. Main session, world, quests, serialized TCP and session-registry tests passed 5/5. The movement probe was validated by its worker against a copied coordinator fixture; it does not show admitted movement or production wiring. Failed world application cleanup now retains a reservation for explicit later recovery, but the production coordinator must implement that recovery and drain.

Integrated `04-mobs` `274e202`, `05-combat` `ada08d9`, `06-inventory` `41ac5ab`, `07-quests` `5b6a89b`, and the standalone guild-rank handler from `08-social` `167cabe`. Main guild-catalog, inventory, mob-frame, quests and combat-scheduler tests passed 5/5. Guild runtime routing, production ownership and original-client gameplay remain outstanding.

### Integración: solicitudes de grupo, habilidades, rangos y reserva de mundo

Se integraron 01 `06f5df0` (solicitudes de grupo con sesión retenida), 05 `0b9e586` (habilidades dirigidas a vistas retenidas), 08 `f176cc0` (persistencia de rango de miembro), y de 09 `c077f4b` únicamente la transferencia de reserva a la sesión y su prueba TCP, sin reemplazar las adaptaciones locales de vida útil. Las sesiones sin reserva de red conservan la creación interna previa; cuando existe una reserva se transfiere al registro de mundo.

Verificado en compilación y CTest: `guild-save-unit`, `vertical-combat-scheduler-unit`, `vertical-combat-view-unit`, `serialized-world-application-tcp-unit` y `vertical-session-registry-unit`. Esta última prueba detectó inicialmente que exigir reserva a fixtures internos impedía crear la sesión; se corrigió y ambas pruebas de sesión/TCP pasaron al repetirlas. La prueba de rango contra SQL aislado aún no se ejecutó en esta integración. Estas piezas no demuestran un cliente jugable completo ni la conexión final de todos los servicios al ejecutable.

### Integración de fases y validación de identidad de red

Integrados 02 `192d801` (efectos en zonas activas), 04 `d12fa8d` (retorno de persecución de monstruos), 05 `3f4d27b` (golpe de habilidad contra objetivo retenido), 07 `51c7a0a` (vida útil de sesión durante misiones) y 09 `7fa9d72` (reserva ligada a cuenta, reino, sesión y conexión). Se endureció `VerticalWorldSession::create` para exigir la reserva exacta; los fixtures internos ahora construyen reservas reales mediante el registro de login, en lugar de omitir la validación. Se preservaron las adaptaciones locales y se añadió manualmente el nuevo oráculo de monstruos a CMake.

Validado con compilación y ocho pruebas: `session-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-quests-unit`, `serialized-world-application-tcp-unit`, `vertical-session-registry-unit`, `vertical-social-live-session-unit` y `vertical-combat-scheduler-unit`. El cambio social 08 `1b02660` requiere reconciliación manual con el runtime más reciente, por lo que aún no está integrado. Falta validar SQL aislado y el flujo completo del cliente.

### Retención social y SQL de gremios

Se reconcilió parcialmente 08 `1b02660` con el árbol principal: las búsquedas sociales devuelven una referencia compartida al propietario completo, y el contexto retiene emisor y destinatarios durante callbacks reentrantes; la liberación de amigos también mantiene el propietario vivo. La prueba de reentrada elimina temporalmente el propietario visible durante el envío y comprueba que siga vivo. Pasaron `vertical-social-session-unit`, `vertical-social-packet-jobs-unit` y `vertical-social-live-session-unit`. La integración completa del runtime social y su enrutamiento al servidor principal aún está pendiente.

Se compiló `guild-sql-integration` y se ejecutó `tools/guild_integration.py` contra el SQL Server aislado y un catálogo temporal `Eternity_Test_Guild_*`: pasó, incluida la actualización y lectura de rango de miembro por el procedimiento original. El fixture eliminó el catálogo temporal y se apagó el contenedor SQL. La primera tentativa encontró el catálogo original aún en recuperación; la segunda tuvo credenciales del contenedor de prueba desactualizadas. La ejecución exitosa esperó acceso real al catálogo y empleó el secreto del contenedor SQL mediante archivo temporal de permisos restrictivos, ya eliminado. Los catálogos originales no se modificaron.

La terminal 07 se reanudó en el mismo hilo con asignación ampliada para integrar propietarios de misiones y fase de mundo en su worktree aislado.

### Integración de fases activas y cierre ordenado

Se integraron 01 `b92b028` (solicitudes y liberación de grupo), 02 `85b0b9d` (ataques y NPC inactivos en zonas activas), 03 `47cc9ae` (sonda de movimiento), 04 `e6e3a43` (selección de camino de persecución), 05 `7947628` (daño propio de habilidad y detención de mob), 06 `79ee558` (validación de propietarios de intercambio) y 08 `c10a757` (entrega de listas sociales reentrante). De 09 `c35a7b5` se integraron el drenaje de registro/trabajos y sus pruebas TCP y de grupo, preservando el fixture de registro principal; su prueba adicional de registro vacío todavía no fue portada. El nuevo oráculo de persecución se añadió manualmente a CMake.

Compilación y nueve pruebas aprobadas: `entry-catalogs-unit`, `vertical-inventory-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `serialized-world-application-tcp-unit`, `vertical-session-registry-unit`, `vertical-db-drain-group-unit`, `vertical-social-session-unit` y `vertical-combat-scheduler-unit`. El cierre probado cubre los propietarios y trabajos de los fixtures, no demuestra todavía la integración completa del proceso servidor ni juego con cliente original. La terminal 07 había quedado sin proceso tras su relanzamiento inicial; se reinició el mismo hilo en una terminal separada y se comprobó que el proceso estaba activo.

### Fases activas y controles de intercambio

Se integraron 01 `5bcdf6a` (propietario de registro y candados de grupo), 02 `89c5d97` (temporizadores de objetos y misiones en zonas activas), 04 `8c20588` (recuperación de persecución bloqueada), 05 `465f634` (detención de monstruos con objetivo y FSM retenidos) y 06 `fd5f004` (comprobación de liquidación antes de notificaciones). El oráculo nuevo se añadió manualmente sin reemplazar cambios locales de CMake. Compilación y cinco pruebas aprobadas: `vertical-inventory-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-session-registry-unit` y `vertical-combat-scheduler-unit`.

La rama 08 contiene una implementación de enrutamiento social sobre un runtime anterior; requiere reconciliación manual con el runtime principal, que conserva catálogos de gremio, experiencia, estado por cuenta y barreras de desconexión más recientes. Aún no se considera conectada al proceso servidor final.

### Unión parcial del propietario social al runtime principal

Se añadió al router de paquetes una ruta de propietarios retenidos antes del despachador general. El runtime puede instalar durante la preparación un `VerticalSocialWorldOwner` ligado al catálogo y mutex de la sesión, comprobar que la preparación haya instalado el mismo propietario en la barrera de guardado, conservar el bundle y despachar paquetes `0x22`; la entrega inicial puede usar sus puertos sociales. Se preservaron el catálogo de gremios, experiencia, índices de mundo y barreras de desconexión del runtime principal. Los casos sin propietario instalado todavía se permiten durante la preparación, por compatibilidad de los escenarios internos; cualquier paquete social en ese estado se rechaza. Falta conectar la preparación de producción real, hacer obligatoria su instalación en el camino de producción y probar el recorrido integrado con SQL y cliente.

`vertical-session-registry-unit` pasó con un caso que verifica que el router entrega el paquete al propietario antes del gameplay; `vertical-social-live-session-unit` pasó y compila la instalación concreta de un propietario social. Estas pruebas no ejecutan todavía el recorrido completo de instalación y despacho desde el runtime: no se debe interpretar como validación de juego social de extremo a extremo.

### Identidad del registro de red y actualizaciones de mundo

Se integraron 01 `da991bd` (envíos de miembros de grupo), 02 `f88f961` (clima y orden de fases activas), 03 `16625f2` (sonda de movimiento y salida), 04 `eb993ae` (búsqueda de persecución), 05 `1d4d580` (opciones de arma contra monstruos), 06 `6b74c9d` (prevalidación de destino de intercambio) y 09 `60182e2` (la reserva debe pertenecer al registro de red emisor). El nuevo oráculo de monstruos se añadió manualmente a CMake. Los fixtures de sesión ahora comparten con el mundo el registro que emite sus reservas; el servidor integrado deberá vincular de igual modo su registro real antes de aceptar sesiones.

Compilación y ocho pruebas aprobadas: `session-unit`, `vertical-inventory-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `serialized-world-application-tcp-unit`, `vertical-session-registry-unit`, `vertical-social-live-session-unit` y `vertical-combat-scheduler-unit`. La unión social parcial sigue sin prueba de recorrido completo y el ejecutable no demuestra aún juego con cliente original. La funcionalidad de protección de liberación social de 08 `40b42eb` ya existe en el runtime principal con controles adicionales de gremio y no se aplicó como parche bruto.

### Adopción exclusiva de sesiones y otros bloques

Se integraron 01 `8d8c0bb` (almacenamiento de buffers de grupo), 02 `0517684` (fases activas simultáneas), 03 `757410f` (sonda de movimiento entre celdas), 04 `2d3ac19` (reinicio de objetivo de persecución), 05 `9d5cb9f` (opciones de arma ligadas a habilidad), 06 `47d359c` (validación previa de destino de intercambio) y 09 `13b376a` (adopción exclusiva de reserva y recuperación de propietario abandonado). Se añadió el oráculo nuevo de mobs a CMake y la dependencia pública de `eternity_game` sobre `eternity_session`, necesaria porque el registro de mundo ahora invoca el registro de red. La prueba portada usa la firma del commit integrado aunque la rama de origen ya había avanzado.

Ocho pruebas aprobadas: `session-unit`, `vertical-inventory-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `serialized-world-application-tcp-unit`, `vertical-session-registry-unit`, `vertical-social-live-session-unit` y `vertical-combat-scheduler-unit`. La prueba de registro incluye ocho intentos concurrentes de adopción y recuperación de una adopción abandonada. La unión de producción y la prueba completa con cliente continúan pendientes.

### Vínculo al reino y cierre de instancias

Se integraron 01 `9ac9375` (composición de propietarios de grupo), 02 `a7b935f` (drenaje de instancias pendientes durante cierre), 05 `d771e31` (comparación de opciones de arma con oráculo original) y 09 `875671b` (registro de mundo ligado al reino configurado). Los fixtures locales de sesiones se adaptaron al reino 0 del registro que emite sus tokens. La prueba verifica rechazo de un token de otro reino sin consumirlo y aceptación por su reino correcto.

Compilación y seis pruebas aprobadas: `vertical-world-unit`, `vertical-mobs-frame-unit`, `serialized-world-application-tcp-unit`, `vertical-session-registry-unit`, `vertical-social-live-session-unit` y `vertical-combat-item-option-unit`. Estas pruebas no sustituyen la unión del servidor de producción ni una sesión jugable con el cliente original.

### Cierre de mundo, persecución y gremios

Se integraron 02 `bfda49b` (mantener vivo el mundo durante reintento de retiro de instancia), 03 `f1db440` (sonda de lotes de movimiento), 04 `b99e566` (puntos intermedios de persecución) y 05 `4ea1110` (penalización de distancia ligada a pasivos). De 08 `cd5856f` se integraron el manejador aislado de mejora de NPC de gremio y su prueba de catálogo; su enlace al runtime y a los puertos reales de configuración/DB/log sigue pendiente. Se añadió manualmente el nuevo oráculo de persecución en CMake.

Compilación y seis pruebas aprobadas: `guild-catalog-unit`, `vertical-combat-damage-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-combat-skill-damage-unit` y `vertical-combat-scheduler-unit`. Se reanudó el hilo de la terminal 06 con asignación ampliada para unir inventario con sesión, actor, zona y persistencia en su worktree aislado. `src/main.cpp --serve` continúa limitado a listado y creación de personajes; los propietarios de mundo todavía no están conectados al arranque real.

### Vida útil de sesión durante desconexión

Integrados 01 `28ddfcf` (barrera de salida de grupo y estado de socket), 02 `34de388` (sondeo de cierre real del mundo e instancias) y 09 `e901faf` (retener la reserva de red hasta liberar la última referencia de E/S). Pasaron `vertical-world-unit`, `serialized-world-application-tcp-unit` y `vertical-session-registry-unit`, incluyendo el orden de liberación de E/S en la prueba TCP.

La rama de misiones 07 `e6841d6` presenta un adaptador de propietario de misiones y copias de numerosos archivos compartidos; no se aplicó en bloque para evitar sustituir versiones más recientes. La propia rama documenta que aún le falta una prueba de instalación y limpieza con sesión real, y continúa trabajando en ella. El servidor `--serve` sigue sin conexión al mundo completo.

### Propietario de misiones asociado a la sesión

Se reconciliaron selectivamente los cambios específicos de 07 `e6841d6` y `df64116`, sin sustituir los encabezados compartidos de red y sesión más recientes. `VerticalWorldSession` puede instalar durante la preparación un propietario de misiones del mismo Actor y entregar referencias retenidas; el Actor mantiene vivos su personaje y escena mediante un propietario separado, evitando una referencia circular hacia la sesión. Se añadió el resolutor por Actor y el proveedor de fase de misiones, y la fase comprueba la identidad del Actor al actualizar temporizadores. La biblioteca de mundo enlaza ahora la implementación de misiones requerida.

Compilación y cinco pruebas aprobadas: `vertical-quests-unit`, `vertical-session-registry-unit`, `vertical-world-unit`, `serialized-world-application-tcp-unit` y `vertical-social-live-session-unit`. La prueba de misiones cubre vida útil del Actor/carácter/escena y rechazo de Actor no registrado; todavía falta probar la instalación exitosa en una sesión preparada, el tick y la limpieza de misiones con servicios reales. Aún hay que construir esos servicios en la preparación de producción, conectar sus paquetes y unir el mundo a `--serve`.

### Estado real del actor y cierre de login

Integrados 02 `54f48ce` (sondeo del cierre del mundo retenido), 03 `2b36590` (sonda con buffers compartidos), 04 `202c3a1` (puntos finales de persecución), 05 `97dbf8c` (daño de habilidades con recursos actuales del Actor), 08 `3d3f951` (lectura de configuración de NPC de gremio) y 09 `fa85ce8` (carreras de heartbeat y cierre de login). De 01 `78ef550` se integró únicamente el adaptador de composición de salida de grupo, gremio y social; su fixture combinado todavía requiere reconciliación con el runtime principal y no se afirma validación integral de ese adaptador. El nuevo oráculo de persecución se añadió manualmente en CMake.

Compilación y siete pruebas aprobadas: `login-network-unit`, `guild-catalog-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-combat-scheduler-unit`, `vertical-combat-skill-damage-unit` y `vertical-combat-skill-hit-damage-unit`. La conexión del mundo completo al ejecutable y la prueba con cliente original siguen pendientes.

### Preparación real de misiones y propietario de sesiones

Se integró 07 `586f40d`: una prueba instala el propietario de misiones durante la preparación de una sesión real, rechaza una referencia circular y una instalación duplicada, verifica identidad exacta del Actor y la referencia retenida durante el cierre, y comprueba que se liberen sesión y E/S sin ciclo. El fixture se adaptó al vínculo de reino del registro de red del coordinador. `vertical-quests-unit` pasó; aún se usan servicios de prueba y no demuestra comportamiento de cliente o SQL en producción.

Se integró selectivamente el propietario compuesto de 01 `c6b25aa`, que reúne el registro de sesiones, runtime, admisión y fábrica de selección. Se reforzó su constructor para exigir explícitamente el registro de red emisor y reino, evitando un propietario que aceptaría conexiones sin poder adoptar sus reservas. La prueba portada usa una reserva real y verifica cancelación de selección, drenaje del trabajo y rechazo posterior al cierre; `vertical-session-registry-unit` pasó. Falta construir sus proveedores reales y conectarlo al arranque `--serve`.

### Restauración de misiones y cierre de aplicaciones de selección

Se integraron 01 `0200a66` (referencia de vida para cada aplicación de selección y barrera de cierre), 02 `79a5dbd` (logout en zonas retenidas), 04 `4034c2c` (composición de servicios de persecución), 05 `c5ffd21` (pruebas de daño tras cambios de recursos), 07 `1a0530f` (restauración de misiones desde la réplica de sesión), 08 `cfc33fb` (registro original de mejora NPC de gremio) y 09 `7daf5da` (drenaje después de fallas de trabajos). El propietario compuesto se reconcilió con la exigencia local de registro de red y reino, y su prueba confirma que el cierre no termina mientras exista una aplicación de selección y que una fábrica conservada después del cierre rechaza solicitudes.

Compilación y ocho pruebas aprobadas: `vertical-quests-unit`, `vertical-session-registry-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `guild-catalog-unit`, `vertical-db-drain-group-unit`, `vertical-combat-skill-damage-unit` y `vertical-combat-skill-hit-damage-unit`. Falta construir los proveedores reales del runtime y conectar el mundo al ejecutable, así como validar el cliente original.

### Réplica de misiones y compra de casa de gremio

Se integraron 02 `fbb2a34` (retención de sesiones antes del logout), 03 `3df9d6c` (sonda de cierre de movimiento), 04 `1663acf` (persecución y selección de ataque), 05 `0394b4f` (recuperación de ataque con recursos del Actor), 07 `4168673` (ruta de misiones a la réplica de personaje con el mutex compartido), 08 `185ac06` (compra de casa en réplica y guardado de gremio) y 09 `d24f563` (retención del registro durante drenaje). La prueba de misiones se reconcilió con el vínculo de reino del coordinador y verifica abandono, cambio de réplica y rechazo de actor o paquete incorrecto.

Compilación y siete pruebas aprobadas: `vertical-quests-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-combat-skill-damage-unit`, `vertical-db-drain-group-unit`, `serialized-world-application-tcp-unit` y `guild-remark-unit`. Adicionalmente, `guild-sql-integration` pasó contra SQL Server aislado en un catálogo temporal, verificando el guardado y lectura de la casa de gremio; el catálogo temporal fue eliminado y el contenedor apagado. La ruta de misiones aún necesita sus proveedores reales de producción y su conexión al ejecutable.

### Drenaje de sesiones y ciclo de misiones

Integrados 01 `71cff51` (frenar nuevas aplicaciones y selecciones antes del drenaje, preservando el vínculo local de registro y reino), 02 `da2210f` (orden de actualización de base del mundo), 03 `889d457` (afinidad al hilo serializado de movimiento), 04 `8e2cf63` (composición de IA de ataque), 05 `9832ff7` (recuperación según estado real del Actor), 07 `f3cb658` (prueba de ciclo de misiones sobre sesión retenida) y 08 `a12c57c` (operación aislada de compra de casa de gremio). La prueba de misiones se reconcilió con el vínculo de reino del coordinador. La operación de casa aún necesita conexión a los proveedores reales del runtime.

Compilación y siete pruebas aprobadas: `vertical-session-registry-unit`, `vertical-quests-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-combat-skill-damage-unit`, `vertical-social-session-unit` y `guild-catalog-unit`. El ejecutable y el cliente original todavía no están integrados de extremo a extremo.

### Despacho de gremio en el runtime retenido

Se reconciliaron los manejadores de gremio de 08 `d889dd3` con el runtime principal: rutas de consulta de etin, rango, aviso, mejora de NPC y compra de casa, además de los paquetes ignorados recuperados. La sesión aporta su catálogo real; las operaciones que lo requieren exigen sus puertos de DB, configuración, dinero o registro en vez de simular éxito. Una prueba envía una consulta de etin por la cola del runtime retenido y verifica la respuesta del gremio; el fixture asigna temporalmente la zona de admisión y no equivale a una conexión de cliente original.

También se integraron 02 `0c100a4` (planificación de actualización de base del mundo), 04 `251f248` (fase de ataque ordinario de monstruos) y 05 `5a0ccf0` (recuperación de pie con recursos de Actor). Compilación y cuatro pruebas aprobadas: `vertical-session-registry-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit` y `vertical-combat-skill-damage-unit`. Faltan proveedores reales y unión del runtime a `--serve`; no se han probado todas las rutas de gremio desde una sesión de producción.

### Preparación SQL y propiedad de puertos de misiones

Se reconciliaron 01 `610f12c` (identidad autenticada retenida y proveedor de preparación SQL del propietario de sesión), 02 `b6f21df` (controlador de actualización DB ligado al arranque y drenaje del mundo), 04 `527399c` (fase de ataque de monstruos), 07 `e522ac4` (la sesión conserva los puertos que utiliza su propietario de misiones) y 08 `e1544aa` (configuración de compra de casa de gremio). Los cambios compartidos se aplicaron manualmente preservando el registro de red/reino, la reserva y E/S retenidas, y la fábrica de admisión que utiliza el registro propio de `WorldRoot`.

Compilación y seis pruebas aprobadas: `vertical-session-registry-unit`, `vertical-quests-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `guild-catalog-unit` y `serialized-world-application-tcp-unit`. La prueba del proveedor SQL comprueba identidad y construcción, pero no abre SQL ni demuestra el guardado completo en producción. El proveedor social, los restantes servicios reales y la conexión del mundo a `--serve` continúan pendientes.

### Identidad de preparación y cierre coordinado

Se integraron 01 `a47fe31` (comprobar que preparación y router de ACK pertenezcan al registro, sesión y ejecutor correctos), 02 `fc63962` (prueba del controlador DB durante drenaje), 03 `ec6706a` (composición de movimiento con mundo compartido), 04 `3be8081` (despacho de ataque con propietario retenido), 05 `4bc8cf4` (contribuciones de bendición en recuperación), 07 `192088a` (retención del mutex de guardado compartido para puertos de misiones), 08 `b437951` (registro de compra de casa ligado a servicios de dinero) y 09 `fe0bd4e` (coordinación de cierre de transporte y propietario de mundo). El control de identidad se reconcilió manualmente con el runtime social del coordinador.

Compilación y ocho pruebas aprobadas: `vertical-session-registry-unit`, `vertical-quests-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-combat-skill-damage-unit`, `vertical-social-session-unit`, `serialized-world-application-tcp-unit` y `vertical-scene-unit`. La construcción completa de los servicios de producción y la prueba con cliente original siguen pendientes.

### Despacho de misiones y cierre de mundo

Se integraron 01 `a0ac7f1` (descartar preparación ajena sin usarla en teardown), 02 `c72984c` (reinicio de obeliscos sobre mapas retenidos), 04 `7023f47` (propietario de IA inactiva por zona), 07 `68f971f` (despacho de paquetes de misión/NPC para el Actor admitido), 08 `d3498f3` (respuesta de donación vacía y guardado de etin) y 09 `696008a` (cierre de transporte ligado al propietario de sesión). La ruta de misiones se conectó manualmente al despachador de propietarios del runtime; sus pruebas comprueban Actor, zona e identidad, pero todavía falta validación con cliente original y proveedores de producción.

Compilación y siete pruebas aprobadas: `vertical-session-registry-unit`, `vertical-quests-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `guild-remark-unit`, `vertical-social-session-unit` y `serialized-world-application-tcp-unit`. La integración SQL de gremio pasó en un catálogo temporal, incluyendo el guardado de etin; el catálogo se eliminó y se apagó el servidor aislado. El mundo completo sigue sin conectarse a `--serve`.

### Recogida de inventario y misiones a través de FIFO

Se integraron selectivamente 06 `97e3acd` y `b0a6151`: adaptador de recogida para la sesión, Actor, zona y propietario de guardado retenidos, con consulta de misiones a la sesión real y comprobación de identidad. Se añadieron al propietario de guardado los accesos de identidad y mutex necesarios; no se copiaron las versiones antiguas de otros subsistemas que la rama había importado. El escenario `vertical-world-inventory-session-unit` pasó, incluyendo rechazo de sesión/Actor o propietario de misión incorrectos. El adaptador aún necesita ser construido por la fábrica de gameplay de producción; callbacks reentrantes no deben alterar el ítem o cerrar la sesión. Una falla de réplica después de retirar el ítem puede dejar una operación parcial: requiere una política real de recuperación, no se afirma atomicidad.

Se integró la prueba de 07 `585dff9`, que envía el ciclo de misión por la cola real de paquetes de sesión retenida. `vertical-quests-unit`, `vertical-session-registry-unit` y `vertical-world-inventory-session-unit` pasaron conjuntamente. La recogida de un ítem de misión con propietario positivo instalado y el recorrido con cliente original todavía requieren validación.

### Preparación reentrante y fases de mundo

Se integraron 01 `248dce9` (cierre reentrante durante construcción y rechazo de preparaciones no frescas), 02 `a0bf074` (mercado y contador de muertes en zonas activas), 03 `cf13833` (colas pasivas de movimiento), 04 `3c3e164` (casos de límites del cursor de persecución), 05 `0d30078` (recuperación de usuario por zona), 08 `0e3b9a3` (paquetes de donación y respuesta vacía) y 09 `797aaf8` (cierre de login y mundo). Las modificaciones del runtime y `WorldRoot` se reconciliaron manualmente para conservar propiedad social, registro propio y barreras previas; el oráculo de persecución se añadió manualmente en CMake.

Compilación y nueve pruebas aprobadas: `login-network-unit`, `vertical-session-registry-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-scene-unit`, `vertical-quests-unit`, `vertical-combat-scheduler-unit`, `vertical-social-session-unit` y `serialized-world-application-tcp-unit`. No se demuestra todavía la unión de producción a `--serve` ni juego con cliente original.

### Recogida positiva de objeto de misión

Se integró 06 `7cfdd2f`, una prueba con propietario de misiones instalado y una recogida real en el escenario de sesión retenida. La batería `vertical-world-inventory-quest-unit`, `vertical-session-registry-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-combat-scheduler-unit`, `guild-save-unit` y `login-network-unit` pasó 7/7. El adaptador aún debe conectarse a la fábrica de gameplay de producción y conserva el riesgo documentado de operación parcial si falla la réplica después de retirar el objeto.

También se integraron 01 `a88eb2b` (evitar abrir recursos después de un cierre reentrante), 02 `c9851e4` (fase de rango de mundo), 04 `fe367a4` (guardas de ataque de monstruos), 05 `a2cb60c` (expiración de efectos de Actor retenido), 08 `4770214` (comando de eliminación de miembro de gremio) y 09 `92df2c2` (mantener E/S de login durante reintento de drenaje). El nuevo comando de gremio pasó su prueba unitaria; todavía no se ejecutó contra SQL aislado. Los cambios de runtime y raíz de mundo se reconciliaron sin reemplazar el registro de admisión compartido ni las barreras sociales existentes. El arranque de producción y el cliente original siguen pendientes.

### Cancelación antes de abrir recursos y ciclo de mundo

Se integraron 01 `81c5486` (cancelación de selección y apertura aún en cola), 02 `7362b97` (orden parcial del ciclo de mundo y decisión de envío), 03 `289978b` (composición única de movimiento por mundo), 04 `a84a289` (fin del ataque de monstruo), 05 `0f41d66` (cola de renacimiento del Actor), 08 `e1e3a5d` (rechazo de solicitud de gremio) y 09 `fd59f55` (preservar handoffs vencidos ante falla de barrido). Los cambios de `WorldRoot` se reconciliaron con la fábrica de admisión compartida; se añadió el oráculo de ataque manualmente en CMake.

Compilación y ocho pruebas aprobadas: `session-unit`, `login-network-unit`, `guild-catalog-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-session-registry-unit`, `vertical-scene-unit` y `vertical-combat-scheduler-unit`. La fase de mundo sigue siendo parcial y requiere servicios reales y conexión al arranque de producción.

### Composición de gameplay y envíos de mundo

Se integraron 02 `5c9e96d` (colas de envío por zona), 03 `f7bdc98` (protección de adquisición reentrante de movimiento), 04 `7f23930` (selección de objetivo de ataque de monstruo), 05 `7ce4a96` (comprobación de grupo para renacimiento), 06 `31ed959` (composición de recogida con el propietario de gameplay), 08 `7e916f9` (rechazo de solicitud de gremio en el runtime) y 09 `5a85c8e` (retener desconexiones de reemplazo fallidas). La raíz de mundo y el runtime se reconciliaron manualmente con sus propietarios existentes, y se añadió el oráculo de ataque en CMake.

Compilación y siete pruebas aprobadas: `login-network-unit`, `vertical-session-registry-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-scene-unit`, `vertical-combat-scheduler-unit` y `vertical-world-inventory-session-unit`. La prueba de inventario cubre composición con el runtime y limpieza del propietario; la fábrica de producción aún necesita los proveedores reales. La ruta alternativa de recogida de 07 `7226f66` no se integró porque el adaptador 06 ya valida la réplica y el guardado compartido; se requiere reconciliar su prueba sin introducir una segunda ruta que eluda esas barreras. La política de recuperación tras falla parcial, `--serve` y la prueba con cliente original siguen pendientes.

### Mutex de cuenta y comprobación de recogida

Se integraron 01 `f2c263f` (proveedor de preparación con acceso al runtime y mutex de la cuenta retenida), 02 `6fb6081` (composición parcial del ciclo activo hasta envío y limpieza), 03 `b2408dd` (caso de fallo de envío de movimiento), 05 `b2a5c93` (registro de renacimiento), 06 `2f15b3f` (revalidar propietarios después de callbacks de recogida), 08 `e226d51` (operación aislada de aceptación de solicitud de gremio) y 09 `e7513c5` (conservar desconexiones de handoff vencido fallidas). Se preservó la exigencia local de registro de red y reino en el constructor de sesión; el nuevo proveedor usa el mutex de cuenta pero aún no instala por sí mismo el propietario social.

Compilación y seis pruebas aprobadas: `login-network-unit`, `guild-catalog-unit`, `vertical-session-registry-unit`, `vertical-world-unit`, `vertical-combat-scheduler-unit` y `vertical-world-inventory-session-unit`. La rama 07 `490ea72` incluye una ruta alternativa de recogida y cambios de transferencia de zona; requiere reconciliación con la ruta de inventario del coordinador, que ya exige el propietario de guardado y sus barreras. No se aplicó en bloque. La recuperación de fallas parciales y el arranque de producción siguen pendientes.

### Transferencia de misiones y barreras de cierre

Se reconciliaron los cambios de 07 hasta `d32da3b`: reubicar el propietario de misiones y sus búsquedas de NPC tras una transferencia de zona, detectar fallas silenciosas de asignación de la réplica y probar los casos de identidad y reentrada. La ruta genérica alternativa de recogida está disponible para la prueba de la rama, pero el runtime del coordinador no la usa: la composición de inventario 06 conserva sus controles de propietario de guardado y mutex. También se recuperó el control de ataque antes de resolver una sesión de misión para recogida.

Se integraron 01 `5464086` (retener el propietario mientras existan routers ACK y comprobar contextos/cuentas sociales antes del cierre), 02 `aec6a33` (colas de envío propiedad de zonas), 04 `d3ad7e2` (comprobaciones de impacto de ataque de monstruos), 06 `e35f38b` (estado de grupo de recogida ligado al runtime retenido) y 08 `4e92ac2` (registro de aceptación de gremio). El constructor del propietario conserva la exigencia local de registro de red y reino. Compilación y seis pruebas aprobadas: `vertical-quests-unit`, `vertical-session-registry-unit`, `vertical-world-unit`, `vertical-mobs-frame-unit`, `vertical-world-inventory-session-unit` y `vertical-social-session-unit`.

La transferencia probada utiliza sesiones y zonas de fixture; faltan su invocación en el flujo completo de cambio de zona y la conexión de servicios de producción a `--serve`. La recuperación de fallas parciales de recogida y la validación con cliente original siguen pendientes.

Integración adicional: 02 0c471f0, 03 38e9659, 04 e88dc9d (oráculo agregado a CMake sin reemplazar integración local), 05 a3148d3, 06 6e373e1 y 09 3ace647. Compilación coordinada y 6/6 pruebas: login-network, vertical-world, vertical-mobs-frame, vertical-scene, vertical-combat-scheduler y vertical-world-inventory-session. Falta integración de desconexión 01, errores de misiones 07 y aceptación de guild 08, cuyos cambios comparten archivos con adaptaciones locales.

Integrado 07 9e1ca22 selectivamente y adaptado a la fase de zonas activas ya integrada; fallos de temporizador o despachador cercan la sesión, y validación de réplica comprueba lotes y conteos. Verificado 3/3 vertical-world-unit, vertical-quests-unit y vertical-session-registry-unit.

Integrado 01 2db1678, barrera de desconexión sin contexto y prueba adaptada al fixture de reserva de red. vertical-session-registry-unit aprobado.

Integrado 08 797d79f: servicios de aceptación de guild y despacho en runtime principal, pruebas guild-catalog-unit y vertical-session-registry-unit aprobadas. El escenario de aceptación online/offline del worker usa un fixture diferente y aún debe portarse al fixture principal; no considerar probado end-to-end.

Integrados 02 c7d4d6d, 03 7a65e28, 04 6e5761c, 05 50e978c, 06 dab6d0c, 07 6d0d324, 09 ab1a657. Batería 9/10 inicialmente: inventory-session colgaba porque intentaba continuar con sesión tras un fallo; se adaptaron pruebas de fallo de componente al despacho directo, y se agregó cierre RAII del io_context para informar errores. inventory-session pasó por separado; en total aprobadas las 10 pruebas previstas. Mantener prueba de fallo de despacho real en vertical-quests-unit.

Integrado 01 3d72583 headers, composición de cierre de zona en owner sin alterar constructor y reservas. Prueba de instalación y router de ACK adaptada pasó vertical-session-registry-unit. Falta portar el escenario completo del worker que valida guardado, ACK y salida de zona; no afirmarlo verificado de extremo a extremo.

Portado escenario 01 completo de logout diferido, guardado con binding real, ACK y liberación de zona/sesión al fixture con reserva de red. vertical-session-registry-unit aprobado. Esto valida composición de prueba; el servidor --serve aún no conecta el mundo completo.

Integrado 08 33d9621: restaurar plazo de reingreso desde snapshot en loaded() y ruta de rechazo de solicitud; vertical-social-session-unit y vertical-session-registry-unit aprobadas. Reglas y aritmética tienen prueba de componente; fixture runtime específico del worker aún no portado al principal.

Raíz de mundo y dueño de sesiones: WorldRoot::create_with_session_owner acepta exclusivamente alias del mismo propietario para registro y cola WorldAdmission; VerticalWorldSessionOwner::root_bindings entrega esos alias con una barrera que impide cerrar el dueño mientras la raíz lo retiene. Se conserva la creación anterior. vertical-world-unit comprueba identidad y vida de los objetos; vertical-session-registry-unit comprueba alias del dueño real y bloqueo de shutdown. Ambas pruebas aprobadas. Sigue pendiente construir los proveedores y conectar esta composición al ejecutable --serve y validarla con cliente original.

La prueba de raíz compartida también encola un usuario en el WorldAdmission del propietario, ejecuta update_admission desde WorldRoot, comprueba la visita y retira al usuario. Pasó vertical-world-unit. Continúa siendo fixture; falta ruta TCP y producción.

La composición raíz/sesión utiliza ahora un callback retenido al update_world del propietario y, por tanto, la barrera with_pass durante callbacks reentrantes, en lugar de recorrer WorldAdmission directamente. vertical-world-unit y vertical-session-registry-unit aprobadas.

Integrados 03 8800f76, 04 1caa8d6, 05 a480ab0, 07 fc73e5d y 09 9771d13; aprobadas session-unit, login-network-unit, vertical-mobs-frame-unit y vertical-combat-scheduler-unit; vertical-quests-unit pasó tras aislar su escena de consumo, que contaminaba el fixture anterior. Se mantiene pendiente conexión de producción.

Integrado 02 101cdac con composición de sesiones compartidas preservada: servicios de envío con recursos retenidos y colas de difusión para primarias e instancias. vertical-world-unit aprobada; la distribución de instancias sigue requiriendo llamada desde el planificador real.

Integrado 06 c96ddc7 con conciliación del fixture de errores fatales: pickup usa reloj de zona y conserva VerticalWorldPartyRuntime hasta terminar; el shutdown del grupo se bloquea con gameplay retenido. vertical-world-inventory-session-unit y vertical-world-inventory-quest-unit aprobadas. La ruta aún no está conectada a --serve y queda pendiente la recuperación ante fallo parcial de réplica.

Integrado 01 9523edbd reconciliando save_binding con el fence social: guardado de salida usa binding de la sesión, comprueba que las misiones estén en la réplica antes del lote, y conserva el servicio de desconexión durante sus fases. vertical-session-registry-unit aprobado, incluyendo estado poblado y la secuencia concreta. Persistencia SQL real y wiring de producción siguen pendientes.

Integrados 03 29192622 (movimiento y colas durante admisión), 05 f01e2b3a (continuaciones de resurrección) y 09 3ffaf739 (cierre de login retenido). session-unit, login-network-unit, vertical-scene-unit y vertical-combat-scheduler-unit pasaron 4/4. La raíz compartida ahora enruta la cola de espera de zona por la barrera de admisión del dueño; vertical-world-unit y vertical-session-registry-unit aprobadas, con tránsito de un usuario de selección a espera de zona. Integrado 01 32ce5ae4, rechazando ACK de salida anterior a solicitud de guardado.

Integrado 07 70a17e8e y unido al dueño real mediante install_retained_quest_zone_services: la fase comprueba la sesión, réplica y mutex compartidos antes de guardar el runtime de misiones. vertical-quests-unit y vertical-session-registry-unit pasaron; los fixtures prueban por separado la fase retenida y la instalación en el dueño, no un cliente real ni SQL de producción.

Integrado 02 2f5bdfed con raíz compartida y callbacks de admisión protegida preservados: send thread periódico, actualización de paquetes, envíos globales retenidos y distribución de instancias con orden original. vertical-world-unit y vertical-session-registry-unit aprobaron. El controlador está disponible para la composición de producción, aún no está enlazado a --serve.

Integrado 06 3faf45db: valida propietario de item, dinero e inserción de réplica y retiene el objeto durante retiro reentrante de zona. Se adaptaron pruebas negativas para llamar al componente directamente, pues el router desconecta por fallos. vertical-world-inventory-quest-unit y vertical-world-inventory-session-unit aprobadas. Falta política para recuperación de fallos parciales tras mutación.

Integrado 08 d1e8bd8e: solicitudes/cancelación/creación de membresía de guild, rutas de rechazo y comando 0b07 de DB. Pasaron guild-save-unit, vertical-social-session-unit y vertical-session-registry-unit; el último ejercita el rechazo por FIFO de sesión real. Ampliada guild-sql-integration: el procedimiento original usp_Create_GuildChar_E se copió a un catálogo temporal, se verificó inserción, rango, contador y solicitud duplicada; prueba aprobada, catálogo eliminado y contenedor aislado detenido. La aceptación completa en cliente y los proveedores de creación de producción siguen pendientes.

Integrado 04 ba692432: preparación de daño, registro, reflejo y cálculo de ataque de monstruos, con cuatro nuevos oráculos de ejecutable. vertical-mobs-frame-unit aprobado con la lista de oráculos y enlaces del árbol principal preservados.

Compilación completa del árbol principal aprobada (746 pasos incrementales). Batería CTest: 367/369 pasaron inicialmente; dos pruebas obelisk-runtime-{asset,maps}-unit fallaron por faltar .local/server-data/Obelisk.ini en el contenedor. Se montó /home/franco/Projects/Eternity/.local/server-data en solo lectura y ambas pasaron al reejecutarse. Esto valida los tests registrados, no equivale a servidor jugable. SQL aislado detenido.

Se copiaron 14 MiB desde /home/franco/Projects/Eternity/.local/server-data a .local/server-data del árbol principal, sin reemplazar archivos y sin modificar originales; diff -qr confirmó identidad. Las dos pruebas de obelisco aprobaron sin montaje externo. Con SQL aislado real, --check-original-loaders completó las consultas originales y cargó 68 mapas, 1379 NPC, 1720 misiones y 25 grupos enlazados; el runtime de zonas no arrancó. Se eliminó el archivo temporal de credenciales y se detuvo SQL.

Agregado WorldAssets: conserva definiciones SQL, NPC/misiones, vínculos de items, status/enchant/skills/mobs/experience, mapas y grupos enlazados, con ActorLoadRules referenciando los mismos propietarios; retain_maps permite al mundo conservarlos. --serve carga y retiene esos datos y reutiliza sus definiciones para reglas de creación. --check-original-loaders reutiliza la misma carga. Verificado contra SQL aislado y datos recuperados: todos los cargadores terminaron, --serve abrió ambos puertos efímeros y cerró limpiamente tras SIGTERM; contenedor SQL detenido y credenciales temporales eliminadas. El servidor sigue siendo solo de listado/creación: aún no instala WorldRoot, sus proveedores ni gameplay.

03-movement llegó a WAITING_FOR_ASSIGNMENT por identidad de transporte, que continúa en la rama 09. Se reanudó la misma sesión Terra low con tarea independiente de transferencia real de zona, movimiento y rebind de misiones/NPC; nuevo runner runner_zone_transfer.py y terminal activa, sin modificar ramas ajenas.

Integrados 03 9a435cdd, 05 0ae0bbe6 y 09 23fa88e0; pasaron vertical-scene, vertical-combat-scheduler, world-network y world-protocol (4/4). Integrado 07 5b95051f: entrada de listas de misiones desde sesión retenida, incluso preparación asíncrona; vertical-protocol, vertical-quests y vertical-session-registry pasaron (3/3).

Integrado 01 49d35201 reconciliando Destination y el fence social existentes: valida identidad/strand del binding, estado de habilidades/efectos y rechaza productores tras iniciar drenaje. La primera prueba de sesión agotó tiempo por fixture que usaba un LoginSession sin reserva; se adaptó a world_claim_fixture y añadió guardia de cierre del io_context. Luego pasaron vertical-session-registry, vertical-social-live-session, vertical-world-inventory-session y vertical-quests. Aún falta conectar estos dueños a --serve.

Integrado 02 8bdaad33: envíos globales retenidos, sesiones perdidas y temporizador/estado de kill count; vertical-world-unit y vertical-scene-unit aprobadas. Integrado 04 b8e0cd6a: ataque normal de monstruos ligado a propietarios de daño/muerte y oráculos nuevos; vertical-mobs-frame-unit aprobada.

Integrado 06 a179507e: invitaciones de intercambio, deadline en sesión y cancelación de intercambio/tienda antes del pickup. Los fixtures negativos se ajustaron a despacho directo para no reutilizar sesiones desconectadas; la limpieza de quest pickup se adelantó al inicio del drenaje. vertical-world-inventory-session-unit y vertical-world-inventory-quest-unit aprobadas. Aún falta conexión al servidor real y recuperación de fallos parciales de réplica.

WorldAssets expone alias retenidos para mapas, items, misiones y vínculos de items. Nuevo world-assets-sql-integration verifica carga real, identidad de todas las referencias ActorLoadRules, retención tras liberar el dueño original y destrucción al liberar el último alias; pasó contra SQL aislado y Map.ini recuperado. Integrado al flujo tools/dev.py integration. Contenedor SQL detenido y credencial temporal eliminada.

Integrado 08 395c25b9: rutas de abandono de guild, notificación/log y eliminación vacía; pasaron guild-save-unit, vertical-disconnect-unit, vertical-social-session-unit y vertical-session-registry-unit. Ampliada guild-sql-integration con usp_Delete_Guild_E copiado desde el catálogo original: verificadas baja de guild y miembros, contador a cero y liberación de réplica en catálogo temporal. Prueba aprobada; catálogo y credencial temporal eliminados, SQL detenido. La operación completa sigue pendiente de probarse con cliente y proveedores productivos.

09-network terminó temporalmente en WAITING_FOR_ASSIGNMENT por el wiring de producción. Se reanudó la misma sesión Terra low en terminal independiente con tarea concreta de identidad real, envíos pendientes y finalización de WorldChannel para desbloquear movimiento; runner_transport_projection.py, sin modificar checkout principal.

Integrado 07 71519aa2 selectivamente: restauración e instalación de misiones tras retener el save binding, verificando identidad de actor, runtime, pool, definiciones y mutex. Se portó AfterBind a las fábricas SQL/social preservando las barreras de 01 y la composición social del árbol principal; vertical-quests-unit, vertical-session-registry-unit y vertical-social-live-session-unit aprobaron. Los proveedores de preparación reales aún deben ser construidos por el arranque de producción.

Integrado 07 2b27af6d: callback de restauración de quest para AfterBind exige política real de registros inválidos, retiene pool y definiciones y comprueba binding/strand/mutex antes de instalar. vertical-quests-unit aprobada. El runtime productivo debe proporcionar el factory, puertos y política originales; no implica ingreso desde cliente.

Integrado 01 67fd48a8: valida propietario de selección y detiene el montaje de personaje/entrada tras cierre reentrante; preservados shared_ptr del personaje para quests y restauración del plazo de guild. La primera prueba usó dos cuentas de fixture sin token; se adaptaron a world_claim_fixture. vertical-session-registry-unit aprobada, además de vertical-quests y vertical-social-live-session aprobadas en la misma compilación previa.

Recompiladas y aprobadas 8/8 pruebas conjuntas después de cambios de selección, quests y fences: vertical-quests-unit, serialized-world-application-tcp-unit, vertical-async-entry-tcp-unit, vertical-async-disconnect-tcp-unit, vertical-session-registry-unit, vertical-social-live-session-unit, vertical-world-inventory-session-unit y vertical-world-inventory-quest-unit. Son fixtures/protocolos, no cliente original.

Integrado 02 d2ab2b71: estado y fases de kill count/bendición para entrada y obeliscos; vertical-world-unit aprobada. WorldAssets carga además KillStatus.SData original y expone su owner retenido para WorldKillCountComposition; world-assets-sql-integration validó ambas facciones, referencias y vida útil usando SQL y archivos reales. SQL aislado detenido. La inicialización real del estado de kill count y los puertos del mundo siguen pendientes.

Integrado 02 6faa9316 (incluye 720e93db y 9577cdc4): inicialización del estado de bajas desde respuesta DB y carga/guardado mediante procedimientos originales, con control del estado del mundo y validación de errores. vertical-world-unit aprobada; la conexión de producción al arranque y prueba SQL real específica siguen pendientes.
WorldAssets carga GuildHouse.ini mediante loader original, retiene configuración y diagnóstico de precio y expone alias con vida útil segura. world-assets-sql-integration y eternity-server compilaron y la prueba SQL con datos reales aprobó; contenedor detenido y credencial temporal eliminada.
Integrado 07 bfa7ffae (incluye 7a367e8d): proveedor de preparación SQL compone restauración de misiones con sesión y rechaza datos inválidos antes de instalarla; vertical-quests-unit aprobada. Falta conectar ese proveedor al arranque de producción.
Integrado 01 b337d224 (c13e32f2, bb9b5007 y b337d224): retención y cancelación segura durante entrega de entrada y despacho FIFO, reconciliado con quest_entry; fixtures adaptados a reserva de red real. vertical-session-registry-unit, vertical-admission-handoff-unit y vertical-quests-unit aprobadas individualmente.
Integrado 03 2dc56026 (f099d134 y 0bb414c5): transferencia retenida, cola de espera entre zonas, suspensión de gameplay y rebind de misiones; preservado fence de despacho de 01. Pasaron world-admission-unit, vertical-session-registry-unit, vertical-quests-unit y la prueba compuesta vertical_move_zone_transfer_probe con actor y misiones reales. Aún faltan puertos y conexión de producción.
Integrado 09 48bfe7f6 (incluye 2c24c195, e9bbb63b, 6ae61e4d y aa4502db): identidad de conexión, límites de envío y completación real de socket, cierre ante fallo de lookup. world-network-unit y serialized-world-application-tcp-unit aprobadas. Aún falta conectar este transporte al mundo de producción.
Integrado 02 eb2fed1d: propietario retenido para el ciclo de carga/guardado SQL de kill count, con prueba vertical-world-unit aprobada; todavía requiere ensamblado con conexiones de producción.
Integrado 04 7e7fed58 (ae085cef, d9f1c6b3, b0b714c2, 7e9930b8 y cbeabe1d): reflejo de habilidades, vistas compartidas, transición de muerte, defensa de equipo y daño a tres recursos; vertical-mobs-frame-unit aprobada con oráculos originales adicionales (2562 casos de daño). Sigue pendiente integración de producción.
Integrado 03 f848113b (incluye b7c1fff8): conserva la autoridad real de zonas e instancias durante el cambio, incluso transferencias a la misma zona. Aprobadas las tres pruebas vertical_move_zone_leases_probe, vertical_move_instance_leases_probe y vertical_move_zone_transfer_probe.
Integrado 05 b5320894: selección de punto de resurrección por mapa y zona, movimiento local y casos de vehículo con protecciones frente a estado obsoleto. vertical-combat-scheduler-unit aprobada; ensamblado de producción pendiente.
Integrado 06 5e97e7f5 (incluye 77c17058..99bf3a1a): respuestas, ofertas, controles y cancelaciones de intercambio retienen ambas sesiones y verifican validez de participantes y objetos. Se adaptaron escenarios deliberadamente inválidos para probar el componente directo sin reutilizar una sesión que el router cierra. vertical-world-inventory-session-unit y vertical-world-inventory-quest-unit aprobadas.
Integrado 07 934515ce (821c68f6): protege restauración ante cambios de réplica y temporizadores durante cierre; vertical-quests-unit aprobada.
Integrado parcialmente 08 f88baa40: expulsion online/offline, respuesta a invitación y salida del último miembro, con cambios del runtime reconciliados; vertical-social-session-unit, guild-save-unit y vertical-session-registry-unit aprobadas. El fixture de integración del worker tests/vertical_session_registry_unit.cpp no se trasladó por divergencia grande del principal; aún requiere prueba de integración equivalente y producción real.
Integrado 01 ccc22043 (02580526, 4b759c6f, cc9eb5f1 y 0adfac31): entrada diferida hasta entrega completa y ubicación publicada, aislamiento de errores en sesiones simultáneas y bloqueo de recepción tras fallo; combinado con el indicador de fallo de despacho y reservas reales. vertical-session-registry-unit aprobada con dos jugadores; vertical-quests-unit y vertical-world-inventory-session-unit aprobadas en la integración previa.
Integrado 02 c10c04e5 (82c6dddf, 13be0f1d, d0d4b892 y bbd83a57): propietario retenido DB-agent para reloj de mundo, bajas y estado de obeliscos, con orden de guardado y saludo. vertical-world-unit aprobada; uso real en --serve pendiente.
Integrado 01 82e50499: interrumpe preparación y publicación de personaje al fallar, preservando propiedad compartida de personaje, restauración de gremio y callbacks de misiones. vertical-session-registry-unit y vertical-quests-unit aprobadas.
Integrado 09 8b769b17 (incluye e750261c): lotes de envío acotados y buffers de prioridad retenidos hasta completación efectiva de socket. world-network-unit y serialized-world-application-tcp-unit aprobadas.
Integrado 05 6b90f8bd: movimiento de resurrección retenido, entrega a grupo y reinicio de comprobación de movimiento contrastados con oráculos; vertical-combat-scheduler-unit aprobada.
Integrado 03 ddedb3a3: prueba extendida de transferencia con autoridad de zona retenida y cierre de sesión durante espera; vertical_move_zone_transfer_probe aprobada.
Integrado 06 1e2ef454 (200f7612 y dd15c7a4): valida confirmación de intercambio, preserva rechazo de ofertas inválidas y transfiere objetos completos retenidos sin liberarlos del pool. Reconciliadas las pruebas negativas para no reutilizar sesión cerrada; mob-drops-inventory-unit y vertical-world-inventory-session-unit aprobadas.
Integrado 02 3ee5ff09 (incluye 438ba14c): calendario nativo y programación/cancelación del guardado de DB-agent. vertical-world-unit y obelisk-runtime-calendar-unit aprobadas; faltan conexiones de servicios reales y --serve.
Compilación general completa (510 pasos incrementales tras cambios compartidos) y batería CTest completa: 369/369 aprobadas en 142,46 s usando assets originales preparados. No incluye una conexión con cliente original ni demuestra producción jugable; SQL real se comprobó por separado en pruebas focalizadas previas.
Integrado 07 3d7d6f43 (56ae50f4, 7144ce15, 4fba17d3, d1a76546): protección de zonas retenidas y criterio de elegibilidad al procesar temporizadores de misiones; bloquea acceso tras fallo de sesión. Aplicado overlay compatible y reconciliados cambios posteriores de mundo. vertical-world-unit y vertical-quests-unit aprobadas. La batería 369/369 corresponde al checkpoint anterior a este cambio.
Integrado 04 5fe01603 (78fc0523, fd46e500 y 5fd8f01f): selección y temporización de habilidades de mobs, notificación de daño y coordinación del final del golpe; oráculos originales (más de 2500 trazas nuevas). vertical-mobs-frame-unit aprobada. Efectos de habilidad y transporte productivos aún no están completos.
Integrado 09 71ec85ac: prueba TCP de reintento tras rechazo de lote prioritario, comprobando retención y liberación única del buffer; serialized-world-application-tcp-unit aprobada.
Integrado 06 e366290a (808adcf1): secuencia DB, cliente y registro para intercambio completo con dueño retenido; 64 trazas originales y vertical-inventory-unit aprobadas. La construcción real del settlement y recuperación ante entrega parcial siguen pendientes.
Integrado 01 ee57d4fc (01a80491): respeta cancelación y evita desconexiones repetidas al informar fallos de admisión; vertical-admission-handoff-unit y vertical-session-registry-unit aprobadas.
Integrado 02 9415ad20: ordena guardado del mundo y usuarios durante eliminación de conexión de DB-agent; vertical-world-unit aprobada. Sigue pendiente conexión real al servicio principal.
