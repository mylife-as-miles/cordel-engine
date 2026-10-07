# Exercise the actual home -> viewport -> Say -> home UI flow, independently
# of the lower-level software experiment harness.
testsuite global:
    before testcase:
        $ _test.timeout = 5.0
    teardown:
        exit

testcase viewport_ui:
    click "Enter viewport"
    pause 0.3
    assert screen "cordel_viewport"
    $ before_dialogue_frames = viewport.metrics.frames
    keysym "K_F7"
    assert "The world continues drawing"
    pause 0.3
    assert eval (viewport.metrics.frames > before_dialogue_frames)
    assert eval (not viewport.input_enabled)
    keysym "K_RETURN"
    pause 0.2
    assert eval viewport.input_enabled
    keysym "K_F9"
    pause 0.2
    assert screen "cordel_home"
    click "Enter viewport"
    pause 0.3
    assert screen "cordel_viewport"
    keysym "K_F9"
    pause 0.2
    assert screen "cordel_home"
