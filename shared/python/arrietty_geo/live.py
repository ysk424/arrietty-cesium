"""Exclusive hardware ownership across row and fly, released on process exit."""
from contextlib import contextmanager
from .common import PlaceError


@contextmanager
def live_session(enabled):
    if not enabled:
        yield
        return
    import ctypes
    from ctypes import wintypes
    api=ctypes.WinDLL('kernel32',use_last_error=True)
    api.CreateMutexW.argtypes=[wintypes.LPVOID,wintypes.BOOL,wintypes.LPCWSTR]
    api.CreateMutexW.restype=wintypes.HANDLE
    api.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD]
    api.ReleaseMutex.argtypes=[wintypes.HANDLE]
    api.CloseHandle.argtypes=[wintypes.HANDLE]
    handle=api.CreateMutexW(None,False,'Local\\ArriettyCesiumLiveHardware')
    if not handle: raise PlaceError('機器の利用状態を確認できませんでした。')
    owned=False
    try:
        owned=api.WaitForSingleObject(handle,0) in (0,0x80)
        if not owned: raise PlaceError('row または fly が実機を使用中です。先に終了してください。')
        yield
    finally:
        if owned: api.ReleaseMutex(handle)
        api.CloseHandle(handle)
