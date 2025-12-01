#!/usr/bin/env python3
"""
Detect if running within VS Code environment.
Returns exit code 0 if VS Code detected, 1 otherwise.
"""

import os
import sys
from pathlib import Path


def check_environment_variables() -> bool:
    """Check for VS Code-specific environment variables."""
    vscode_indicators = [
        "VSCODE_PID",
        "VSCODE_IPC_HOOK",
        "VSCODE_IPC_HOOK_CLI",
        "VSCODE_GIT_ASKPASS_NODE",
        "VSCODE_GIT_ASKPASS_MAIN",
    ]

    for var in vscode_indicators:
        if os.environ.get(var):
            return True

    # Check TERM_PROGRAM for vscode
    if os.environ.get("TERM_PROGRAM") == "vscode":
        return True

    return False


def check_vscode_directory(workspace_root: Path) -> bool:
    """Check if .vscode directory exists in workspace."""
    vscode_dir = workspace_root / ".vscode"
    return vscode_dir.exists() and vscode_dir.is_dir()


def check_parent_process() -> bool:
    """Check if parent process is VS Code using psutil (if available)."""
    try:
        import psutil

        current_process = psutil.Process()
        parent = current_process.parent()

        # Traverse up to 5 levels of parent processes
        for _ in range(5):
            if parent is None:
                break

            # VS Code process names across platforms
            vscode_names = [
                "code",
                "code.exe",
                "Code",
                "code-insiders",
                "code-insiders.exe",
            ]
            if parent.name() in vscode_names:
                return True

            parent = parent.parent()

    except ImportError:
        # psutil not available, skip this check
        pass
    except Exception:
        # Any other error, skip this check
        pass

    return False


def is_vscode_environment(workspace_root: Path = None) -> bool:
    """
    Detect if running within VS Code environment.

    Args:
        workspace_root: Path to workspace root (for .vscode check)

    Returns:
        True if VS Code detected, False otherwise
    """
    # Check environment variables (most reliable)
    if check_environment_variables():
        return True

    # Check parent process tree (requires psutil)
    if check_parent_process():
        return True

    # Check for .vscode directory (weakest indicator, but useful)
    if workspace_root and check_vscode_directory(workspace_root):
        return True

    return False


def main():
    """Main entry point."""
    # Try to get workspace root from command line
    workspace_root = None
    if len(sys.argv) > 1:
        workspace_root = Path(sys.argv[1]).resolve()

    # Detect VS Code
    detected = is_vscode_environment(workspace_root)

    # Return exit code: 0 = detected, 1 = not detected
    sys.exit(0 if detected else 1)


if __name__ == "__main__":
    main()
