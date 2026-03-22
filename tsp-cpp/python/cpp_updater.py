import os
import subprocess
import shutil

def get_all_source_files():
    source_extensions = ('.cpp', '.c', '.h', '.hpp')
    cmake_extensions = ('.cmake', 'CMakeLists.txt')
    source_dirs = ('src', 'include')
    all_files = ['CMakeLists.txt']

    for source_dir in source_dirs:
        for root, dirs, files in os.walk(source_dir):
            for file in files:
                if file.endswith(source_extensions):
                    all_files.append(os.path.join(root, file))

    for root, dirs, files in os.walk("."):
        for file in files:
            if file.endswith(cmake_extensions):
                all_files.append(os.path.join(root, file))

    return list(all_files)

def need_rebuild(exe_path=os.path.join("build", "src", "tsp")):
    if not os.path.exists(exe_path):
        return True

    exe_mtime = os.path.getmtime(exe_path)

    for source_file in get_all_source_files():
        if os.path.getmtime(source_file) > exe_mtime:
            return True

    return False

def compile_project(build_dir="build"):
    print("Compiling project...")
    os.makedirs(build_dir, exist_ok=True)

    config_cmd = ["cmake", "-S", ".", "-B", build_dir, "-DCMAKE_BUILD_TYPE=Release"]

    result = subprocess.run(config_cmd, capture_output=True, text=True)

    if result.returncode != 0:
        if os.path.exists('build'):
            shutil.rmtree('build')
        result = subprocess.run(config_cmd + ["-G", "MinGW Makefiles"], capture_output=True, text=True)

    if result.returncode != 0:
        print(f"CMake failed: {result.stderr}")
        return False

    result = subprocess.run(
        ["cmake", "--build", build_dir],
        capture_output=True, text=True
    )

    if result.returncode != 0:
        print(f"Build failed: {result.stderr}")
        return False

    print("Compiling complete")
    return True

def recompiles_if_necessary(build_dir="build", exe_path=os.path.join("build", "src", "tsp")):
    if need_rebuild(exe_path):
        compile_project(build_dir)
