#define DLL extern "C" __declspec(dllexport)

// Because Platform.cpp calls main.cpp's Stop method. I doubt we will ever need to do anything here.
void Stop(bool internal) {};

char* EmuDirectory;

DLL int test()
{
	return 1;
}