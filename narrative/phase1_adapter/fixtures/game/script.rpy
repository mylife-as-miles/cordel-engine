# Copyright (c) 2026 CORDEL contributors. MIT. Same AST in adapter and UI reference.
define config.name = "CORDEL Narrative Ownership Fixture"
define config.version = "0.1.0-dev"
define config.save_directory = None
define config.has_autosave = False
define config.rollback_enabled = False
define config.allow_skipping = False
define config.window = "hide"

init python:
    import os
    import sys
    sys.path.insert(0, os.path.abspath(os.path.join(config.gamedir, "../..", "worker")))
    sys.path.insert(0, os.path.abspath(os.path.join(config.gamedir, "../..", "protocol")))
    from service import Worker, Reference
    bridge = Reference() if os.environ.get("CORDEL_NARRATIVE_REFERENCE") == "1" else Worker()
    def worker_command():
        renpy.arguments.takes_no_arguments()
        bridge.run()
        return False
    renpy.arguments.register_command("cordel_worker", worker_command, uses_display=False)
    if isinstance(bridge, Worker):
        config.menu_actions = False
        narrator = bridge.say
        menu = bridge.menu
    else:
        def record_dialogue(event, **kwargs):
            if event == "begin":
                bridge.dialogue(kwargs.get("what", ""))
        narrator = Character(None, callback=record_dialogue)

screen say(who, what):
    window:
        id "window"
        yalign 1.0
        vbox:
            text what id "what"
    timer 0.06 action Return()

screen choice(items):
    vbox:
        for item in items:
            textbutton item.caption action item.action
    timer 0.06 action items[bridge.reference_index].action

label start:
    call cordel_story
    $ bridge.finish_reference()
    $ renpy.quit()

label cordel_story:
    $ outcome = "unset"
    $ story_counter = 0
    "Can you still hear me?" id cordel_opening
    $ bridge.command("set_beacon_enabled", "tall_gold", True)
    $ bridge.wait("beacon_reached")
    "Good." id cordel_good
    jump cordel_choice_boundary

label cordel_choice_boundary:
    $ bridge.boundary = "cordel_choice_boundary"
    menu:
        "Continue":
            $ outcome = "continue"
            $ story_counter += 1
        "Stay":
            $ outcome = "stay"
            $ story_counter += 2
    jump cordel_final_boundary

label cordel_final_boundary:
    $ bridge.boundary = "cordel_final_boundary"
    if outcome == "continue":
        "We continue." id cordel_continue
    else:
        "We stay." id cordel_stay
    return

label cordel_exception:
    "Before the fault." id cordel_fault
    $ raise RuntimeError("CORDEL deterministic narrative exception")
    return

label cordel_invalid_target:
    $ bridge.command("set_beacon_enabled", "missing_object", True)
    return

label cordel_pause:
    $ bridge.command("pause_world", "world", True)
    "Simulation paused explicitly." id cordel_paused
    $ bridge.command("resume_world", "world", False)
    "Simulation resumed." id cordel_resumed
    return
