#!/usr/bin/env python3
"""Run through compute_desktop_launch in the milkrunner workspace, never as root."""
import json, os, signal, subprocess, sys, time
from pathlib import Path
mode=sys.argv[1] if len(sys.argv)>1 else 'file'
if mode not in ('file','system'): raise SystemExit('Expected file or system')
root=Path.cwd(); evidence=root/'evidence';evidence.mkdir(exist_ok=True)
media=Path('/var/lib/mycelium-compute/media/Rave Nine - Man or Machine (Original Mix).mp3')
presets=sorted((root/'test-presets').glob('*.milk'))
preset=next((p for p in presets if 'Confetti' in p.name),presets[0])
status=evidence/f'audio-{mode}.json';history=evidence/f'audio-{mode}-history.json'
seconds=70 if mode=='file' else 25
binary=root/'build/milk-runner-visualizer-audio'
if not binary.exists(): binary=root/'build/milk-runner-visualizer'
env=os.environ.copy();env.update(LD_LIBRARY_PATH=str(root/'external/projectm-install/lib'),LIBGL_ALWAYS_SOFTWARE='1',QSG_RENDER_LOOP='basic',QSG_NO_VSYNC='1',QT_QPA_PLATFORM='xcb')
# File test deliberately disables the audio server to establish independence.
if mode=='file': env['PULSE_SERVER']='unix:/nonexistent/mycelium-file-test'
logs=[];children=[];records=[];started=time.monotonic()
def stop(*args):
    for p in children:
        if p.poll() is None:p.terminate()
    for p in children:
        try:p.wait(5)
        except subprocess.TimeoutExpired:p.kill();p.wait()
    if args:raise SystemExit(143)
signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
try:
    if mode=='system':
        log=open(evidence/'audio-player.log','w');logs.append(log)
        player=subprocess.Popen(['ffmpeg','-v','error','-re','-stream_loop','-1','-i',str(media),'-vn','-f','pulse','mycelium'],env=env,stdout=log,stderr=log);children.append(player)
    log=open(evidence/f'audio-{mode}.log','w');logs.append(log)
    cmd=[str(binary),'--preset',str(preset),'--preset-interval','120','--automation-status',str(status),'--run-seconds',str(seconds)]
    cmd+=['--audio-file',str(media),'--audio-loop'] if mode=='file' else ['--audio-source','system','--audio-device','default']
    proc=subprocess.Popen(cmd,env=env,stdout=log,stderr=log);children.append(proc)
    while proc.poll() is None:
        try:record=json.loads(status.read_text());record['observed_elapsed_seconds']=time.monotonic()-started;records.append(record)
        except (OSError,json.JSONDecodeError):pass
        time.sleep(1)
    try:records.append(json.loads(status.read_text()))
    except (OSError,json.JSONDecodeError):pass
    tmp=history.with_suffix('.tmp');tmp.write_text(json.dumps({'mode':mode,'elapsed_seconds':time.monotonic()-started,'exit_code':proc.returncode,'records':records},indent=2));os.chmod(tmp,0o640);tmp.replace(history)
    print(json.dumps({'mode':mode,'exit_code':proc.returncode,'elapsed_seconds':time.monotonic()-started,'evidence':str(history)}))
    raise SystemExit(proc.returncode)
finally:
    stop()
    for log in logs:log.close()
