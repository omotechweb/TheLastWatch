# Bootstrap for run_python.ps1 -Editor: runs the script named in TLW_SCRIPT once the editor
# has finished starting up, then closes the editor.

import os
import traceback
import unreal

_handle = None


def _run(delta):
    global _handle
    unreal.unregister_slate_post_tick_callback(_handle)
    script = os.environ.get("TLW_SCRIPT", "")
    try:
        with open(script, encoding="utf-8") as f:
            code = compile(f.read(), script, "exec")
        exec(code, {"__name__": "__main__", "__file__": script})
        unreal.log("TLW script finished: " + script)
    except Exception:
        unreal.log_error("TLW script failed:\n" + traceback.format_exc())
    unreal.SystemLibrary.quit_editor()


_handle = unreal.register_slate_post_tick_callback(_run)
