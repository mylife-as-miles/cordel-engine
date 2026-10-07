# CORDEL-only reference spike; upstream runtime and examples remain untouched.
define config.name = "CORDEL ENGINE — Phase 1.1 Viewport Spike"
define config.version = "0.1.0-dev"
define config.screen_width = 1280
define config.screen_height = 720
define config.rollback_enabled = False
define config.save_directory = None
define config.has_autosave = False
define config.developer = True
define config.allow_skipping = False
define config.window = "hide"
define config.gl2 = True
define config.pass_controller_events = True
define config.adjust_view_size = lambda width, height: (width, height)

init python:
    import os
    from cordel_spike.runtime import Viewport, register_shader, shutdown_all
    register_shader()
    config.at_exit_callbacks.append(shutdown_all)
    # LCtrl belongs to the fly camera in this project, rather than story skip.
    config.keymap["skip"] = []
    config.keymap["stop_skipping"] = []
    if os.environ.get("CORDEL_VIEWPORT_SELFTEST") == "1":
        preferences.physical_size = (922, 519)
        preferences.fullscreen = False
        preferences.maximized = False

screen cordel_home():
    add Solid("#18212b")
    vbox:
        xalign 0.5
        yalign 0.5
        spacing 24
        text "CORDEL ENGINE 0.1.0-dev" size 38
        text "Phase 1.1 — Real-time 3D viewport reference spike" size 24
        textbutton "Enter viewport" action Return("enter")
        textbutton "Quit" action Return("quit")
        text "F9 leaves the viewport. Re-enter to test resource lifecycle." size 18

screen cordel_viewport(viewport):
    add viewport
    key "K_F7" action Return("dialogue")
    key "K_F9" action Return("leave")
    on "hide" action Function(viewport.close, "screen_hidden")

# Only shown by the software harness. Executes after per_interact clears input.
screen cordel_test_input(callback):
    timer 0.03 action Function(callback, _update_screens=False)

screen say(who, what):
    window:
        id "window"
        background Solid("#101722ee")
        xfill True
        yalign 1.0
        ysize 156
        padding (28, 18)
        vbox:
            if who:
                text who id "who"
            text what id "what" size 25
    if os.environ.get("CORDEL_VIEWPORT_SELFTEST") == "1":
        timer 0.75 action Return()

label start:
    if os.environ.get("CORDEL_VIEWPORT_SELFTEST") == "1":
        jump cordel_selftest
    while True:
        call screen cordel_home
        if _return == "quit":
            $ renpy.quit()
        $ viewport = Viewport()
        show screen cordel_viewport(viewport)
        $ viewport_result = None
        while viewport_result != "leave":
            $ viewport_result = renpy.ui.interact()
            $ viewport.release_input("interaction_exited")
            if viewport_result == "dialogue":
                $ viewport.set_input_enabled(False)
                "The world continues drawing while Ren'Py waits for this line. Click or press Enter to return."
                $ viewport.set_input_enabled(True)
        hide screen cordel_viewport
        $ viewport.close()
        $ viewport = None
        $ renpy.pause(0.05, hard=True)
