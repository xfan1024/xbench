import sys
import yaml
import logging
import workload
import common
from collections import OrderedDict

logging.basicConfig(level=logging.DEBUG)
sysinfo = common.get_system_info()

from workload import CpuTest
from workload import OpensslTest
from workload import MemTest
from collections import OrderedDict

cputest_test = workload.CpuTest()
cputest_result = cputest_test.run()

openssl_test = workload.OpensslTest()
openssl_result = openssl_test.run()

memtest = workload.MemTest()
memtest_result = memtest.run()

cpu_result = [cputest_result, openssl_result]
mem_result = [memtest_result]

result = OrderedDict([
    ('system', sysinfo),
    ('all_tests', [
        OrderedDict([
            ('type', 'cpu'),
            ('tests', cpu_result)
        ]),
        OrderedDict([
            ('type', 'mem'),
            ('tests', mem_result)
        ])
    ])
])

class OrderedDumper(yaml.SafeDumper):
    pass

def dict_representer(dumper, data):
    return dumper.represent_dict(data.items())

OrderedDumper.add_representer(OrderedDict, dict_representer)
print(yaml.dump(result, Dumper=OrderedDumper))

