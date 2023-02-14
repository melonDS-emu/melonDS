#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#define Log_SetChannel(ChannelName)
#define Log_ErrorPrint(msg) puts(msg "\n");
#define Log_ErrorPrintf(...) do { printf(__VA_ARGS__); putchar('\n'); } while (0)
#define Log_WarningPrint(msg) puts(msg)
#define Log_WarningPrintf(...) do { printf(__VA_ARGS__); putchar('\n'); } while (0)
#define Log_PerfPrint(msg) puts(msg)
#define Log_PerfPrintf(...) do { printf(__VA_ARGS__); putchar('\n'); } while (0)
#define Log_InfoPrint(msg) puts(msg)
#define Log_InfoPrintf(...) do { printf(__VA_ARGS__); putchar('\n'); } while (0)
#define Log_VerbosePrint(msg) puts(msg)
#define Log_VerbosePrintf(...) do { printf(__VA_ARGS__); putchar('\n'); } while (0)
#define Log_DevPrint(msg) puts(msg)
#define Log_DevPrintf(...) do { printf(__VA_ARGS__); putchar('\n'); } while (0)
#define Log_ProfilePrint(msg) puts(msg)
#define Log_ProfilePrintf(...) do { printf(__VA_ARGS__); putchar('\n'); } while (0)

#ifdef _DEBUG
#define Log_DebugPrint(msg) puts(msg)
#define Log_DebugPrintf(...) do { printf(__VA_ARGS__); putchar('\n'); } while (0)
#define Log_TracePrint(msg) puts(msg)
#define Log_TracePrintf(...) do { printf(__VA_ARGS__); putchar('\n'); } while (0)
#else
#define Log_DebugPrint(msg)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
  } while (0)
#define Log_DebugPrintf(...)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
  } while (0)
#define Log_TracePrint(msg)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
  } while (0)
#define Log_TracePrintf(...)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
  } while (0)
#endif

#endif