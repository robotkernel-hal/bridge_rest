from conan import ConanFile

class MainProject(ConanFile):
    python_requires = "conan_template/[>=5]@robotkernel/stable"
    python_requires_extend = "conan_template.RobotkernelConanFile"

    name = "bridge_rest"
    description = "robotkernel service bridge via rest api"
    exports_sources = ["*", "!.gitignore"]

    def requirements(self):
        self.requires("robotkernel/[~6]@robotkernel/unstable")
        self.requires("libhttpserver/0.18.2@3rdparty/stable")

