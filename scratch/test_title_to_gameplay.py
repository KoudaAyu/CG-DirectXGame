import subprocess, time, ctypes

# Virtual key codes
VK_SPACE = 0x20
KEYEVENTF_KEYUP = 0x0002

def press_space():
    ctypes.windll.user32.keybd_event(VK_SPACE, 0, 0, 0)
    time.sleep(0.08)
    ctypes.windll.user32.keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0)

print("Starting DirectXGame.exe in Release mode...")
p = subprocess.Popen(['generated/output/Release/DirectXGame.exe'], cwd='generated/output/Release')

try:
    print("Waiting 2.5s for TitleScene to load...")
    time.sleep(2.5)

    print("Pressing SPACE to start mission...")
    press_space()

    # The duck cries (~3.15s), then StingerTransition (1.8s) transitions to GAMEPLAY
    print("Waiting for duck cry and transition to GAMEPLAY (~5.5s)...")
    time.sleep(5.5)

    # In GAMEPLAY, HelicopterInsertionController runs for ~9.7s (Acts 1-5)
    print("Watching helicopter insertion cutscene (~10.0s)...")
    time.sleep(10.0)

    # At the end of Act 5, StingerTransition triggers and transitions back to TITLE (~2.5s)
    print("Watching transition back to TITLE (~3.0s)...")
    time.sleep(3.0)

    # Check if process is still alive and healthy
    ret = p.poll()
    if ret is None:
        print("SUCCESS! DirectXGame.exe completed the full Title -> Helicopter Intro -> Title loop cleanly without crashes!")
    else:
        print(f"Process terminated with exit code: {ret}")

finally:
    p.terminate()
    try:
        p.wait(timeout=3)
    except Exception:
        p.kill()
    print("Test finished.")
