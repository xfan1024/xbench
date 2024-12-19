import os
import subprocess
import common
import exception

script_dir = os.path.dirname(os.path.realpath(__file__))

class CpuTest:
    def __init__(self):
        self.test_result = None

    def test_type(self):
        return "cpu"

    def run(self):
        subtest_list = ['PRIME', 'FIB', 'XORSHIFT', 'SORT-I32', 'SORT-U64', 'CIRCLE', 'FPMAT-MUL', 'FPMAT-CONV']
        subtest_info = {
            'PRIME':        {'weight': 1},
            'FIB':          {'weight': 1},
            'XORSHIFT':     {'weight': 1},
            'SORT-I32':     {'weight': 1},
            'SORT-U64':     {'weight': 1},
            'CIRCLE':       {'weight': 1},
            'FPMAT-MUL':    {'weight': 1},
            'FPMAT-CONV':   {'weight': 1},
        }
        score_factor = 4
        cmdtest_tmpl = f"xb-cputest -T <THREAD> -t 1 -q " + ' '.join(subtest_list)
        return common.run_multithread_test('cputest', cmdtest_tmpl, subtest_list, subtest_info, score_factor)
