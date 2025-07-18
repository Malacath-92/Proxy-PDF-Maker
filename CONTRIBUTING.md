# Contributing to Proxy-PDF-Maker

First off, thanks for taking the time to contribute! 

All types of contributions are encouraged and valued. See the [Table of Contents](#table-of-contents) for different ways to help and details about how this project handles them. Please make sure to read the relevant section before making your contribution. It will make it a lot easier for us maintainers and smooth out the experience for all involved. The community looks forward to your contributions. 

And if you like the project, but just don't have time to contribute, that's fine. There are other easy ways to support the project and show your appreciation, which we would also be very happy about:
- Star the project
- Join the _Magic Proxies_ Discord server and chat with us
- Refer this project in your project's README
- Mention the project in your community and tell your friends about it


## Reporting an Issue

If your program crashes or otherwise behaves weirdly we would be very happy to receive a bug report for it. Before you report an issue though please ensure the following:
- You are running the latest version of the program. You can check the version you run by pressing F1.
- The bug is not already present in the [issues page](https://github.com/Malacath-92/Proxy-PDF-Maker/issues).
- Check the _Magic Proxies_ Discord server to be sure this is not a known issue in the community.

Once you are sure it makes sense to report the bug please do the following:
- Collect information about the bug:
    - Your operating system,
    - zip up the images you are working with,
    - copy that zip and the `proj.json` (or other if you renamed it) into a folder.

To report a bug visit the [new issue page](https://github.com/Malacath-92/Proxy-PDF-Maker/issues/new), describe your issue and make sure to include:
- Your operating system,
- the app version,
- the images and project files you collected earlier,
- exact steps to reproduce the issue.

## Feature Request

If you would like to see a new feature in the app we are happy to hear it. Just ensure the following:
- You are running the latest version of the program. You can check the version you run by pressing F1.
- The feature is not already requested in the [issues page](https://github.com/Malacath-92/Proxy-PDF-Maker/issues).
- Check the _Magic Proxies_ Discord server to be sure this is not a feature that may be hidden or hard to find.

To request a feature visit the [new issue page](https://github.com/Malacath-92/Proxy-PDF-Maker/issues/new), describe your feature and explain how it would be used and who the target audience would be.

## Contributing with Code

Code contributions are very welcome, to help you make them here is some information about the project:
- We work with C++23, not all features are available on all platforms however.
- Dependencies are managed with Conan.
- The build is managed with CMake.
- The following platforms are supported and tested:

|           | Windows            | Ubuntu-ARM64             | Ubuntu-x86 | MacOS-ARM64              | MacOS-x86                |
|---        |---                 |---                       |---         |---                       |---                       |
| Compiler  | Visual Studio 2022 | gcc-14                   | gcc-14     | Xcode 16.2               | Xcode 16.2               |
| Status    | App Tested         | Tests Pass, App Untested | App Tested | Tests Pass, App Untested | Tests Pass, App Untested |
|           | [![Windows](https://github.com/Malacath-92/Proxy-PDF-Maker/actions/workflows/Windows-CI.yml/badge.svg)](https://github.com/Malacath-92/Proxy-PDF-Maker/actions/workflows/Windows-CI.yml)|[![Ubuntu-ARM64](https://github.com/Malacath-92/Proxy-PDF-Maker/actions/workflows/Ubuntu-ARM64-CI.yml/badge.svg)](https://github.com/Malacath-92/Proxy-PDF-Maker/actions/workflows/Ubuntu-ARM64-CI.yml)|[![Ubuntu-x86](https://github.com/Malacath-92/Proxy-PDF-Maker/actions/workflows/Ubuntu-x86-CI.yml/badge.svg)](https://github.com/Malacath-92/Proxy-PDF-Maker/actions/workflows/Ubuntu-x86-CI.yml)|[![MacOS - Arm64](https://github.com/Malacath-92/Proxy-PDF-Maker/actions/workflows/MacOS-ARM64-CI.yml/badge.svg)](https://github.com/Malacath-92/Proxy-PDF-Maker/actions/workflows/MacOS-ARM64-CI.yml)|[![MacOS - x86](https://github.com/Malacath-92/Proxy-PDF-Maker/actions/workflows/MacOS-x86-CI.yml/badge.svg)](https://github.com/Malacath-92/Proxy-PDF-Maker/actions/workflows/MacOS-x86-CI.yml)

### Required Software

To build the project locally you require the following software:
- Git
- A recent CMake
- Conan 2
- A functioning C++ Compiler, see tested versions above

### Cloning the Repo

Open a command prompt wherever you want the project to be cloned to. Then run the following command:

```sh
git clone https://github.com/Malacath-92/Proxy-PDF-Maker.git
cd Proxy-PDF-Maker
git submodule update --init --recursive
```

### Building

Open a command prompt in the root of the project. Then run the following commands to configure your projects:

```sh
mkdir build
cd build
cmake .. -DPPP_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
```

If you configured e.g. with Visual Studio you can now work in the generated solution. Otherwise you can build via:

```sh
cd build
cmake --build .
```

### Submitting a Pull-Request

When you submit a Pull-Request Github will kick off jobs to test your code. Please make sure they all pass before expecting a maintainer to look closely at your PR. Also make sure that you format your code at least once, you can do this via the following command:

```sh
cmake --build . --target=format_proxy_pdf
```
