"""Synthetic regressions for combined #87/#75/#90; no game image required."""
from paths import ROOT
import importlib.util
import contextlib
import io
import os
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import patches


class StabilityIntegrationTests(unittest.TestCase):
    def test_frame_heap_fix_checks_both_instructions_and_accepts_already_applied(self):
        words={0x2195c0a:b'\xba\x00\x00\x04\x00',
               0x2195c47:b'\xba\x00\x00\x04\x00'}
        def read(start,end):
            for at,data in words.items():
                if at-1<=start and end<=at+4:
                    return data[start-at+1:end-at+1]
            return b''
        self.assertTrue(patches.frame_heaps_compatible(read))
        words[0x2195c47]=b'\xba\x00\x00\x10\x00'
        self.assertTrue(patches.frame_heaps_compatible(read))
        words[0x2195c47]=b'\xb9\x00\x00\x04\x00'
        self.assertFalse(patches.frame_heaps_compatible(read))
        words[0x2195c47]=b'\xba\x00\x00\x08\x00'
        self.assertFalse(patches.frame_heaps_compatible(read))

    def test_patch_group_change_only_disables_that_group(self):
        # Compute expected fingerprints from synthetic zero memory using the real XML sites.
        # This verifies grouping/isolation without pretending to validate the retail hashes.
        changes={}
        def read(start,end):
            return bytes(changes.get(at,0) for at in range(start,end))
        expected={group:patches.fingerprint(read,names=names)
                  for group,names in patches.PATCH_GROUPS.items()}
        with patch.dict(patches.PATCH_FINGERPRINTS,{'01.09':expected}):
            self.assertEqual(patches.matching_groups_of(read),set(patches.PATCH_GROUPS))
            for group,names in patches.PATCH_GROUPS.items():
                with self.subTest(group=group):
                    changes.clear()
                    site=patches.patch_sites(patches.BUILTIN_XML,'01.09',names)[0][0]
                    changes[site]=1
                    self.assertEqual(patches.matching_groups_of(read),set(patches.PATCH_GROUPS)-{group})

    def test_auto_live_resolution_dof_table(self):
        spec=importlib.util.spec_from_file_location('stability_run',ROOT/'run.py')
        module=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as folder:
            config=Path(folder)/'bbport.ini'
            for output,dof,expected in [('3840x2160','',True),('3840x2160','1',True),
                                       ('3840x2160','0',False),('1920x1080','1',False),
                                       ('2560x1080','1',True),('1280x720','1',False)]:
                with self.subTest(output=output,dof=dof):
                    config.write_text('effect_dof='+dof+'\n' if dof else '')
                    self.assertEqual(module.auto_live_blurs_dof(output,config),expected)

    def test_partial_fps_executable_keeps_output_patches_and_guarded_heap_fix(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            elf=bytearray(128)
            struct.pack_into('<Q',elf,0x20,64)
            struct.pack_into('<HH',elf,0x36,56,1)
            struct.pack_into('<IIQQQQQQ',elf,64,1,5,0,0,0,0,0x6000000,0)
            (root/'eboot.elf').write_bytes(elf)
            config=root/'bbport.ini'
            config.write_text('output_res=5120x1440\nupscaler=fsr4\npreset=0\nquit_to_desktop=0\n')
            argv=['patches.py','--out',str(root),'--game-dir',str(root),'--settings',str(config),
                  '--fps','60','--render-res','5120x1440','--output-res','5120x1440',
                  '--extra','Restore Debug Camera']
            with patch.object(sys,'argv',argv), patch.dict(os.environ,{'BB_FORCE_PATCHES':''}), \
                 patch.object(patches,'matching_groups',return_value={'resolution','effects'}), \
                 patch.object(patches,'frame_heaps_compatible',return_value=True), \
                 contextlib.redirect_stdout(io.StringIO()):
                patches.main()
            blob=(root/'patches.bin').read_bytes()
            count=struct.unpack_from('<Q',blob,16)[0]
            writes={}
            pos=24
            for _ in range(count):
                at,size=struct.unpack_from('<QQ',blob,pos)
                pos+=16
                writes[at]=blob[pos:pos+size]
                pos+=size
            self.assertEqual(writes[0x2195c0a],b'\x00\x00\x10\x00')
            self.assertEqual(writes[0x2195c47],b'\x00\x00\x10\x00')
            self.assertEqual(writes[patches.UI_WIDTH],b'\xb8'+(5120).to_bytes(3,'little'))
            self.assertEqual(writes[patches.UI_HEIGHT],b'\xb9'+(1440).to_bytes(3,'little'))
            forbidden=patches.compile_patches(patches.BUILTIN_XML,['60 FPS++','Restore Debug Camera'],
                                              '01.09',[(0,0x6000000)])
            self.assertTrue(all(at not in writes for at,_data in forbidden))
