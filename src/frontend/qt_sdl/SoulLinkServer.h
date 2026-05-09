/*
    Soul Link DS — Milestone 2: hardcoded read endpoint
    Listens on localhost TCP. When a client connects, returns hex bytes
    at a hardcoded RAM address, then closes the connection.
    Verify with: nc 127.0.0.1 8765
*/

#pragma once

#include <QObject>
#include <QTcpServer>

namespace melonDS { class NDS; }
class EmuInstance;

class SoulLinkServer : public QObject
{
    Q_OBJECT
public:
    explicit SoulLinkServer(EmuInstance* emu, QObject* parent = nullptr);
    ~SoulLinkServer();

    void start(quint16 port = 8765);

private slots:
    void onNewConnection();

private:
    QTcpServer* server;
    EmuInstance* emuInstance;
};
