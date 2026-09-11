"""Exercise Windows key messages through the actual UE/Slate input path.

Starts an isolated offline game; never sends input to an existing live session.
This verifies standard keypad messages, not a physical keypad's USB reports.
"""
import argparse
import ctypes
from ctypes import wintypes
from pathlib import Path
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', default='C:/Program Files/Epic Games/UE_5.8')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    log = repo.parents[1] / 'logs/row' / f'keypad-{time.time_ns()}.log'
    log.parent.mkdir(exist_ok=True, parents=True)
    user = ctypes.WinDLL('user32', use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user.SetForegroundWindow.argtypes = [wintypes.HWND]
    user.GetForegroundWindow.restype = wintypes.HWND
    user.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
    user.AttachThreadInput.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.BOOL]
    kernel = ctypes.WinDLL('kernel32')
    kernel.GetCurrentThreadId.restype = wintypes.DWORD
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([
        str(Path(args.engine) / 'Engine/Binaries/Win64/UnrealEditor.exe'),
        str(repo / 'unreal/ArriettyCesium/ArriettyCesium.uproject'),
        '/Game/Row/Maps/CesiumRow', '-game','-RowTestFixture', '-RowOffline', '-nohmd','-DisablePlugins=OpenXR',
        '-nosplash', '-nosound', '-windowed', '-ResX=1000', '-ResY=700',
        '-RowQuitAfter=180', f'-abslog={log}',
        '-ExecCmds=t.MaxFPS 60,t.IdleWhenNotForeground 0',
    ], startupinfo=startup)
    print(f'Offline input test started (PID {process.pid})', flush=True)
    handle = None

    def read_log():
        return log.read_text(encoding='utf-8-sig', errors='replace') if log.exists() else ''

    def wait_until(predicate, seconds=10):
        until = time.monotonic() + seconds
        while time.monotonic() < until:
            if predicate():
                return
            if process.poll() is not None:
                raise RuntimeError(f'UE exited early ({process.returncode}); {log}')
            time.sleep(.1)
        raise TimeoutError(f'Input test timed out; {log}')

    def find_window():
        nonlocal handle
        @callback_type
        def visit(window, _):
            nonlocal handle
            pid = wintypes.DWORD()
            user.GetWindowThreadProcessId(window, ctypes.byref(pid))
            title = ctypes.create_unicode_buffer(512)
            user.GetWindowTextW(window, title, len(title))
            window_class = ctypes.create_unicode_buffer(256)
            user.GetClassNameW(window, window_class, len(window_class))
            if pid.value == process.pid and window_class.value == 'UnrealWindow':
                handle = window
                return False
            return True
        user.EnumWindows(visit, 0)
        return handle is not None

    def post(message, key, bits):
        if not user.PostMessageW(handle, message, key, bits):
            raise ctypes.WinError(ctypes.get_last_error())

    expected = []
    presses = 0

    def activate_test_window():
        # This is an interactive Windows-message integration test. Temporarily
        # share focus state with the foreground thread, then detach immediately;
        # otherwise Windows can deny activation after the user visits chat.
        current = kernel.GetCurrentThreadId()
        foreground = user.GetWindowThreadProcessId(user.GetForegroundWindow(), None)
        attached = bool(foreground and foreground != current and user.AttachThreadInput(current, foreground, True))
        try:
            user.ShowWindow(handle, 9)
            user.SetForegroundWindow(handle)
        finally:
            if attached:
                user.AttachThreadInput(current, foreground, False)
        wait_until(lambda: user.GetForegroundWindow() == handle)
        time.sleep(.1)

    def press(label, key, scan, extended, action):
        nonlocal presses
        # Tool/chat interaction may move focus during this longer setup test.
        # Restore only our own test window before sending each targeted message.
        activate_test_window()
        bits = 1 | (scan << 16) | (int(extended) << 24)
        post(0x100, key, bits)
        # A held key must not repeatedly toggle or stop the session.
        for _ in range(3):
            post(0x100, key, bits | (1 << 30))
        post(0x101, key, bits | (1 << 30) | (1 << 31))
        presses += 1
        expected.extend(['calibration_begin', 'start'] if action == 'start' else [action])
        wait_until(lambda: read_log().count('ROW_CONTROL action=') >= len(expected), 25)
        time.sleep(.3)
        actual = [line.split('ROW_CONTROL action=')[1].split()[0]
                  for line in read_log().splitlines() if 'ROW_CONTROL action=' in line]
        assert actual == expected, (label, expected, actual)
        assert read_log().count('route=preprocessor') == presses, 'Input bypassed keypad router'
        print(f'PASS {label}: {action}, repeats ignored', flush=True)

    try:
        wait_until(lambda: 'ROW_READY' in read_log(), 60)
        wait_until(find_window, 30)
        activate_test_window()
        time.sleep(5)  # Let initial streaming settle before starting the model.
        press('keypad Enter starts setup', 0x0D, 0x1C, True, 'calibration_begin')
        press('keypad Enter keeps setup running', 0x0D, 0x1C, True, 'calibration_continue')
        press('numpad 0 cancels setup', 0x60, 0x52, False, 'stop_home')
        press('keypad Enter (extended)', 0x0D, 0x1C, True, 'start')
        press('keypad Enter pause', 0x0D, 0x1C, True, 'pause')
        press('keypad Enter resume', 0x0D, 0x1C, True, 'start')
        press('numpad 0 / Num Lock on', 0x60, 0x52, False, 'stop_home')
        press('keypad Enter restart', 0x0D, 0x1C, True, 'start')
        press('numpad 0 / Num Lock off (Insert)', 0x2D, 0x52, False, 'stop_home')
        press('main keyboard Enter', 0x0D, 0x1C, False, 'start')
        press('main keyboard Enter pause', 0x0D, 0x1C, False, 'pause')
        press('Escape stop', 0x1B, 0x01, False, 'stop_home')
    finally:
        if handle is None:
            find_window()
        if process.poll() is None and handle:
            post(0x10, 0, 0)  # Close only the process created by this test.
        process.wait(timeout=30)
    assert process.returncode == 0, f'UE exit code {process.returncode}'
    assert 'Fatal error' not in read_log()
    print(f'PASS {presses} Windows key presses, calibration/start/resume and clean shutdown. Evidence: {log}')


if __name__ == '__main__':
    main()
