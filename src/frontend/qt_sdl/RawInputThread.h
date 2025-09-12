#ifndef RAWINPUTTHREAD_H
#define RAWINPUTTHREAD_H

#include <QThread>
#include <QMutex>
#include <QPair>

extern "C"
{
#include "rawinput/rawinput.h"
}

class RawInputThread : public QThread
{
    Q_OBJECT

	void run() override;

public:
	explicit RawInputThread(QObject* parent);
	~RawInputThread();

	void internalReceiveDelta(Raw_Axis axis, int delta);

	QPair<int, int> fetchMouseDelta();

private:
	QMutex mouseDeltaLock;

	int mouseDeltaX;
	int mouseDeltaY;
};

#endif