#!/usr/bin/env python3
"""Validate repository layout and Keil project references."""

from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
PROJECTS = (
    REPOSITORY_ROOT / "Master_MCU" / "Project.uvprojx",
    REPOSITORY_ROOT / "Slave_MCU" / "Project.uvprojx",
)
REQUIRED_FILES = (
    "README.md",
    "CHANGELOG.md",
    "Shared/Protocol/inter_mcu_protocol.c",
    "Shared/Protocol/inter_mcu_protocol.h",
    "Master_MCU/App/app_scheduler.c",
    "Master_MCU/App/flight_safety.c",
    "Master_MCU/BSP/board_config.h",
    "Master_MCU/BSP/control_timers.c",
    "tests/host/test_inter_mcu_protocol.c",
    "tests/host/test_flight_safety.c",
)
REMOVED_DUPLICATE_DIRECTORIES = (
    "Master_MCU/Start",
    "Master_MCU/Library",
    "Master_MCU/System",
    "Slave_MCU/Start",
    "Slave_MCU/Library",
    "Slave_MCU/System",
)


def resolve_project_path(project: Path, raw_path: str) -> Path:
    normalized = raw_path.replace("\\", "/")
    return (project.parent / normalized).resolve()


def validate_keil_project(project: Path) -> list[str]:
    errors: list[str] = []
    try:
        root = ET.parse(project).getroot()
    except (OSError, ET.ParseError) as error:
        return [f"{project.relative_to(REPOSITORY_ROOT)}: {error}"]

    for element in root.iter("FilePath"):
        raw_path = (element.text or "").strip()
        if not raw_path:
            continue
        target = resolve_project_path(project, raw_path)
        if not target.is_file():
            errors.append(
                f"{project.relative_to(REPOSITORY_ROOT)} references "
                f"missing file: {raw_path}"
            )

    include_paths: set[str] = set()
    for element in root.iter("IncludePath"):
        for raw_path in (element.text or "").split(";"):
            raw_path = raw_path.strip()
            if raw_path:
                include_paths.add(raw_path)

    for raw_path in sorted(include_paths):
        target = resolve_project_path(project, raw_path)
        if not target.is_dir():
            errors.append(
                f"{project.relative_to(REPOSITORY_ROOT)} references "
                f"missing include directory: {raw_path}"
            )

    project_text = project.read_text(encoding="utf-8")
    if r"..\Shared\Protocol\inter_mcu_protocol.c" not in project_text:
        errors.append(
            f"{project.relative_to(REPOSITORY_ROOT)} does not compile "
            "the shared inter-MCU protocol"
        )

    return errors


def main() -> int:
    errors: list[str] = []

    for relative_path in REQUIRED_FILES:
        if not (REPOSITORY_ROOT / relative_path).is_file():
            errors.append(f"required file is missing: {relative_path}")

    for relative_path in REMOVED_DUPLICATE_DIRECTORIES:
        if (REPOSITORY_ROOT / relative_path).exists():
            errors.append(f"duplicate platform directory returned: {relative_path}")

    for project in PROJECTS:
        errors.extend(validate_keil_project(project))

    if errors:
        print("Project validation failed:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print("Project validation passed:")
    print("  - required engineering files are present")
    print("  - duplicate platform trees are absent")
    print("  - both Keil project XML files are valid")
    print("  - all Keil source and include references exist")
    print("  - both firmware targets compile the shared protocol")
    return 0


if __name__ == "__main__":
    sys.exit(main())
