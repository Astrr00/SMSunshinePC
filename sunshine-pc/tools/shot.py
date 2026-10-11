import subprocess,os,sys,time
tree,frame,png=sys.argv[1],sys.argv[2],sys.argv[3]
extra=dict(a.split('=',1) for a in sys.argv[4:])
bld=os.path.abspath("build/"+tree)
disc=os.path.abspath("../sms/orig/GMSJ01/Super Mario Sunshine (Japan).rvz")
env=dict(os.environ); env.update(SUNSHINE_DEV_SKIP_MOVIES="1",SUNSHINE_WINDOW_POS="30000,30000",SDL_WINDOW_ACTIVATE_WHEN_SHOWN="0",SUNSHINE_WINDOW_SIZE="640,480",AURORA_SCREENSHOT=f"{frame}:{os.path.abspath(png)}")
env.setdefault("SUNSHINE_DEV_MAX_SECONDS","900")
env.setdefault("SUNSHINE_MUTE","1")
env.update(extra)
log=open(os.path.join(bld,"shot.log"),"w")
p=subprocess.Popen([os.path.join(bld,"sunshine.exe"),"--dvd",disc],cwd=bld,stdout=log,stderr=subprocess.STDOUT,env=env,creationflags=0x08000000|0x4000)
open(os.path.join(bld,"shot.pid"),"w").write(str(p.pid)); print(p.pid)
