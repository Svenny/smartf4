# SmartF4

This plugin can handle header/source switching better than vanilla when there are many pairs of files with the same base name.

For example, assume you have the following files in your project:
```
src/system/one/foo.cpp
src/module/two/foo.cpp
include/system/one/foo.hpp
include/module/two/foo.hpp
```

Vanilla header/source switching can easily pair `src/system/one/foo.cpp` to `include/module/two/foo.hpp` (which is surely not intended).
This plugin implements a heuristic taking into account intermediate directories in the path which reduces chances of such misbehavior.

Each matching is cached so repeated file switching is instant regardless of project size.
Though if the plugin messes something up (and beleive me it does), this cache can be reset by a hotkey (Ctrl+Shift+F4 by default).

---

The rest of this readme and CI setup is copy-paste from Qt Creator plugin template project.

## How to Build

Create a build directory and run

    cmake -DCMAKE_PREFIX_PATH=<path_to_qtcreator> -DCMAKE_BUILD_TYPE=RelWithDebInfo <path_to_plugin_source>
    cmake --build .

where `<path_to_qtcreator>` is the relative or absolute path to a Qt Creator build directory, or to a
combined binary and development package (Windows / Linux), or to the `Qt Creator.app/Contents/Resources/`
directory of a combined binary and development package (macOS), and `<path_to_plugin_source>` is the
relative or absolute path to this plugin directory.

## How to Run

Run a compatible Qt Creator with the additional command line argument

    -pluginpath <path_to_plugin>

where `<path_to_plugin>` is the path to the resulting plugin library in the build directory
(`<plugin_build>/lib/qtcreator/plugins` on Windows and Linux,
`<plugin_build>/Qt Creator.app/Contents/PlugIns` on macOS).

You might want to add `-temporarycleansettings` (or `-tcs`) to ensure that the opened Qt Creator
instance cannot mess with your user-global Qt Creator settings.

When building and running the plugin from Qt Creator, you can use

    -pluginpath "%{buildDir}/lib/qtcreator/plugins" -tcs

on Windows and Linux, or

    -pluginpath "%{buildDir}/Qt Creator.app/Contents/PlugIns" -tcs

for the `Command line arguments` field in the run settings.
