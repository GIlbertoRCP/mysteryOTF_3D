"""Smoke test for questioning: collect two pieces of evidence, then question Margaret until she cracks.

  UnrealEditor.exe NoirMystery.uproject -ExecCmds="py Scripts/playtest_interview.py" -unattended -nosplash

Screenshots (ScreenShotNNNNN.png): greeting, an answer, the evidence menu, the crack, the follow-up.
"""
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels.load_level("/Game/Maps/L_LookDev")
levels.editor_request_begin_play()

state = {"t": 0.0, "step": 0}


def find(world, cls, label):
    return next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls) if a.get_actor_label() == label)


def shot(world):
    unreal.SystemLibrary.execute_console_command(world, "Shot showui")


def script(world, pawn):
    """(delay seconds, action) pairs, run in order."""
    margaret = find(world, unreal.NoirSuspect, "Suspect_Margaret")

    def verify_initial():
        comp = margaret.get_composure()
        print(f"[PLAYTEST] Margaret initial composure: {comp:.2f}")
        assert abs(comp - 1.0) < 0.05, f"Expected initial composure ~1.0, got {comp}"
        # Verify EvaluateContradiction method
        c_watch = margaret.evaluate_contradiction("Alibi", "Evidence_Watch")
        c_letter = margaret.evaluate_contradiction("Alibi", "Evidence_Letter")
        c_cuff = margaret.evaluate_contradiction("Alibi", "Evidence_Cufflink")
        print(f"[PLAYTEST] EvaluateContradiction: Watch={c_watch}, Letter={c_letter}, Cufflink={c_cuff}")
        assert c_watch, "Watch should contradict alibi"
        assert c_letter, "Letter should contradict alibi"
        assert not c_cuff, "Cufflink should not contradict alibi"

    def verify_watch():
        shot(world)
        comp = margaret.get_composure()
        print(f"[PLAYTEST] Composure after presenting Watch: {comp:.2f}, Stress: {margaret.get_stress():.2f}")
        assert comp < 0.9, f"Composure should have dropped, got {comp}"
        assert not margaret.is_cracked(), "Margaret should not have cracked yet"

    def verify_crack():
        shot(world)
        comp = margaret.get_composure()
        print(f"[PLAYTEST] Composure after presenting Letter: {comp:.2f}, Cracked: {margaret.is_cracked()}")
        assert margaret.is_cracked(), "Margaret should crack after letter"
        assert abs(comp - 0.0) < 0.05, f"Composure should be 0.0 on crack, got {comp}"
        topics = margaret.get_available_topics()
        print(f"[PLAYTEST] Available topics count after crack: {len(topics)}")
        assert len(topics) >= 3, "Topic 3 should be unlocked after cracking"

    return [
        (5, lambda: find(world, unreal.NoirSearchable, "Search_Desk").interact(pawn)),
        (1.5, lambda: pawn.end_inspect()),
        (1, lambda: find(world, unreal.NoirSearchable, "Search_BookcaseN").interact(pawn)),
        (1.5, lambda: pawn.end_inspect()),
        (1, lambda: pawn.set_actor_location(unreal.Vector(160, 40, 100), False, True)),
        (1, lambda: (verify_initial(), margaret.interact(pawn))),
        (1.5, lambda: shot(world)),                       # greeting
        (0.5, lambda: pawn.choose(1)),
        (0.5, lambda: shot(world)),                     # answer
        (0.5, lambda: pawn.toggle_present()),
        (0.5, lambda: shot(world)),                     # evidence menu
        (0.5, lambda: pawn.choose(1)),                  # the watch: stress 0.6
        (0.5, verify_watch),
        (0.5, lambda: pawn.toggle_present()),
        (0.5, lambda: pawn.choose(2)),                  # the letter: cracks
        (0.5, verify_crack),                            # crack line, follow-up unlocked
        (0.5, lambda: pawn.choose(3)),
        (0.5, lambda: shot(world)),                     # follow-up answer
        (0.5, lambda: (print("[PLAYTEST] ALL CONTRADICTION & COMPOSURE TESTS PASSED!"), pawn.end_interview())),
        (1, lambda: levels.editor_request_end_play()),
        (2, None),
    ]


def tick(dt):
    state["t"] += dt
    world = ues.get_game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    if "steps" not in state:
        if not pawn:
            return
        state["steps"] = script(world, pawn)
    steps = state["steps"]
    if state["step"] >= len(steps):
        return
    delay, action = steps[state["step"]]
    if state["t"] < delay:
        return
    state["t"] = 0.0
    state["step"] += 1
    if action:
        action()
    else:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
