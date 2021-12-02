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
};

extern bool IsArgFlag(int arg);

extern void ManageArgs(int argc, char** argv);

}

#endif