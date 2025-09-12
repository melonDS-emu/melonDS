#include "RawInputThread.h"
#include <QPair>

void sample_on_rel(void* tag, Raw_Axis axis, int delta, RawInputThread* rawInputThread) {
	if (rawInputThread == nullptr) return;
	rawInputThread->internalReceiveDelta(axis, delta);
}

void sample_on_plug(int idx, void* _) {
    int *next_tag = (int*)_;
    int tag = *next_tag;
    *next_tag = tag + 1;

    raw_open(idx, (void*)tag);
    printf("Device %d at idx %d plugged.\n", (int)tag, idx);
}

void sample_on_unplug(void* tag, void* _) {
    raw_close(tag);
    printf("Device %d unplugged.\n", (intptr_t)tag);
}

RawInputThread::RawInputThread(QObject* parent): QThread(parent)
{
	raw_init();

	int dev_cnt = raw_dev_cnt();
	printf("Detected %d devices.\n", dev_cnt);

	int next_tag = 0;
	for (int i=0; i<dev_cnt; ++i) {
		raw_open(i, (void*)(next_tag++));
	}

	raw_on_rel((Raw_On_Rel)sample_on_rel, this);
	raw_on_unplug(sample_on_unplug, 0);
    raw_on_plug(sample_on_plug, &next_tag);
}

RawInputThread::~RawInputThread() {
	raw_quit();
}

void RawInputThread::internalReceiveDelta(Raw_Axis axis, int delta) {
	mouseDeltaLock.lock();
	if (axis == Raw_Axis::RA_X) mouseDeltaX += delta;
	else if (axis == Raw_Axis::RA_Y) mouseDeltaY += delta;
	mouseDeltaLock.unlock();
}

QPair<int ,int> RawInputThread::fetchMouseDelta() {
	mouseDeltaLock.lock();
	QPair<int, int> value = QPair(mouseDeltaX, mouseDeltaY);
	mouseDeltaX = 0;
	mouseDeltaY = 0;
	mouseDeltaLock.unlock();
	return value;
}

void RawInputThread::run()
{
	while (isRunning()) {
		raw_poll();
	}
}