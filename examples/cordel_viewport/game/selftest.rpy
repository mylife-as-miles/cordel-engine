# The full executed-evidence harness is in a Python module, separate from demo UI.
label cordel_selftest:
    python:
        from cordel_spike.selftest import run
        try:
            run()
        except Exception:
            import traceback
            traceback.print_exc()
            renpy.quit(status=1)
    $ renpy.quit(status=0)
