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
    "Doxyfile",
    "docs/README.md",
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
    "docs/EMBEDDED_ENGINEERING_UPGRADE.md",
    "docs/CODING_STANDARD.md",
    "docs/STATIC_ANALYSIS.md",
    "docs/DOXYGEN.md",
    "docs/DOXYGEN_MAINPAGE.md",
    "docs/LEARNING_ROADMAP.md",
    "docs/learning/PHASE_7_ENGINEERING_QUALITY.md",
    "docs/HAL.md",
    "docs/HAL_DESIGN.md",
    "docs/learning/PHASE_2_HAL_ARCHITECTURE.md",
    "docs/FREERTOS_ARCHITECTURE.md",
    "docs/FREERTOS_MIGRATION_PLAN.md",
    "docs/learning/PHASE_6_FREERTOS_MIGRATION.md",
    "docs/PARAMETERS.md",
    "docs/LOGGING.md",
    "docs/FLIGHT_DATA_LOGGER.md",
    "docs/GROUND_STATION_PROTOCOL.md",
    "docs/learning/PHASE_3_FLIGHT_DATA_LOGGER.md",
    "docs/learning/PHASE_4_PARAMETER_DEBUG_SYSTEM.md",
    "docs/learning/PHASE_5_GROUND_STATION_INTERFACE.md",
    "Shared/Services/flight_data_logger.c",
    "Shared/Services/flight_data_logger.h",
    "Shared/Services/parameter_catalog.c",
    "Shared/Services/parameter_catalog.h",
    "Shared/Services/parameter_persistence.c",
    "Shared/Services/parameter_persistence.h",
    "Shared/Services/ground_station_service.c",
    "Shared/Services/ground_station_service.h",
    "docs/SYSTEM_RELIABILITY.md",
    "docs/learning/PHASE_1_SYSTEM_RELIABILITY.md",
    "tests/host/README.md",
    "tools/run_quality_gates.ps1",
    "Shared/Services/event_log.c",
    "Shared/Services/event_log.h",
    "Shared/Services/parameter_store.c",
    "Shared/Services/parameter_store.h",
    "Shared/Safety/watchdog_manager.c",
    "Shared/Safety/watchdog_manager.h",
    "Shared/Safety/fault_manager.c",
    "Shared/Safety/fault_manager.h",
    "Shared/Safety/failsafe_state_machine.c",
    "Shared/Safety/failsafe_state_machine.h",
    "Master_MCU/App/app_task_model.c",
    "Master_MCU/App/app_task_model.h",
    "Master_MCU/App/freertos_task_plan.c",
    "Master_MCU/App/freertos_task_plan.h",
    "Shared/Drivers/driver_status.c",
    "Shared/Drivers/driver_status.h",
    "Shared/Drivers/status_led.c",
    "Shared/Drivers/status_led.h",
    "Shared/HAL/hal_gpio.c",
    "Shared/HAL/hal_gpio.h",
    "Shared/HAL/hal_status.h",
    "Shared/HAL/hal_uart.c",
    "Shared/HAL/hal_uart.h",
    "Shared/HAL/hal_i2c.c",
    "Shared/HAL/hal_i2c.h",
    "Shared/HAL/hal_spi.c",
    "Shared/HAL/hal_spi.h",
    "Shared/HAL/hal_pwm.c",
    "Shared/HAL/hal_pwm.h",
    "Shared/HAL/hal_timer.c",
    "Shared/HAL/hal_timer.h",
    "Shared/Protocol/inter_mcu_protocol.c",
    "Shared/Protocol/inter_mcu_protocol.h",
    "Shared/Protocol/ground_station_protocol.c",
    "Shared/Protocol/ground_station_protocol.h",
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
SHARED_FORBIDDEN_TOKENS = (
    "stm32f10x",
    "STM32F10x",
    "Platform/STM32F1",
    "Platform\\STM32F1",
)
SHARED_FORBIDDEN_CALLS = re.compile(
    r"\b(?:malloc|calloc|realloc|free|printf|sprintf|vsprintf)\s*\("
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


def validate_host_test_registration() -> list[str]:
    errors: list[str] = []
    host_test_root = REPOSITORY_ROOT / "tests" / "host"
    cmake_file = host_test_root / "CMakeLists.txt"
    cmake_text = cmake_file.read_text(encoding="utf-8")

    for test_source in sorted(host_test_root.glob("test_*.c")):
        if test_source.name not in cmake_text:
            errors.append(
                f"host test is not registered in CMake: {test_source.name}"
            )

    return errors


def validate_shared_portability() -> list[str]:
    errors: list[str] = []
    shared_root = REPOSITORY_ROOT / "Shared"

    for source in sorted(shared_root.rglob("*")):
        if source.suffix.lower() not in {".c", ".h"}:
            continue
        text = source.read_text(encoding="utf-8", errors="replace")
        relative_source = source.relative_to(REPOSITORY_ROOT)
        for line_number, line in enumerate(text.splitlines(), start=1):
            for token in SHARED_FORBIDDEN_TOKENS:
                if token in line:
                    errors.append(
                        f"{relative_source}:{line_number} imports "
                        f"platform-specific token: {token}"
                    )
            match = SHARED_FORBIDDEN_CALLS.search(line)
            if match:
                errors.append(
                    f"{relative_source}:{line_number} uses forbidden "
                    f"runtime call in Shared: {match.group(0).rstrip('(')}"
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

    markdown_documents = sorted(REPOSITORY_ROOT.glob("*.md"))
    markdown_documents.extend(sorted((REPOSITORY_ROOT / "docs").rglob("*.md")))
    for document in markdown_documents:
        errors.extend(validate_markdown_links(document))
    errors.extend(validate_host_test_registration())
    errors.extend(validate_shared_portability())

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
    print("  - internal Markdown document links resolve")
    print("  - every host test source is registered in CMake")
    print("  - Shared remains platform-independent and allocation-free")
    return 0


if __name__ == "__main__":
    sys.exit(main())
