import os
import subprocess
import common
import exception

script_dir = os.path.dirname(os.path.realpath(__file__))

class MemTest:
    def __init__(self):
        self.test_result = None

    def test_type(self):
        return "ram"

    def run(self):
        subtest_list = ['COPY', 'LOAD', 'STORE']
        subtest_info = {
            'COPY':     {'weight': 1},
            'LOAD':     {'weight': 1},
            'STORE':    {'weight': 1},
        }
        score_factor = 0.052
        test_gb = common.get_suggested_memory_use_gb()
        cmdtest_tmpl = f"xb-memtest -G {test_gb} -T <THREAD> -t 1 -q " + ' '.join(subtest_list)
        return common.run_multithread_test('memtest', cmdtest_tmpl, subtest_list, subtest_info, score_factor)
