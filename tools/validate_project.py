#!/usr/bin/env python3
"""Validate repository layout and Keil project references."""

from __future__ import annotations

import re
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
    "docs/README.md",
    "docs/DEVELOP_ENGINEERING_LEARNING_GUIDE.md",
    "docs/PROJECT_STRUCTURE.md",
    "docs/ARCHITECTURE.md",
    "docs/DRIVER_API.md",
    "docs/DEVELOPMENT_GUIDE.md",
    "docs/BUILD.md",
    "docs/TESTING.md",
    "docs/PINOUT.md",
    "docs/PROTOCOL.md",
    "docs/SAFETY.md",
    "docs/MAINTENANCE.md",
    "docs/RELEASE.md",
    "docs/ROADMAP.md",
    "Shared/Protocol/inter_mcu_protocol.c",
    "Shared/Protocol/inter_mcu_protocol.h",
    "Master_MCU/Core/main.c",
    "Master_MCU/Control/attitude_estimator.c",
    "Master_MCU/Control/kalman_filter.c",
    "Master_MCU/Control/pid_controller.c",
    "Master_MCU/Drivers/Bus/soft_i2c.c",
    "Slave_MCU/Core/main.c",
    "Slave_MCU/BSP/board_inputs.c",
    "Slave_MCU/Drivers/Sensors/BMP390/bmp390.c",
    "Slave_MCU/Drivers/Sensors/BMP390/bmp3.c",
    "Slave_MCU/Drivers/Sensors/BMP390/bmp3.h",
    "Slave_MCU/Drivers/Sensors/BMP390/bmp3_defs.h",
    "Master_MCU/App/app_scheduler.c",
    "Master_MCU/App/flight_safety.c",
    "Master_MCU/BSP/board_config.h",
    "Master_MCU/BSP/control_timers.c",
    "Shared/Drivers/dma_rx.c",
    "Shared/Drivers/millisecond_clock.c",
    "Shared/Scheduling/periodic_tasks.c",
    "Slave_MCU/App/slave_app.c",
    "Slave_MCU/App/slave_scheduler.c",
    "Slave_MCU/BSP/slave_board.c",
    "Slave_MCU/Drivers/Communication/master_link.c",
    "docs/RUNTIME_SCHEDULING.md",
    "tests/host/test_scheduler.c",
    "tests/host/test_slave_io.c",
    "tests/host/test_slave_app.c",
    "tests/host/test_inter_mcu_protocol.c",
    "tests/host/test_flight_safety.c",
)
REMOVED_DUPLICATE_DIRECTORIES = (
    "Master_MCU/User",
    "Master_MCU/Hardware",
    "Slave_MCU/User",
    "Slave_MCU/Hardware",
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

    build_files = root.findall(".//Files/File")
    seen_sources: set[Path] = set()
    for entry in build_files:
        raw_path = entry.findtext("FilePath") or ""
        target = resolve_project_path(project, raw_path)
        if target in seen_sources:
            errors.append(f"{project.name}: duplicate source entry: {raw_path}")
        seen_sources.add(target)
        if entry.findtext("FileName") != target.name:
            errors.append(f"{project.name}: filename/path mismatch: {raw_path}")

    options = project.with_suffix(".uvoptx")
    try:
        options_root = ET.parse(options).getroot()
        option_paths = sorted(
            (node.text or "").replace("\\", "/")
            for node in options_root.iter("PathWithFileName")
        )
        build_paths = sorted(
            (node.text or "").replace("\\", "/")
            for node in root.iter("FilePath")
        )
        if option_paths != build_paths:
            errors.append(f"{options.relative_to(REPOSITORY_ROOT)}: file list differs from build project")
    except (OSError, ET.ParseError) as error:
        errors.append(f"{options.relative_to(REPOSITORY_ROOT)}: {error}")

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

    for shared in (r"..\Shared\Drivers\dma_rx.c",
                   r"..\Shared\Drivers\millisecond_clock.c",
                   r"..\Shared\Scheduling\periodic_tasks.c"):
        if shared not in project_text:
            errors.append(f"{project.relative_to(REPOSITORY_ROOT)} does not compile {shared}")
    if project.parent.name == "Slave_MCU":
        for node in root.iter():
            if (node.tag in ("IROM", "OCR_RVCT4") and int(node.findtext("StartAddress") or "0", 0) == 0x08000000 and
                    int(node.findtext("Size") or "0", 0) > 0xFC00):
                errors.append("Slave IROM must reserve calibration flash at 0x0800FC00")
    return errors


def validate_markdown_links(document: Path) -> list[str]:
    errors: list[str] = []
    text = document.read_text(encoding="utf-8")

    for match in re.finditer(r"\[[^\]]+\]\(([^)]+)\)", text):
        raw_target = match.group(1).strip()
        if (
            not raw_target
            or raw_target.startswith("#")
            or "://" in raw_target
            or raw_target.startswith("mailto:")
        ):
            continue

        path_text = raw_target.split("#", 1)[0].strip()
        if path_text.startswith("<") and path_text.endswith(">"):
            path_text = path_text[1:-1]
        target = (document.parent / path_text).resolve()
        if not target.exists():
            errors.append(
                f"{document.relative_to(REPOSITORY_ROOT)} references "
                f"missing document: {raw_target}"
            )

    return errors


def validate_cmake_paths() -> list[str]:
    """Check literal repository references; CMake/CTest remains the build gate."""
    errors: list[str] = []
    for cmake in (REPOSITORY_ROOT / "CMakeLists.txt",
                  REPOSITORY_ROOT / "tests/host/CMakeLists.txt"):
        text = re.sub(r"#[^\n]*", "", cmake.read_text(encoding="utf-8"))
        for raw in re.findall(r"(?<![\w])(?:\.\./)+[A-Za-z0-9_./-]+", text):
            if not (cmake.parent / raw).exists():
                errors.append(f"{cmake.relative_to(REPOSITORY_ROOT)}: missing path: {raw}")
        for block in re.findall(r"add_executable\(\s*\w+\s+([^)]*)\)", text):
            for raw in block.split():
                if raw.endswith((".c", ".h")) and not (cmake.parent / raw).is_file():
                    errors.append(f"{cmake.relative_to(REPOSITORY_ROOT)}: missing source: {raw}")
    return errors


def main() -> int:
    errors: list[str] = []

    for relative_path in REQUIRED_FILES:
        if not (REPOSITORY_ROOT / relative_path).is_file():
            errors.append(f"required file is missing: {relative_path}")

    for relative_path in REMOVED_DUPLICATE_DIRECTORIES:
        if (REPOSITORY_ROOT / relative_path).exists():
            errors.append(f"obsolete or duplicate directory returned: {relative_path}")

    errors.extend(validate_cmake_paths())

    for project in PROJECTS:
        errors.extend(validate_keil_project(project))

    markdown_documents = sorted(REPOSITORY_ROOT.glob("*.md"))
    markdown_documents.extend(sorted((REPOSITORY_ROOT / "docs").rglob("*.md")))
    for document in markdown_documents:
        errors.extend(validate_markdown_links(document))

    if errors:
        print("Project validation failed:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print("Project validation passed:")
    print("  - required engineering files are present")
    print("  - obsolete and duplicate directories are absent")
    print("  - both Keil projects and option file lists agree")
    print("  - all Keil source and include references exist")
    print("  - both firmware targets compile the shared protocol")
    print("  - internal Markdown document links resolve")
    print("  - literal CMake source and include paths exist")
    return 0


if __name__ == "__main__":
    sys.exit(main())

