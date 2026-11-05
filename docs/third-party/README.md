# Distribution credits

| Dependency | Purpose | Verified installed version | License / provenance |
|---|---|---|---|
| GLFW | Window, context, input | 3.5.1 Homebrew package (Mach-O current 3.5.0) | zlib/libpng-style, [full notice](GLFW-LICENSE.md); copied from installed package LICENSE.md |
| libpng | PNG decoding | 1.6.59 Homebrew package | PNG Reference Library License version 2, [full notice](libpng-LICENSE.txt); copied from installed package LICENSE |
| OpenGL | Graphics API | Apple system; requests 3.3 Core | System framework, not redistributed |
| zlib | libpng compression dependency | Apple system libz.1, Mach-O current 1.2.12 | System library, not redistributed |
| Procedural textures | Developer examples | M3/M4 original assets | Existing CC0 notices in Resources/assets/textures |

GLFW and libpng are redistributed unmodified except Mach-O load-command relocation
and ad-hoc signatures. Their copyright notices are retained. System Cocoa, IOKit,
CoreFoundation, CoreVideo, Foundation, OpenGL, libobjc, libc++ and libSystem remain
system references. Exact transitive load commands are recorded in ../data/m63c/package.txt.
The package build follows installed libraries, and verification fails on unresolved
or external references; review licenses again if future dependencies change.
No repository-wide license is granted by these dependency notices.
