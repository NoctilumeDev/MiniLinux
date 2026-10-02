import json, sys, time
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError

base, output = sys.argv[1].rstrip('/'), Path(sys.argv[2])
results = []

def keep(label, state, **extra):
    results.append(dict(label=label, state=state, **extra))
    output.write_text(json.dumps(results, indent=2), encoding='utf-8')

def request(path, body=None, expected=200):
    data = json.dumps(body).encode() if body is not None else None
    headers = {'Content-Type': 'application/json'} if data is not None else {}
    try:
        response = urlopen(Request(base+path, data=data, headers=headers), timeout=25)
    except HTTPError as exc:
        response = exc
    with response:
        assert response.status == expected, (path, response.status, expected)
        content = response.read()
    return json.loads(content) if path.startswith('/api/') else content

def command(text, expected):
    old = request('/api/state')
    offset, sequence = len(old['console']), old['sequence']
    request('/api/input', {'text': text+'\r'})
    deadline = time.monotonic()+25
    while True:
        state = request('/api/state')
        if 'mini$ ' in state['console'][offset:]:
            break
        assert state['status'] == 'running', state
        assert time.monotonic() < deadline, (text, state['console'][-2000:])
        time.sleep(0.05)
    new = state['console'][offset:]
    assert expected in new, (text, expected, new)
    keep(text, state, observed_output=new)
    return state, sequence

assert b'MiniLinux Console' in request('/')
initial = request('/api/state')
assert initial['status'] == 'running'
keep('initial', initial, python_executable=sys.executable)
command('help', 'help | ls | cat')
command('ls', '/hello.txt  17 bytes')
command('cat hello.txt', 'hello from ramfs')
command('ps', 'shell')
command('run hello', 'hello from user mode')
command('run reader', 'hello from ramfs')
state, seq = command('run counter-a counter-b', 'counter-b')
events = [e for e in state['events'] if e['id'] > seq]
ids = [int(e['fields'][0]) for e in events if e['kind'] == 'spawn']
assert len(ids) == 2, ids
transitions = {(int(e['fields'][0]), int(e['fields'][1])) for e in events if e['kind'] == 'schedule'}
assert (ids[0],ids[1]) in transitions and (ids[1],ids[0]) in transitions
markers = {pid:[e['fields'] for e in events if e['kind']=='data' and int(e['fields'][0])==pid and int(e['fields'][3],16)] for pid in ids}
a,b = markers[ids[0]][-1],markers[ids[1]][-1]
assert a[1]==b[1] and a[2]!=b[2] and a[3]!=b[3], (a,b)
command('run fault', 'status 142')
command('cat hello.txt', 'hello from ramfs')
command('check', 'process capacity and reclamation passed')
state,_ = command('mem', 'free')
assert {p['program'] for p in state['processes']} == {'init','shell'}
request('/api/stop', {})
state = request('/api/state')
assert state['status']=='stopped'
keep('stopped',state)
request('/api/input', {'text':'help\r'}, expected=503)
request('/api/restart', {})
fresh=request('/api/state')
assert fresh['status']=='running' and fresh['epoch']!=initial['epoch']
assert 'hello from ramfs' not in fresh['console']
assert {p['pid'] for p in fresh['processes']}=={1,2}
keep('restarted-fresh',fresh)
print('DESKTOP HTTP PASS: real commands, CPL3 counters, fault retake, check/mem, stop/restart fresh epoch')
