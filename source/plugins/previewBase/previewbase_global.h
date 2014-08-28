#ifndef PREVIEWBASE_GLOBAL_H
#define PREVIEWBASE_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(PREVIEWBASE_LIBRARY)
#  define PREVIEWBASESHARED_EXPORT Q_DECL_EXPORT
#else
#  define PREVIEWBASESHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // PREVIEWBASE_GLOBAL_H
