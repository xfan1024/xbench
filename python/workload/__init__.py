from workload.memtest import MemTest
from workload.cputest import CpuTest
from workload.openssltest import OpensslTest

ram_test_list       = [MemTest]
cpu_test_list       = []
disk_test_list      = []
combined_test_list  = []

all_test = [
    {'name': 'cpu',         'test_list': cpu_test_list},        # prime pi
    {'name': 'ram',         'test_list': ram_test_list},        # memtest
    {'name': 'disk',        'test_list': disk_test_list},       # fio emptyfile
    {'name': 'combined',    'test_list': combined_test_list},   # build database procsh
]
