#ifndef CLI_H
#define CLI_H

namespace CLI {

extern char* DSRomPath;
extern char* GBARomPath;

extern char* GetNextArg(int argc, char** argv, int argp);

extern void ManageArgs(int argc, char** argv);

}

#endif