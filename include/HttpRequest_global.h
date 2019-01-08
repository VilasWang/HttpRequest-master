#ifndef HTTP_REQUEST_GLOBAL_H
#define HTTP_REQUEST_GLOBAL_H

#ifndef HTTP_REQUEST_STATIC

#ifdef HTTP_REQUEST_LIB
# define HTTP_REQUEST_EXPORT __declspec(dllexport)
#else
# define HTTP_REQUEST_EXPORT __declspec(dllimport)
#endif

#else

# define HTTP_REQUEST_EXPORT

#endif

#endif // HTTP_REQUEST_GLOBAL_H
