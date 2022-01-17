from os import mkdir, chmod, write
from sys import stderr
from pathlib import Path
from shutil import copytree
from subprocess import Popen, PIPE
from tempfile import TemporaryDirectory, NamedTemporaryFile

def generate_c_application(idl_file, module, types, write_executable=None) -> NamedTemporaryFile:
    source_dir = Path(__file__).resolve().parent / "c_app"

    with TemporaryDirectory() as t_dir:
        #generate files step

        t_dir = Path(t_dir)
        copytree(source_dir, t_dir/ "c_app")
        build_dir = t_dir / "build"
        mkdir(build_dir)

        with open(t_dir / "c_app" / "xtypes_dynamic_types.idl", "w") as f:
            f.write(idl_file)

        textfile = """
#include "switch.hpp"
#include "xtypes_dynamic_types.hpp"
#include "validate.hpp"

bool validation::validate(
    dds::domain::DomainParticipant &participant,
    dds::sub::Subscriber &subscriber,
    unsigned long n_samples,
    const std::string &type_name) {
"""
        for _type in types:
            textfile += f"""
    if (type_name == "{_type}")
    return validate_impl<{module}::{_type}>(participant,subscriber,n_samples,type_name);
    """

        textfile += """
    return false;
}
"""


        with open(t_dir / "c_app" / "switch.cpp", "w") as f:
            f.write(textfile)

        # CMake generate step

        cmake = Popen(
            ["cmake", "../c_app", "-DIDL_FILE=xtypes_dynamic_types.idl", "-DCMAKE_BUILD_TYPE=DEBUG"], #include dir for cyclonedds-cxx ?
            cwd=build_dir, stdout=PIPE
        )
        out, err = cmake.communicate()

        if cmake.returncode != 0:
            print(out.decode(), file=stderr)
            print(err.decode(), file=stderr)
            raise Exception("Could not generate C application with CMake.")

        # CMake build step

        cmake = Popen(
            ["cmake", "--build", ".", "--target", "xtypes_sub"],
            cwd=build_dir, stdout=PIPE
        )
        out, err = cmake.communicate()

        if cmake.returncode != 0:
            print(out.decode(), file=stderr)
            print(err.decode(), file=stderr)
            raise Exception("Could not build C application with CMake.")

        executable = NamedTemporaryFile('w+b', delete=False)

        with open(build_dir / 'xtypes_sub', 'rb') as f:
            executable.write(f.read())
            executable.close()

        if write_executable is not None:
            with open(write_executable, "wb") as e:
                with open(build_dir / 'xtypes_sub', 'rb') as f:
                    e.write(f.read())

        chmod(executable.name, 0o700)
        return executable, textfile


def run_cpp_app(executable, type, num_samples):
    return Popen([executable.name, type, str(num_samples)], stdout=PIPE)
