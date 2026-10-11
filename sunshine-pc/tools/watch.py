# Launch the game (low priority, detached) with a live monitor image: build/live.png is rewritten every N presented
# frames from Aurora's offscreen present, independent of window stacking. Usage: watch.py <tree> [KEY=VAL ...]
import subprocess,os,sys
tree=sys.argv[1]
extra=dict(a.split('=',1) for a in sys.argv[2:])
bld=os.path.abspath("build/"+tree)
disc=os.path.abspath("../sms/orig/GMSJ01/Super Mario Sunshine (Japan).rvz")
env=dict(os.environ); env.update(SUNSHINE_WINDOW_POS="30000,30000",SDL_WINDOW_ACTIVATE_WHEN_SHOWN="0",SUNSHINE_WINDOW_SIZE="640,480",AURORA_SCREENSHOT_EVERY="30:"+os.path.abspath("build/live.png"))
env.setdefault("SUNSHINE_DEV_MAX_SECONDS","900")
env.setdefault("SUNSHINE_MUTE","1")
env.update(extra)
log=open(os.path.join(bld,"watch.log"),"w")
p=subprocess.Popen([os.path.join(bld,"sunshine.exe"),"--dvd",disc],cwd=bld,stdout=log,stderr=subprocess.STDOUT,env=env,creationflags=0x08000000|0x4000)
open("build/watch.pid","w").write(str(p.pid)); print(p.pid)
