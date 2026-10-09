# 06 Stage descriptions

Status: resolved (2026-10-09)
Type: task
Blocked by: 01

Spec: `.scratch/stage-select/spec.md`, "Stage names and descriptions".

- Take a screenshot of each stage's large preview on stage select with every stage unlocked. An online session forces unlock-all; `tools/a11y/drive_net.py` with a script that stops at stage select reaches it. Keep the images in the scratchpad, not in git.
- Write one or two plain sentences per stage, 29 in all, about what a player would want to know:
  - the ground's shape: one flat platform, several levels, walk-off edges;
  - moving parts and hazards;
  - its feel.
- Base them on the previews and on what `docs/a11y/gameplay-state.md` records; say nothing that a preview and the code don't support.
- Hover says the name and the description as one announcement: "Corneria. A fight on the hull of a ship flying over a city." Locked tiles and the random tile have no description.

## Done when

- The table has a description for every stage, checked against its preview.
- Unit tests and the drive scripts' expected hover lines are updated.
- Issue 07 asks the maintainer to judge the descriptions.

## Comments

### 2026-10-09, implementation (agent)

Implemented; judging the words by ear is issue 07.

- **No large preview.** Stage select draws no bigger picture of the hovered stage: hovering changes only the two-line name, and after a pick the screen dims under "Now loading" (screenshots of an offline run with `--no-card`, which unlocks every stage like an online session). The previews are the tiles themselves, about 140 by 120 pixels in a 1920 by 1080 window; each was cropped from one screenshot, enlarged four times and looked at. The three past stages' tiles show only a background (a rainbow over a sea, an orange sky with clouds, a sunset), so their descriptions lean on the stages' code.
- **Sources.** Each description says only what its tile shows or what the stage's code in `src/melee/gr/` (named parameters, parts, item kinds) or `docs/a11y/gameplay-state.md` supports: for example Great Bay's floating floor that tilts and sinks (`floatfloor_*`) and turtle (`kame_*`), Fourside's UFO and crane beam (`ufo_*`, `crane_iron_*`), Onett's springy awnings and cars with hit boxes, Brinstar's acid (`grZe_AcidLevelEntry`), Brinstar Depths turning with Kraid's hand, Yoshi's Island's nine blocks that disable their collision after damage or a hard landing, Yoshi's Island N64's clouds that sink while stood on and vanish, the barrels of both Kongo Jungles (`ftCo_BarrelWait`), Corneria's Great Fox laser. Claims the code did not show clearly were left out: Peach's Castle's moving parts, Green Greens' bombs, which way Icicle Mountain scrolls, whether Mute City's and Big Blue's cars hit, platform counts for the past stages.
- **Table.** `sss_names.cpp` holds the description beside each name (`stage_description`), one or two sentences each. Hover says them as one announcement after the name ("Corneria. A fight on the hull of a big ship flying over a city. Arwings fly past and fire, and so does the ship's cannon."); the random tile and locked tiles have none. `SssSpeech::hover_words` builds it; `tile_words` stays the name alone, for "Random: Corneria.", "Stage: Corneria." and, in issue 04, "Could not reach Corneria".
- **Tests.** `test_sss_speech`: the hover lines with descriptions, Corneria's whole announcement as a literal, every stage's description one or two sentences ending in a full stop, and `tile_words` without a description. `sss_hover.drive` waits on "Battlefield. One ..." and on any description after the other stages it meets.
- **Issue 07** names the descriptions in its listening list: length, usefulness, whether they slow browsing, and any that mislead, since they were written without playing.
- **CLAUDE.md.** The stale scene announcement wording the lead reported was already gone (issue 01's commit rewrote that line); the stage select status line now names the descriptions and the skipped scene, which it still listed as to come.
- **Verification**, quick level: build, style, unit tests (`sss_speech` updated; `launcher_data` fails as known), a bounded `title` run, `sss_hover.drive` (twice) and `sss_skipped.drive` passed.

For issue 04: a step names its destination with `hover_words` (the description included, as on any hover); "Could not reach" and the glide log keep `tile_words`, the name alone. The longest announcement is now about two sentences, so the next step's interruption matters for quick browsing.

### 2026-10-09, descriptions checked against the stage code (agent)

A second pass checked each stage's claims against `src/melee/gr/`, and twelve descriptions changed:

- **Great Bay:** the platform that tilts and sinks is Tingle's balloon, and the turtle dives and comes up somewhere else.
- **Peach's Castle:** two platforms rise when stood on and sink back.
- **Both Kongo Jungles:** a moving barrel catches fighters and fires them out. Nothing in the code places the barrel below the stage, and Klaptraps appear in Kongo Jungle too.
- **Yoshi's Story:** the Shy Guys carry food.
- **Rainbow Cruise:** the scroll's route is in animation data, so it is no longer described.
- **Yoshi's Island:** the blocks become passable for a while rather than falling.
- **Green Greens:** blocks fall and stack, some of them bombs.
- **Brinstar:** pieces break when hit and grow back.
- **Onett:** the awnings give way after enough landings.
- **Mute City:** the cars can hit fighters.
- **Mushroom Kingdom:** gained its scale lifts.

Still not in the code by name: Peach's Castle's Bullet Bill, a POW block, and the acid's damage, which `docs/a11y/gameplay-state.md` records. Brinstar keeps "burns" on that primer's word. `sss_hover.drive` passed again.

### 2026-10-09, the rule applied to what the code does not show (agent)

The lead asked that wording follow the code check strictly.

- **Brinstar:** no longer says the acid burns. The code shows the acid rising and falling (`grZe_AcidLevelEntry`) but not dealing damage, and the primer's "burns" is not enough.
- **Mushroom Kingdom II:** now says a flying carpet "crosses the stage as a platform". The tile shows the carpet, and the code shows an unnamed platform that enters from beyond the edge and crosses the stage.
- **Big Blue:** keeps "a track racing past below". The tile shows the roads, and the code gives the stage a motion that runs past; the description no longer says the track carries anyone away.
- **Both Kongo Jungles:** say "a moving barrel that catches fighters and fires them out", without "below" (changed in the previous pass).
- **Poké Floats:** keeps its Pokémon balloons, which the tile shows (Chansey, Psyduck).

Well-known Melee features left out for lack of code support, for the play test to add back if wanted:

- the acid burning on Brinstar;
- Peach's Castle's Banzai Bill (an unnamed object with a hit box, which the camera follows, fits it), its switches and bumpers;
- Mushroom Kingdom's POW block, Goombas and Koopas;
- Mushroom Kingdom II's Pidgit and log, by name;
- the Kongo barrels sitting under the stage and shooting fighters back up;
- Big Blue's track carrying fighters away;
- Rainbow Cruise's route, from the ship to the platforms and back;
- Dream Land N64's three platforms;
- Kraid's attacks hurting fighters;
- Icicle Mountain scrolling mostly upward;
- Fountain of Dreams' rising platforms being the side ones (the primer says they are);
- Great Bay's falling moon and Tingle knocking the balloon over;
- Venom's lower level;
- Hyrule Temple's tunnel.

Verification, quick level: build, style, unit tests (`launcher_data` fails as known), a bounded `title` run and `sss_hover.drive` passed.
