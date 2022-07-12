from conans import tools, python_requires, AutoToolsBuildEnvironment

base = python_requires("conan_template/[~=5]@robotkernel/stable")

class MainProject(base.RobotkernelConanFile):
    name = "bridge_rest"
    description = "robotkernel-5 service bridge via rest api"
    exports_sources = ["*", "!.gitignore"] + ["!%s" % x for x in tools.Git().excluded_files()]
    requires = "robotkernel/[~=6.0]@robotkernel/unstable", "libhttpserver/0.18.2@3rdparty/unstable"

