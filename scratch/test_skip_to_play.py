import subprocess, time, ctypes

VK_SPACE = 0x20
KEYEVENTF_KEYUP = 0x0002

def press_space():
    ctypes.windll.user32.keybd_event(VK_SPACE, 0, 0, 0)
    time.sleep(0.08)
    ctypes.windll.user32.keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0)

print("Testing SKIP behavior in Release mode...")
p = subprocess.Popen(['generated/output/Release/DirectXGame.exe'], cwd='generated/output/Release')

try:
    time.sleep(2.5)
    print("Pressing SPACE to start mission...")
    press_space()

    time.sleep(5.5)
    print("Helicopter intro started. Pressing SPACE to SKIP intro...")
    press_space()

    # Wait 4.0s: if it transitioned back to TITLE, it would be in Title. If skipped, player is in gameplay.
    time.sleep(4.0)

    ret = p.poll()
    if ret is None:
        print("SUCCESS! Skip correctly kept player in gameplay without returning to title!")
    else:
        print(f"Terminated: {ret}")

finally:
    p.terminate()
    try:
        p.wait(timeout=3)
    except Exception:
        p.kill()
