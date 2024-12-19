import os
import subprocess
import common
import exception

script_dir = os.path.dirname(os.path.realpath(__file__))

class OpensslTest:
    def __init__(self):
        self.test_result = None

    def test_type(self):
        return "ram"

    def run(self):
        subtest_list = ['SHA1-8K', 'SHA256-8K', 'SHA512-8K', 'MD5-8K', 'SM3-8K']
        subtest_info = {
            'SHA1-8K':      {'weight': 1},
            'SHA256-8K':    {'weight': 1},
            'SHA512-8K':    {'weight': 1},
            'MD5-8K':       {'weight': 1},
            'SM3-8K':       {'weight': 1},
        }
        score_factor = 0.003
        cmdtest_tmpl = f"xb-openssl -T <THREAD> -t 1 -q " + ' '.join(subtest_list)
        return common.run_multithread_test('openssl', cmdtest_tmpl, subtest_list, subtest_info, score_factor)
