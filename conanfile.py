from conan import ConanFile


class ProxyPDF(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    def requirements(self):
        # GUI and App Framework
        self.requires("qt/6.8.3")

        # File System Watcher
        self.requires("efsw/1.4.1")

        # Image Procession
        self.requires("opencv/4.11.0")

        # JSON Write/Read
        self.requires("nlohmann_json/3.12.0")

        # PDF Write/Read
        self.requires("podofo/1.0.3")

        # String Formatting
        self.requires("fmt/12.1.0")

        # Enum Reflection
        self.requires("magic_enum/0.9.7")

        # Unit Testing
        self.requires("catch2/3.7.1")

        # Conflict Resolution
        self.requires("zstd/1.5.7", override=True)
        self.requires("openjpeg/2.5.2", override=True)
        self.requires("icu/74.2", override=True)
        self.requires("libjpeg/9f", override=True)

    def configure(self):
        self.options["qt"].shared = False
        self.options["qt"].qtsvg = True
        self.options["qt"].opengl = "no"
        self.options["qt"].openssl = True
        self.options["qt"].with_md4c = False
        self.options["qt"].with_odbc = False
        self.options["qt"].with_mysql = False
        self.options["qt"].with_sqlite3 = False
        self.options["qt"].with_pq = False

        self.options["openssl"].shared = False

        self.options["opencv"].img_hash = True

        self.options["opencv"].shared = False
        self.options["opencv"].ml = False
        self.options["opencv"].dnn = False
        self.options["opencv"].flann = False
        self.options["opencv"].video = False
        self.options["opencv"].calib3d = False
        self.options["opencv"].videoio = False
        self.options["opencv"].objdetect = False
        self.options["opencv"].stitching = False
        self.options["opencv"].with_tiff = False
        self.options["opencv"].with_ffmpeg = False
        self.options["opencv"].with_openexr = False
        self.options["opencv"].with_wayland = False
        self.options["opencv"].with_imgcodec_hdr = False
        self.options["opencv"].with_imgcodec_pfm = False
        self.options["opencv"].with_imgcodec_pxm = False
        self.options["opencv"].with_imgcodec_sunraster = False
