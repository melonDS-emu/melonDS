#ifndef CLI_H
#define CLI_H

namespace CLI {

enum
{
    cliArg_Help,
    cliArg_GBARomPath,
    cliArg_UseFreeBios,
    cliArg_FirmwareLanguage,
    cliArg_Renderer,
    cliArg_DSiMode,
};

extern char* GetNextArg(int argc, char** argv, int argp);

extern bool ManageArgs(int argc, char** argv);

}

#endif