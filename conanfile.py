from conans import ConanFile, tools

class MainProject(ConanFile):
    python_requires = "conan_template/[>=5]@robotkernel/stable"
    python_requires_extend = "conan_template.RobotkernelConanFile"

    name = "bridge_rest"
    description = "robotkernel-5 service bridge via rest api"
    exports_sources = ["*", "!.gitignore"] + ["!%s" % x for x in tools.Git().excluded_files()]
    requires = "robotkernel/5.0.50@robotkernel/snapshot", "libhttpserver/0.18.2@3rdparty/unstable"
    #requires = "robotkernel/[~=5]@robotkernel/stable", "libhttpserver/0.18.2@3rdparty/unstable"

