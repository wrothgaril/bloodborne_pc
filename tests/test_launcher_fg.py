"""Unit tests for Frame Generation and Reflex settings in bbport launcher and environment."""
import os
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'launcher'))
sys.path.insert(0, str(ROOT / 'scripts'))
# The module itself: a second copy rebinds the launcher's page modules (bbport_setup, bbport_help,
# bbport_search) to it, and later tests then translate through the copy's language.
import bbport_launcher_win as launcher  # noqa: E402


class Variable:
    def __init__(self, value):
        self.value = value
        self.callbacks = []

    def get(self):
        return self.value

    def set(self, value):
        self.value = value
        for callback in self.callbacks:
            callback()

    def trace_add(self, _mode, callback):
        self.callbacks.append(callback)


class Combobox:
    def __init__(self, _parent, **options):
        self.options = options
        self.text = ''
        self.events = {}

    def current(self, index=None):
        if index is not None:
            self.text = self.options['values'][index]
        return self.options['values'].index(self.text) if self.text in self.options['values'] else -1

    def set(self, text):
        self.text = text

    def get(self):
        return self.text

    def bind(self, event, callback):
        self.events[event] = callback

    def configure(self, **kwargs):
        self.options.update(kwargs)


class LauncherFgSettingsTests(unittest.TestCase):
    def test_ini_defaults_include_fg_and_reflex(self):
        self.assertIn('frame_generation', launcher.INI_DEFAULTS)
        self.assertEqual(launcher.INI_DEFAULTS['frame_generation'], 'off')
        self.assertIn('fg_backend', launcher.INI_DEFAULTS)
        self.assertEqual(launcher.INI_DEFAULTS['fg_backend'], 'dlss')
        self.assertIn('reflex', launcher.INI_DEFAULTS)
        self.assertEqual(launcher.INI_DEFAULTS['reflex'], 'on')

    def test_fg_backend_options(self):
        backend_keys = [k for k, _ in launcher.FG_BACKENDS]
        self.assertEqual(backend_keys, ['dlss', 'fsr', 'fsr401'])

    def test_dlss_and_fsr_fg_modes(self):
        dlss_modes = [k for k, _ in launcher.FG_MODES_DLSS]
        fsr_modes = [k for k, _ in launcher.FG_MODES_FSR]
        self.assertEqual(dlss_modes, ['off', '2x', '3x', '4x'])
        self.assertEqual(fsr_modes, ['off', '2x'])

    def test_reflex_modes(self):
        reflex_keys = [k for k, _ in launcher.REFLEX_MODES]
        self.assertEqual(reflex_keys, ['off', 'on', 'boost'])

    def mode_choice(self, backend, mode):
        window = launcher.Launcher.__new__(launcher.Launcher)
        vars_dict = {'fg_backend': Variable(backend), 'frame_generation': Variable(mode)}
        window.var = lambda key, _store: vars_dict[key]
        window.ttk = SimpleNamespace(Combobox=Combobox)
        return window.fg_mode_choice(None), vars_dict

    def test_modes_follow_the_backend(self):
        # Created as FSR (two choices), the production choice exposes DLSS's 3x/4x after a switch.
        box, vars_dict = self.mode_choice('fsr', 'off')
        self.assertEqual(len(box.options['values']), 2)
        vars_dict['fg_backend'].set('dlss')
        self.assertEqual(len(box.options['values']), 4)
        box.current(3)
        box.events['<<ComboboxSelected>>'](None)
        self.assertEqual(vars_dict['frame_generation'].get(), '4x')
        self.assertEqual(box.current(), 3)

    def test_2x_backends_clamp_the_factor(self):
        # FSR 3.1 and AMD ML-FG make one frame: 3x or 4x fall back to 2x, Off stays Off.
        for backend in ('fsr', 'fsr401'):
            with self.subTest(backend=backend):
                box, vars_dict = self.mode_choice('dlss', '4x')
                vars_dict['fg_backend'].set(backend)
                self.assertEqual(vars_dict['frame_generation'].get(), '2x')
                self.assertEqual(box.current(), 1)
                vars_dict['frame_generation'].set('off')
                vars_dict['fg_backend'].set('dlss')
                self.assertEqual(vars_dict['frame_generation'].get(), 'off')

    def test_game_environment_passes_fg_and_reflex(self):
        app_settings = dict(launcher.APP_DEFAULTS)
        ini_settings = {
            'frame_generation': '3x',
            'fg_backend': 'dlss',
            'reflex': 'boost',
            'output_res': '2560x1440'
        }
        env = launcher.game_environment(app_settings, ini_settings)
        self.assertEqual(env.get('BB_FRAME_GENERATION'), '3x')
        self.assertEqual(env.get('BB_FG_BACKEND'), 'dlss')
        self.assertEqual(env.get('BB_REFLEX'), 'boost')
        self.assertEqual(env.get('BB_OUTPUT_RES'), '2560x1440')

    def test_game_environment_respects_existing_os_env(self):
        app_settings = dict(launcher.APP_DEFAULTS)
        ini_settings = {
            'frame_generation': '2x',
            'fg_backend': 'fsr',
            'reflex': 'on'
        }
        old_val = os.environ.get('BB_FRAME_GENERATION')
        try:
            os.environ['BB_FRAME_GENERATION'] = '4x'
            env = launcher.game_environment(app_settings, ini_settings)
            self.assertEqual(env.get('BB_FRAME_GENERATION'), '4x')
            self.assertEqual(env.get('BB_FG_BACKEND'), 'fsr')
        finally:
            if old_val is None:
                os.environ.pop('BB_FRAME_GENERATION', None)
            else:
                os.environ['BB_FRAME_GENERATION'] = old_val


if __name__ == '__main__':
    unittest.main()
