//  Draft from cfr_steps_sample.steps.jsonl: 2 rows. Fill in every TODO and REVIEW.
rows.push_back(row(
    "01.03", "TODO title", in(LUMIOSE),
    {
        make(StoryStepKind::TALK, "talk_L2", "TODO why"),  // L2 lza_talk_to
        make(StoryStepKind::MASH, "scene_L3", "TODO why", "a"),  // L3 lza_talk A
        // RAW 2 inputs (L4-L5: press A; dpad DOWN) -- REVIEW: replace with a story action
        StoryStep{.kind = StoryStepKind::ANSWER, .name = "answer_L6", .why = "TODO why", .yes = false},  // L6 lza_pick 1
        move("move_L8", "TODO why"),  // L8 lza_move ahead 1500
        // REVIEW L9 lza_move 270 800: FAILED: run_story_move(): a day/night change cut the run short.
        // RAW 1 inputs (L10: stick L 0 1 400) -- REVIEW: replace with a story action
        move("move_L11", "TODO why"),  // L11 lza_move 270 800
        make(StoryStepKind::BATTLE, "battle_L12", "TODO why"),  // L12 lza_battle
    }
));
rows.push_back(row(
    "01.04", "TODO title", in(LUMIOSE),
    {
        StoryStep{.kind = StoryStepKind::ROUTE, .name = "fly_L14", .why = "TODO why", .target = in("centrico-plaza"), .variant = "fly"},  // L14 lza_fly centrico-plaza
        // REVIEW L15 lza_face 45: a turn on its own; fold it into the next lza_move's heading
        // REVIEW L16 lza_pick 2: ANSWER picks choice 0 or 1; a longer list needs a GAME step
        // REVIEW L17 lza_heal: FAILED: PLZA AutoStory: heal_at_pokemon_center is not implemented yet. TODO: talk to the nurse: no LZA program heals at a Pokemon Center yet.
        // REVIEW L18 type Kosmos: no story step for this command
        make(StoryStepKind::MASH, "scene_L19", "TODO why"),  // L19 lza_talk
    }
));

//  story_moves():
{"move_L8", StoryMove{.heading = 90, .duration = 1500ms}},
{"move_L11", StoryMove{.heading = 270, .duration = 800ms}},
