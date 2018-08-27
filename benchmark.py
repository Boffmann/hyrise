import os

from datetime import datetime

import subprocess
from subprocess import run

import requests
import urllib

def bot_sendtext(bot_message):
    encoded_message = urllib.parse.quote_plus(bot_message)
    bot_token = '606667730:AAHrpRwxIYfhhV8GrbYHGgKvGqSBVjHMZMw'
    bot_chatID = '5505853'
    send_text = 'https://api.telegram.org/bot' + bot_token + '/sendMessage?chat_id=' + bot_chatID + '&parse_mode=Markdown&text=' + encoded_message

    requests.get(send_text)

CWD = os.getcwd()
RESULT_DIR_BASE = 'results'
EXECUTABLE = 'build-release/numaBenchmarkTPCH'
STATUS_FILE = 'current_run.status'
OUTPUT_FILE = 'current_run.out'
NAME_FILE = 'current_run.name'

SCALE = 1
# SCALE = 0.1
QUERY_RUNS_PER_CORE = 3
# ITERATIONS_OVERALL = 3
ITERATIONS_OVERALL = 3
# CHUNK_SIZE = 100000
CHUNK_SIZE_TXT = ''
CHUNK_SIZE_TXT = '_chunksizeMAX'
# CHUNK_SIZE_TXT = '_chunksize' + CHUNK_SIZE

APPENDIX = ''
timestamp = datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
RESULT_DIR_NAME = 'tpch_scale' + str(SCALE) + CHUNK_SIZE_TXT + '_runspercore' + str(QUERY_RUNS_PER_CORE) + APPENDIX + '_' + timestamp
RESULT_DIR = os.path.join(RESULT_DIR_BASE, RESULT_DIR_NAME)

# core_counts = list(range(80, 0, -5)) + [4, 3, 2, 1, 0]
# core_counts = list(range(224, 0, -14)) + [7, 1, 0]
core_counts = list(range(224, 0, -28)) + [7, 1, 0]
# core_counts = list(range(20, -1, -1))
# core_counts = [10]

# [print(c) for c in core_counts]
# for cc in core_counts:
#     runs_per_query = QUERY_RUNS_PER_CORE * int(cc)
#     if runs_per_query == 0:
#         runs_per_query = QUERY_RUNS_PER_CORE
#     print(cc, runs_per_query)
# exit(0)

if not os.path.exists(RESULT_DIR_BASE):
    os.makedirs(RESULT_DIR_BASE)
if not os.path.exists(RESULT_DIR):
    os.makedirs(RESULT_DIR)

if os.path.exists(STATUS_FILE):
    os.remove(STATUS_FILE)
if os.path.exists(OUTPUT_FILE):
    os.remove(OUTPUT_FILE)
if os.path.exists(NAME_FILE):
    os.remove(NAME_FILE)

bot_sendtext('Starting benchmarks...')

for iteration in range(ITERATIONS_OVERALL):
    for core_count in core_counts:
        with open(STATUS_FILE, 'a+') as status:
            status.write('Running iteration ' + str(iteration) + ', core count ' + str(core_count) + '...\n')
        if core_count == 0:
            use_scheduler = 'false'
            runs_per_query = QUERY_RUNS_PER_CORE
        else:
            use_scheduler = 'true'
            runs_per_query = QUERY_RUNS_PER_CORE * core_count

        result_file = os.path.join(RESULT_DIR, str(core_count) + 'cores-' + str(iteration) + '.json')
        with open(OUTPUT_FILE, 'a+') as output:
            args = [
                EXECUTABLE,
                '-v',
                '-o', result_file,
                '-s', str(SCALE),
                '--runs', str(runs_per_query),
                '--scheduler=' + use_scheduler,
                '--cores', str(core_count),
                # '--chunk_size', str(CHUNK_SIZE),
                # '--pcm',
                # '-q', '6', '-q', '13'
                # '-q', '1', '-q', '3', '-q', '6', '-q', '7', '-q', '10', '-q', '13',
                # '-q', '1', '-q', '3', '-q', '5', '-q', '6', '-q', '7', '-q', '9', '-q', '10', '-q', '13',
                '-q', '1', '-q', '2', '-q', '3', '-q', '4', '-q', '5', '-q', '6', '-q', '7', '-q', '8', '-q', '9', '-q', '10', '-q', '11', '-q', '12', '-q', '13', '-q', '14', '-q', '16', '-q', '17', '-q', '18', '-q', '19', '-q', '21', '-q', '22',
                ]
            run(args, cwd=CWD, stdout=output, stderr=output)
            # run(args, cwd=CWD)
    bot_sendtext('Iteration ' + str(iteration + 1) + ' of ' + str(ITERATIONS_OVERALL) + ' complete!')

with open(STATUS_FILE, 'a+') as status:
    status.write('Done\n')
with open(NAME_FILE, 'w') as name_file:
    name_file.write(RESULT_DIR + '\n')

bot_sendtext('Benchmarks complete!')
