# Prints the build job count: 4 while the user is active, 14 after 5 min without keyboard/mouse input.
import ctypes
class L(ctypes.Structure): _fields_ = [("s", ctypes.c_uint), ("t", ctypes.c_uint)]
i = L(8); ctypes.windll.user32.GetLastInputInfo(ctypes.byref(i))
print(14 if ctypes.windll.kernel32.GetTickCount() - i.t > 300000 else 4)
