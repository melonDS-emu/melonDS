#ifndef PLATFORMOGL_H
#define PLATFORMOGL_H

//! You can do the following if you want to use a different OpenGL function loader:
///
/// 1. Define \c MELONDS_NO_GLAD and \c MELONDS_PLATFORMOGL_PRIVATE in your project's build.
/// 2. Create a header named \c PlatformOGLPrivate.h in your project.
/// 3. Add its location to your project's \c #include search path.
/// 4. Declare whatever symbols you need in PlatformOGLPrivate.h.

#ifndef MELONDS_NO_GLAD
#include "frontend/glad/glad.h"
#endif

#ifdef MELONDS_PLATFORMOGL_PRIVATE
#include "PlatformOGLPrivate.h"
#endif

#endif
