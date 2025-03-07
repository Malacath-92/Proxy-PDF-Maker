from conan import ConanFile


class ProxyPDF(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    def requirements(self):
        self.requires("fmt/9.1.0")
        self.requires("qt/6.7.3")
        self.requires("libvips/8.14.2")
        self.requires("libharu/2.4.4")
        self.requires("pdf-writer/4.6.7")
        self.requires("nlohmann_json/3.11.3")
        self.requires("catch2/3.7.1")

    def configure(self):
        self.options["qt"].shared = False
        self.options["qt"].opengl = "no"
        self.options["qt"].openssl = False
        self.options["qt"].with_md4c = False
        self.options["qt"].with_odbc = False
        self.options["qt"].with_mysql = False
        self.options["qt"].with_sqlite3 = False

        self.options["libvips"].shared = False

        self.options["pdf-writer"].shared = False
