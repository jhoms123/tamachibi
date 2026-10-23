# Connected gameplay and Botany update

This change set includes the food Garden and ornamental Plant Room overhaul,
its authored sprites and rendered previews, and the gameplay changes below.
It targets `ESP32_Tamachibi_v47_ESP32U`; the legacy Pico firmware is separate.

## Completed activities drive progression

`GameplayActions.h` defines the shared action IDs. Successful activity code calls
`completeGameplayAction`; dispatching, inspecting, cancelling, unaffordable actions,
and failed preconditions do not emit that event. Living World skills/reputation,
Deep Life goals/emotions, and Growing Together promises/hobbies then receive the
same completed activity. Runtime credits prevent repeating one activity within a
five-minute life tick from stacking its secondary progression and friendship coin
bonus. This ledger resets on load/new life and does not alter the EEPROM layout.

Real minigame terminal outcomes, including losses, count as completed play. Holding
the exit button never fabricates a result. Arcade outcomes are consumed exactly
once, recorded by both world systems, and preserve the actual assisted win/loss.
Fast legitimate completions remain valid. Stage-specific games emit completion
only after finishing. Learned favorite games survive adult evolution.

Routine flags describe the day; they no longer duplicate narrative action credit.
Chores match their actual action type, promises require a matching completion,
and ritual/tradition streaks advance once per pet day. Past missed chores no longer
cause a fairness penalty on every later day.

## Shared access and time rules

House and City shortcuts go through `executeAction`, just like the main menu.
Stage unlocks, sleep, school/work, runaway state, and Sitter mode therefore apply
to both routes. Sitter returns before schedules or any simulation component ticks:
pet, plants, fish, narrative timers and secondary progression are all paused.
Ending Sitter restores normal scheduled play; its normal cooldown remains.

## Social and family progression

Neighbors are directly accessible from Talk. Dating starts through an adult's
actual town friendship (60+), rather than a second instant partner selector.
Opening Family only inspects the current relationship. Readiness consistently
requires adulthood and partner bond 80; compatibility is informational rather
than an additional contradictory readiness gate. Existing partners remain valid.

Town interactions are limited per resident and interaction type per life tick.
Family outings/dinners and relevant relationship stories update the town relation
and core partner bond, while harmony represents the state of the household.
Ordinary conversations with the owner no longer automatically increase every
child's bond and a spouse's harmony. Family actions have their own completion
credit. Welcoming children, club meetings and rival interactions have appropriate
daily limits/costs instead of rewarding repeated screen visits. Group switching
starts the new club's progression. Scheduled meetings, child aging and anniversaries
are each processed once on the relevant day, rather than on every five-minute tick
throughout their triggering hour/day.

## Individual identity

Each saved birth seed, lineage and temperament derives a stable natural activity:
Garden, Plant Room, Aquarium, Study, Train, Music, Explore, Cook, Craft or Games.
The same species can have a different aptitude in another life. Completed talent
activities earn an additional relevant advanced skill point. Gardening talents
improve high-quality yield, florists improve mature sales, fish carers gain extra
favorite-meal friendship, training/music talents improve successful practice,
cooks gain kindness, and crafters reduce advanced material requirements.
Status page 8 explains the current pet's aptitude. Learned likes remain separate
and respond to how the player raises the pet.

Weather preference is no longer overwritten by every activity. The remembered
best neighbor follows actual relationship strength rather than simply the most
recent person visited. Personality and routine inspection cannot mint stat rewards.

## Resource and location connections

| Source | Practical effect |
| --- | --- |
| Food Garden | Twelve crops -> pantry ingredients -> cooking -> meals |
| Plant Room | Ten ornamentals -> mature sales and daily florist orders -> coins, rank, style, town reputation and memories |
| Mature ornamentals | Keeping a healthy plant retains its existing daily benefit; selling frees a limited pot and funds growth |
| School project | Successful studying builds extra intelligence |
| Park project | Level 2 reduces exploration energy cost |
| Clinic project | Reduces purchased medicine cost; the selector shows the actual dose price |
| Market project and reputation | Increase mature ornamental sale value |
| Spaceport project | Reduces the displayed and charged space-trip biofuel cost |
| Fossil donation | Also advances the town museum community project |
| Fishing catch | Also supplies Aquarium food |
| Companion care/training | Consistent XP thresholds and levels improve existing expedition strength |
| Career skills | Each career uses its appropriate advanced skill; the displayed pay bonus and paid wage share one formula |

Watering/tending saturated plants no longer produces endless free care rewards.
Ornamentals start at the species' appropriate moisture level. Work requires a real
completed shift; choosing a career or being too tired pays nothing. Companion
care has need/resource checks and shared completion limits. Festivals can only
award one entry per pet day. Community donations and narrative/research
contributions use the same 0–100 percent progression scale.

## Research has a purpose

Paradise -> Applied Research offers four projects for **10 research + 1 energy**:

- Water and tend planted food crops that need irrigation.
- Tend the ornamental collection.
- Add 10 XP to a selected adult career.
- Add 25% to the day's rotating unfinished town project.

Invalid projects and insufficient funds do not spend resources. Completion also
creates a scrapbook memory and advances the shared hobby experience. Field
expansion costs 50 coins and 50 research at the existing Planet Level gate. It
actually doubles the selected outdoor toy's daily effect, matching its description.
Planet Level still describes raised generations and discovered habitats; research
is a separate spendable resource.

## Verification

Verified with the pinned PlatformIO ESP32 target and Arduino-ESP32 3.3.12.
The firmware build, gameplay regression checks, and 23 renderer fixtures pass.

Run `python3 tools/check_gameplay.py` to compile and execute extracted real
firmware dispatch/completion, arcade accounting, Deep Life timers, Growing Together
action handling and research spending against deterministic UI/activity doubles.
The checks cover cancellations, insufficient energy, cooldowns, failed/repeated
outcomes, world-route access, promises, chore matching, daily streaks and aging,
individual seed stability, and atomic research spending.

Run `python3 tools/render_ui.py` after dependencies are installed to rebuild the
firmware-rendered Garden/Plant Room previews and renderer assertions.
Run `python3 -m platformio run -e esp32u` inside the ESP32 project for a full build.
Physical button feel, real-time balance over a multi-day life and OLED presentation
still require playtesting on the device. No existing save record is enlarged or
moved by this update.
