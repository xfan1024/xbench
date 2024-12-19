import os
import subprocess
import logging
from collections import OrderedDict

def get_nthread_weight(nthread):
    weight = {
        1: 4,
        2: 3,
        4: 2,
        'all': 2,
    }
    return weight[nthread]

def weighted_average(values, subtest_info):
    score = 0
    subtest_weight_sum = 0
    for subtest in values:
        thread_score = 0
        thread_weight_sum = 0
        for subtest_value in subtest['values']:
            thread = subtest_value['thread']
            value = subtest_value['value']
            thread_weight = get_nthread_weight(thread)
            thread_weight_sum += thread_weight
            thread_score += value * thread_weight
        thread_score /= thread_weight_sum
        score += thread_score * subtest_info[subtest['name']]['weight']
        subtest_weight_sum += subtest_info[subtest['name']]['weight']
    score /= subtest_weight_sum
    return score


def get_interested_nthread_list():
    return [1, 2, 4]
    # return [1]

def get_nthread_list_to_test():
    res = get_interested_nthread_list()
    ncpu = os.cpu_count()
    if ncpu not in res:
        res.append(ncpu)
    half_ncpu = ncpu // 2
    if half_ncpu > 0 and half_ncpu not in res:
        res.append(half_ncpu)
    res.sort()
    return res
    # return [1, os.cpu_count()]

def get_available_memory():
    cg_avail = None
    try:
        with open("/sys/fs/cgroup/memory/memory.limit_in_bytes", "r") as f:
            limit = int(f.read())
        with open("/sys/fs/cgroup/memory/memory.usage_in_bytes", "r") as f:
            usage = int(f.read())
        cg_avail = limit - usage
    except:
        pass
    # consider available memory
    with open("/proc/meminfo", "r") as f:
        for line in f:
            if "MemAvailable" in line:
                mem_avail = int(line.split()[1]) * 1024
                if cg_avail is not None:
                    return min(mem_avail, cg_avail)
                else:
                    return mem_avail
    assert False, "Failed to get available memory"

def get_suggested_memory_use_gb():
    avail = get_available_memory()
    gb_avail = int(get_available_memory() * 0.5 / 1024 / 1024 / 1024)
    if gb_avail > 16:
        return 16
    if gb_avail < 1:
        if avail >= 1200 * 1024 * 1024:
            logging.warning("Too little memory available, be careful using test results")
            return 1
        logging.error("Too little memory available, cannot do this test")
    return gb_avail

def result_convert(thread_results, subtest_list, subtest_info):
    subtest_results = []
    all_cpu = os.cpu_count()
    half_cpu = all_cpu // 2

    for subtest in subtest_list:
        half_value = None
        all_value = None
        best_value_in_all_and_half = None
        # unit = subtest_info[subtest]['unit']
        subtest_values = []
        for thread_result in thread_results:
            thread = thread_result['thread']
            value = thread_result[subtest]
            if thread_result['thread'] in get_interested_nthread_list():
                subtest_values.append({
                    'thread': thread,
                    'value': value,
                })
            if thread_result['thread'] == all_cpu:
                all_value = value
            if thread_result['thread'] == half_cpu:
                half_value = value

        if half_value is None:
            best_value_in_all_and_half = all_value
        else:
            best_value_in_all_and_half = max(half_value, all_value)
        subtest_values.append({
            'thread': 'all',
            'value': best_value_in_all_and_half
        })

        subtest_result = {
            'name': subtest,
            # 'unit': unit,
            'values': subtest_values
        }
        subtest_results.append(subtest_result)
    return subtest_results

def popen(command):
    logging.debug(f"Running command: {command}")
    if isinstance(command, str):
        return subprocess.Popen(command, shell=True, stdout=subprocess.PIPE)
    elif isinstance(command, list):
        return subprocess.Popen(command, stdout=subprocess.PIPE)

def shell(command):
    logging.debug(f"Running command: {command}")
    try:
        if isinstance(command, str):
            result = subprocess.run(command, shell=True, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        elif isinstance(command, list):
            result = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return result.stdout.decode('utf-8')
    except subprocess.CalledProcessError as e:
        # logging.error(f"Command failed with error: {e}")
        return None

def get_system_info():
    uname_info = os.uname()
    kernel_version = uname_info.release
    distro = 'unknown'
    with open("/etc/os-release", "r") as f:
        for line in f:
            if "PRETTY_NAME" in line:
                distro = line.split("=")[1].strip().strip('"')
                break
    cpu_model = shell('cat /proc/cpuinfo  | grep "model name" | sed -n 1p | cut -d: -f2')
    cores = os.cpu_count()
    if cpu_model:
        cpu_model = cpu_model.strip()
    else:
        cpu_model = f'unknown({uname_info.machine})'
    cores = os.cpu_count()
    return OrderedDict([
        ('cpu', cpu_model),
        ('cores', cores),
        ('kernel', kernel_version),
        ('distro', distro),
    ])

def run_testcmd(testcmd):
    proc = popen(testcmd)
    row = {}
    for line in proc.stdout:
        splited = line.decode().split()
        name = splited[0]
        value = float(splited[1])
        row[name] = value
    proc.wait()
    return row

def run_multithread_test(name, testcmd_tmpl, subtest_list, subtest_info, score_factor):
    thread_list = get_nthread_list_to_test()
    thread_results = []
    test_gb = get_suggested_memory_use_gb()
    if test_gb == 0:
        raise exception.SkipException("Not enough memory to run memtest")

    for nthread in thread_list:
        testcmd = testcmd_tmpl.replace("<THREAD>", str(nthread))
        row = run_testcmd(testcmd)
        row['thread'] = nthread
        thread_results.append(row)

    result = result_convert(thread_results, subtest_list, subtest_info)
    score = weighted_average(result, subtest_info) * score_factor
    return OrderedDict([
        ('name', name),
        ('score', int(round(score))),
        ('subtests', result),
    ])
