
#!/usr/bin/env python3

import argparse
import ctypes
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


TEST_MAIN = r'''#include <Windows.h>
#include <iostream>

#include "SDK/Engine_classes.hpp"

// Basic.cpp was added to the VS project
// Engine_functions.cpp was added to the VS project

DWORD MainThread(HMODULE Module)
{
    /* Code to open a console window */
    AllocConsole();
    FILE* Dummy;
    freopen_s(&Dummy, "CONOUT$", "w", stdout);
    freopen_s(&Dummy, "CONIN$", "r", stdin);

    /* Functions returning "static" instances */
    SDK::UEngine* Engine = SDK::UEngine::GetEngine();
    SDK::UWorld* World = SDK::UWorld::GetWorld();

    /* Getting the PlayerController, World, OwningGameInstance, ... should all be checked not to be nullptr! */
    SDK::APlayerController* MyController = World->OwningGameInstance->LocalPlayers[0]->PlayerController;

    /* Print the full-name of an object ("ClassName PackageName.OptionalOuter.ObjectName") */
    std::cout << Engine->ConsoleClass->GetFullName() << std::endl;

    /* Manually iterating GObjects and printing the FullName of every UObject that is a Pawn (not recommended) */
    for (int i = 0; i < SDK::UObject::GObjects->Num(); i++)
    {
        SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);

        if (!Obj)
            continue;

        if (Obj->IsDefaultObject())
            continue;

        /* Only the 'IsA' check using the cast flags is required, the other 'IsA' is redundant */
        if (Obj->IsA(SDK::APawn::StaticClass()) || Obj->HasTypeFlag(SDK::EClassCastFlags::Pawn))
        {
            std::cout << Obj->GetFullName() << "\n";
        }
    }

    /* You might need to loop all levels in UWorld::Levels */
    SDK::ULevel* Level = World->PersistentLevel;
    SDK::TArray<SDK::AActor*>& Actors = Level->Actors;

    for (SDK::AActor* Actor : Actors)
    {
        /* The 2nd and 3rd checks are equal, prefer using EClassCastFlags if available for your class. */
        if (!Actor || !Actor->IsA(SDK::EClassCastFlags::Pawn) || !Actor->IsA(SDK::APawn::StaticClass()))
            continue;

        SDK::APawn* Pawn = static_cast<SDK::APawn*>(Actor);
        // Use Pawn here
    }

    /*
    * Changes the keyboard-key that's used to open the UE console
    *
    * This is a rare case of a DefaultObjects' member-variables being changed.
    * By default you do not want to use the DefaultObject, this is a rare exception.
    */
    SDK::UInputSettings::GetDefaultObj()->ConsoleKeys[0].KeyName = SDK::UKismetStringLibrary::Conv_StringToName(L"F2");

    /* Creates a new UObject of class-type specified by Engine->ConsoleClass */
    SDK::UObject* NewObject = SDK::UGameplayStatics::SpawnObject(Engine->ConsoleClass, Engine->GameViewport);

    /* The Object we created is a subclass of UConsole, so this cast is **safe**. */
    Engine->GameViewport->ViewportConsole = static_cast<SDK::UConsole*>(NewObject);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
        break;
    }

    return TRUE;
}
'''


CMAKE_LISTS = r'''cmake_minimum_required(VERSION 3.21)
project(SDKBuildTest LANGUAGES CXX)

if(NOT DEFINED SDK_ROOT)
    message(FATAL_ERROR "SDK_ROOT must point to the generated SDK directory")
endif()

file(TO_CMAKE_PATH "${SDK_ROOT}" SDK_ROOT)

foreach(SDK_SOURCE
    "${SDK_ROOT}/SDK/Basic.cpp"
    "${SDK_ROOT}/SDK/Engine_functions.cpp"
    "${SDK_ROOT}/SDK/CoreUObject_functions.cpp"
)
    if(NOT EXISTS "${SDK_SOURCE}")
        message(FATAL_ERROR "Required source file not found: ${SDK_SOURCE}")
    endif()
endforeach()

add_library(SDKBuildTest SHARED
    TestMain.cpp
    "${SDK_ROOT}/SDK/Basic.cpp"
    "${SDK_ROOT}/SDK/Engine_functions.cpp"
    "${SDK_ROOT}/SDK/CoreUObject_functions.cpp"
)

target_include_directories(SDKBuildTest PRIVATE "${SDK_ROOT}")
target_compile_features(SDKBuildTest PRIVATE cxx_std_23)
set_target_properties(SDKBuildTest PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(SDKBuildTest PRIVATE
        -Wno-invalid-offsetof
        -Wno-c++11-narrowing
    )
endif()
'''


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compile a generated SDK DLL test with MSVC and ClangCL."
    )
    parser.add_argument(
        "--ClearOutput",
        "--clear-output",
        action="store_true",
        dest="clear_output",
        help="Remove SDKTest after both builds succeed.",
    )
    parser.add_argument(
        "--SelfDelete",
        "--self-delete",
        action="store_true",
        dest="self_delete",
        help="Delete this script after both builds succeed.",
    )
    return parser.parse_args()


def wait_on_failure(message: str) -> int:
    if os.name == "nt":
        try:
            ctypes.windll.kernel32.AllocConsole()
            ctypes.windll.kernel32.SetConsoleTitleW("SDK Build Test Failed")
            sys.stdin = open("CONIN$", "r", encoding="utf-8")
            sys.stdout = open("CONOUT$", "w", encoding="utf-8", buffering=1)
            sys.stderr = open("CONOUT$", "w", encoding="utf-8", buffering=1)
        except OSError:
            pass

    print("\nSDK build test failed.", file=sys.stderr)
    print(message, file=sys.stderr)
    try:
        input("\nPress Enter to close...")
    except (EOFError, OSError):
        pass
    return 1


def run_logged(command: list[str], log_file: Path, cwd: Path) -> bool:
    printable = subprocess.list2cmdline(command)
    print(f"Running: {printable}")

    with log_file.open("a", encoding="utf-8", errors="replace") as log:
        log.write(f"\n> {printable}\n")
        log.flush()
        try:
            result = subprocess.run(
                command,
                cwd=cwd,
                stdout=log,
                stderr=subprocess.STDOUT,
                text=True,
                check=False,
            )
        except OSError as error:
            log.write(f"\nFailed to start command: {error}\n")
            return False

        log.write(f"\nExit code: {result.returncode}\n")
        return result.returncode == 0


def build_toolchain(
    name: str,
    source_dir: Path,
    build_dir: Path,
    output_dir: Path,
    log_file: Path,
    sdk_root: Path,
    generator: str,
    generator_instance: str,
    toolset: str | None = None,
) -> bool:
    if build_dir.exists():
        shutil.rmtree(build_dir)
    build_dir.mkdir(parents=True)
    output_dir.mkdir(parents=True, exist_ok=True)
    log_file.write_text(f"{name} build log\n", encoding="utf-8")

    configure = [
        "cmake",
        "-S",
        str(source_dir),
        "-B",
        str(build_dir),
        "-G",
        generator,
        "-A",
        "x64",
        f"-DSDK_ROOT={sdk_root}",
        f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY={output_dir}",
        f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={output_dir}",
    ]
    if toolset:
        configure.extend(["-T", toolset])
    if generator_instance:
        configure.append(f"-DCMAKE_GENERATOR_INSTANCE={generator_instance}")

    if not run_logged(configure, log_file, source_dir):
        return False

    build = [
        "cmake",
        "--build",
        str(build_dir),
        "--config",
        "Release",
        "--target",
        "SDKBuildTest",
    ]
    return run_logged(build, log_file, source_dir)


def find_vs_toolchain(generators: set[str], toolset: str | None = None) -> tuple[str, str]:
    """Select a matching CMake generator and VS instance with the required tools."""
    program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        raise RuntimeError(f"Visual Studio discovery tool not found: {vswhere}")

    required_components = ["Microsoft.VisualStudio.Component.VC.Tools.x86.x64"]
    if toolset == "ClangCL":
        required_components.append("Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset")

    # Query each major version separately so the generator always matches the instance.
    # MSVC and ClangCL may use different installs when only one has the Clang tools.
    for generator, version_range in (
        ("Visual Studio 18 2026", "[18.0,19.0)"),
        ("Visual Studio 17 2022", "[17.0,18.0)"),
    ):
        if generator not in generators:
            continue
        result = subprocess.run(
            [
                str(vswhere),
                "-latest",
                "-products", "*",
                "-version", version_range,
                "-requires", *required_components,
                "-utf8",
                "-property", "installationPath",
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            check=True,
        )

        found = [line.strip() for line in result.stdout.splitlines() if line.strip()]
        if found:
            return generator, found[0]

    raise RuntimeError(
        f"No compatible Visual Studio 2022 or 2026 installation found for {toolset or 'MSVC'}. "
        "Install the Desktop development with C++ tools, and for ClangCL also the "
        "'C++ Clang Compiler for Windows' + 'MSBuild support for LLVM (clang-cl) toolset' components. "
        "CMake must support the installed Visual Studio version (3.21+ for VS 2022; 4.2+ for VS 2026)."
    )


def main() -> int:
    args = parse_args()
    sdk_root = Path(__file__).resolve().parent
    test_root = sdk_root / "SDKTest"
    logs_dir = test_root / "Logs"
    output_dir = test_root / "Output"

    required_sources = [
        sdk_root / "SDK" / "Basic.cpp",
        sdk_root / "SDK" / "Engine_functions.cpp",
        sdk_root / "SDK" / "CoreUObject_functions.cpp",
    ]
    missing = [path for path in required_sources if not path.is_file()]
    if missing:
        missing_list = "\n".join(f"Missing required source: {path}" for path in missing)
        return wait_on_failure(missing_list)

    if shutil.which("cmake") is None:
        return wait_on_failure("cmake was not found on PATH.")

    try:
        capabilities = subprocess.run(
            ["cmake", "-E", "capabilities"], capture_output=True, text=True, check=True
        )
        generators = {entry["name"] for entry in json.loads(capabilities.stdout)["generators"]}
        msvc_generator, msvc_instance = find_vs_toolchain(generators)
        clang_generator, clang_instance = find_vs_toolchain(generators, "ClangCL")
    except (OSError, subprocess.CalledProcessError, ValueError, KeyError, RuntimeError) as error:
        return wait_on_failure(f"Could not select Visual Studio toolchains: {error}")

    logs_dir.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)
    (test_root / "TestMain.cpp").write_text(TEST_MAIN, encoding="utf-8", newline="\n")
    (test_root / "CMakeLists.txt").write_text(CMAKE_LISTS, encoding="utf-8", newline="\n")

    msvc_ok = build_toolchain(
        name="MSVC",
        source_dir=test_root,
        build_dir=test_root / "Build-MSVC",
        output_dir=output_dir / "MSVC",
        log_file=logs_dir / "MSVC.log",
        sdk_root=sdk_root,
        generator=msvc_generator,
        generator_instance=msvc_instance,
    )

    clang_ok = build_toolchain(
        name="ClangCL",
        source_dir=test_root,
        build_dir=test_root / "Build-Clang",
        output_dir=output_dir / "Clang",
        log_file=logs_dir / "Clang.log",
        sdk_root=sdk_root,
        generator=clang_generator,
        toolset="ClangCL",
        generator_instance=clang_instance,
    )

    print(f"MSVC:   {'succeeded' if msvc_ok else 'failed'}")
    print(f"Clang:  {'succeeded' if clang_ok else 'failed'}")

    if not (msvc_ok and clang_ok):
        return wait_on_failure(f"Build logs are in: {logs_dir}")

    if args.clear_output:
        shutil.rmtree(test_root)
        print(f"Both builds succeeded; removed {test_root}")
    else:
        print(f"DLL output is in: {output_dir}")
        print(f"Build logs are in: {logs_dir}")

    if args.self_delete:
        script_path = Path(__file__).resolve()
        try:
            script_path.unlink()
        except OSError as error:
            return wait_on_failure(f"Could not delete script {script_path}: {error}")
        print(f"Both builds succeeded; deleted {script_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

